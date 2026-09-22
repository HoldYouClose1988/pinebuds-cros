/***************************************************************************
 * Stage B: poor-side FF mic → TWS → good-side speaker (experimental CROS).
 ***************************************************************************/
#include "cros_tws.h"

#include "app_audio.h"
#include "app_ibrt_customif_cmd.h"
#include "app_tws_if.h"
#include "app_utils.h"
#include "audioflinger.h"
#include "cmsis_os.h"
#include "hal_trace.h"
#include "string.h"
#include "tgt_hardware.h"

#ifdef ANC_APP
#include "app_anc.h"
#endif

/* Avoid pulling the full IBRT header graph into this TU. */
extern int tws_ctrl_send_cmd(uint32_t cmd_code, uint8_t *p_buff, uint16_t length);
extern bool app_tws_ibrt_tws_link_connected(void);

#ifndef CROS_POOR_IS_RIGHT
#define CROS_POOR_IS_RIGHT 1
#endif

#define CROS_SAMPLE_RATE AUD_SAMPRATE_16000
#define CROS_BITS AUD_BITS_16
#define CROS_FRAME_SAMPLES 160 /* 10 ms @ 16 kHz — 320 B fits TWS ctrl buf */
#define CROS_FRAME_BYTES (CROS_FRAME_SAMPLES * 2)
#define CROS_DMA_BYTES (CROS_FRAME_BYTES * 2)
#define CROS_RING_BYTES (CROS_FRAME_BYTES * 12)

#define CROS_GAIN_Q15 18000 /* ~0.55 — a bit hotter than Stage A local LB */
#define CROS_PLAY_VOL 12
#define CROS_STREAM_ID AUD_STREAM_ID_0

static uint8_t capture_dma_buf[CROS_DMA_BYTES];
static uint8_t playback_dma_buf[CROS_DMA_BYTES];
static uint8_t pcm_ring[CROS_RING_BYTES];
static uint8_t tx_pkt[CROS_FRAME_BYTES];

static bool inited;
static bool enabled;
static bool tx_running;
static bool rx_running;
static volatile bool tx_busy;
static uint32_t tx_frames;
static uint32_t rx_frames;
static uint32_t tx_drops;
static uint32_t underruns;

static int16_t apply_gain_clip(int16_t s) {
  int32_t v = ((int32_t)s * CROS_GAIN_Q15) >> 15;
  if (v > 32767)
    v = 32767;
  if (v < -32768)
    v = -32768;
  return (int16_t)v;
}

bool cros_tws_is_poor_side(void) {
#if CROS_POOR_IS_RIGHT
  return app_tws_is_right_side();
#else
  return app_tws_is_left_side();
#endif
}

bool cros_tws_is_enabled(void) { return enabled; }

void cros_tws_on_audio_tx_done(void) { tx_busy = false; }

static uint32_t capture_handler(uint8_t *buf, uint32_t len) {
  int16_t *pcm = (int16_t *)buf;
  uint32_t samples = len / sizeof(int16_t);
  uint32_t send_bytes;

  for (uint32_t i = 0; i < samples; i++) {
    pcm[i] = apply_gain_clip(pcm[i]);
  }

  /* Cap to one TWS packet; drop remainder of oversized DMA period. */
  send_bytes = len;
  if (send_bytes > CROS_FRAME_BYTES) {
    send_bytes = CROS_FRAME_BYTES;
  }

  if (!app_tws_ibrt_tws_link_connected()) {
    tx_drops++;
    return len;
  }

  if (tx_busy) {
    tx_drops++;
    return len;
  }

  memcpy(tx_pkt, buf, send_bytes);
  tx_busy = true;
  if (tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_AUDIO, tx_pkt,
                        (uint16_t)send_bytes) != 0) {
    tx_busy = false;
    tx_drops++;
  } else {
    tx_frames++;
    if ((tx_frames & 0x7F) == 0) {
      TRACE(3, "[cros_tws] tx=%u drops=%u busy=%d", tx_frames, tx_drops,
            (int)tx_busy);
    }
  }
  return len;
}

static uint32_t playback_handler(uint8_t *buf, uint32_t len) {
  if (app_audio_pcmbuff_length() >= (int)len) {
    app_audio_pcmbuff_get(buf, (uint16_t)len);
  } else {
    memset(buf, 0, len);
    underruns++;
  }
  rx_frames++;
  return len;
}

static int start_tx(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  if (tx_running) {
    return 0;
  }

  memset(&cfg, 0, sizeof(cfg));
  cfg.bits = CROS_BITS;
  cfg.sample_rate = CROS_SAMPLE_RATE;
  cfg.channel_num = AUD_CHANNEL_NUM_1;
  cfg.device = AUD_STREAM_USE_INT_CODEC;
  cfg.vol = CODEC_SADC_VOL;
  cfg.io_path = AUD_INPUT_PATH_ASRMIC;
  cfg.handler = capture_handler;
  cfg.data_ptr = capture_dma_buf;
  cfg.data_size = CROS_DMA_BYTES;
  cfg.chan_sep_buf = false;

  ret = af_stream_open(CROS_STREAM_ID, AUD_STREAM_CAPTURE, &cfg);
  if (ret) {
    TRACE(2, "[cros_tws] TX capture open fail %d", ret);
    return ret;
  }
  af_stream_start(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  tx_running = true;
  tx_busy = false;
  tx_frames = tx_drops = 0;
  TRACE(0, "[cros_tws] TX (poor mic) START");
  return 0;
}

static int start_rx(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  if (rx_running) {
    return 0;
  }

  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));

  memset(&cfg, 0, sizeof(cfg));
  cfg.bits = CROS_BITS;
  cfg.sample_rate = CROS_SAMPLE_RATE;
  cfg.channel_num = AUD_CHANNEL_NUM_1;
  cfg.device = AUD_STREAM_USE_INT_CODEC;
  cfg.vol = CROS_PLAY_VOL;
  cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
  cfg.handler = playback_handler;
  cfg.data_ptr = playback_dma_buf;
  cfg.data_size = CROS_DMA_BYTES;
  cfg.chan_sep_buf = false;

  ret = af_stream_open(CROS_STREAM_ID, AUD_STREAM_PLAYBACK, &cfg);
  if (ret) {
    TRACE(2, "[cros_tws] RX playback open fail %d", ret);
    return ret;
  }
  af_stream_start(CROS_STREAM_ID, AUD_STREAM_PLAYBACK);
  rx_running = true;
  rx_frames = underruns = 0;
  TRACE(0, "[cros_tws] RX (good speaker) START");
  return 0;
}

static void stop_tx(void) {
  if (!tx_running) {
    return;
  }
  af_stream_stop(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  af_stream_close(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  tx_running = false;
  tx_busy = false;
  TRACE(0, "[cros_tws] TX STOP");
}

static void stop_rx(void) {
  if (!rx_running) {
    return;
  }
  af_stream_stop(CROS_STREAM_ID, AUD_STREAM_PLAYBACK);
  af_stream_close(CROS_STREAM_ID, AUD_STREAM_PLAYBACK);
  rx_running = false;
  TRACE(0, "[cros_tws] RX STOP");
}

static int apply_enabled(bool on) {
  if (on) {
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);
    af_set_priority(AF_USER_TEST, osPriorityHigh);
#ifdef ANC_APP
    if (app_anc_work_status()) {
      TRACE(0, "[cros_tws] disabling ANC for FF mic access");
      app_anc_disable();
    }
#endif
    if (cros_tws_is_poor_side()) {
      stop_rx();
      return start_tx();
    }
    stop_tx();
    return start_rx();
  }

  stop_tx();
  stop_rx();
  app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
  af_set_priority(AF_USER_TEST, osPriorityAboveNormal);
  return 0;
}

void cros_tws_init(void) {
  if (inited) {
    return;
  }
  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));
  enabled = false;
  tx_running = rx_running = false;
  tx_busy = false;
  inited = true;
  TRACE(1, "[cros_tws] init (Stage B CROS, poor=%s)",
        CROS_POOR_IS_RIGHT ? "RIGHT" : "LEFT");
}

int cros_tws_start(void) {
  uint8_t mode = 1;

  if (!inited) {
    cros_tws_init();
  }
  if (enabled) {
    return 0;
  }
  if (!app_tws_ibrt_tws_link_connected()) {
    TRACE(0, "[cros_tws] TWS not linked — pair buds in case first");
    return -1;
  }

  enabled = true;
  tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_MODE, &mode, 1);
  TRACE(1, "[cros_tws] ENABLE (local is %s)",
        cros_tws_is_poor_side() ? "POOR/TX" : "GOOD/RX");
  return apply_enabled(true);
}

int cros_tws_stop(void) {
  uint8_t mode = 0;

  if (!enabled) {
    return 0;
  }
  enabled = false;
  if (app_tws_ibrt_tws_link_connected()) {
    tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_MODE, &mode, 1);
  }
  TRACE(0, "[cros_tws] DISABLE");
  return apply_enabled(false);
}

int cros_tws_toggle(void) {
  if (enabled) {
    return cros_tws_stop();
  }
  return cros_tws_start();
}

void cros_tws_on_peer_mode(uint8_t on) {
  bool want = (on != 0);
  TRACE(1, "[cros_tws] peer mode=%d", (int)want);
  if (want == enabled) {
    /* Re-apply roles in case side/link changed. */
    if (want) {
      apply_enabled(true);
    }
    return;
  }
  enabled = want;
  apply_enabled(want);
}

void cros_tws_on_peer_audio(uint8_t *data, uint16_t len) {
  if (!enabled || cros_tws_is_poor_side() || !rx_running || !data || !len) {
    return;
  }
  if (app_audio_pcmbuff_put(data, len) != 0) {
    app_audio_pcmbuff_discard((uint16_t)(len / 2));
    app_audio_pcmbuff_put(data, len);
  }
  if ((rx_frames & 0x7F) == 0) {
    TRACE(2, "[cros_tws] rx_play=%u underrun=%u", rx_frames, underruns);
  }
}
