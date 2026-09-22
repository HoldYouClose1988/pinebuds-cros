/***************************************************************************
 * Stage B: poor-side FF mic → TWS → good-side speaker (experimental CROS).
 *
 * v0.2.2: stop flooding the IBRT cmd queue (was ~1s backlog + chop).
 *  - Exactly 1 packet in flight; never queue ahead of tx_done
 *  - 10 ms ADPCM frames
 *  - Hard RX latency clamp (~60 ms max buffered)
 *  - Soft limiter so loud nail scrapes don't spike the link
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

extern int tws_ctrl_send_cmd(uint32_t cmd_code, uint8_t *p_buff, uint16_t length);
extern bool app_tws_ibrt_tws_link_connected(void);
/* Direct path from customif — avoids tws_ctrl mailbox backlog. */
extern int app_ibrt_cros_audio_send_now(uint8_t *p_buff, uint16_t length);

#ifndef CROS_POOR_IS_RIGHT
#define CROS_POOR_IS_RIGHT 1
#endif

#define CROS_SAMPLE_RATE AUD_SAMPRATE_16000
#define CROS_BITS AUD_BITS_16
#define CROS_FRAME_SAMPLES 160 /* 10 ms */
#define CROS_FRAME_BYTES (CROS_FRAME_SAMPLES * 2)
#define CROS_ADPCM_BYTES (CROS_FRAME_SAMPLES / 2)
#define CROS_PKT_BYTES (4 + CROS_ADPCM_BYTES)
#define CROS_DMA_BYTES (CROS_FRAME_BYTES * 2)
#define CROS_RING_BYTES (CROS_FRAME_BYTES * 10)
#define CROS_PREBUF_BYTES (CROS_FRAME_BYTES * 2)  /* ~20 ms */
#define CROS_MAX_BUF_BYTES (CROS_FRAME_BYTES * 6) /* ~60 ms latency clamp */

#define CROS_GAIN_Q15 16000 /* ~0.49 — quieter to reduce spike dropouts */
#define CROS_LIM_THRESH 20000
#define CROS_PLAY_VOL 12
#define CROS_STREAM_ID AUD_STREAM_ID_0
#define CROS_PKT_MAGIC 0xA5

static uint8_t capture_dma_buf[CROS_DMA_BYTES];
static uint8_t playback_dma_buf[CROS_DMA_BYTES];
static uint8_t pcm_ring[CROS_RING_BYTES];
static uint8_t tx_pkt[CROS_PKT_BYTES];
static int16_t pcm_acc[CROS_FRAME_SAMPLES];
static uint16_t pcm_acc_count;
static int16_t decode_pcm[CROS_FRAME_SAMPLES];

static bool inited;
static bool enabled;
static bool tx_running;
static bool rx_running;
static volatile uint8_t tx_busy;
static uint8_t tx_busy_age;
static uint32_t tx_frames;
static uint32_t rx_pkts;
static uint32_t tx_drops;
static uint32_t underruns;
static uint32_t rx_drops;

/* ---- IMA ADPCM (mono) ---- */
static const int16_t ima_step_table[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t ima_index_table[16] = {-1, -1, -1, -1, 2, 4, 6, 8,
                                          -1, -1, -1, -1, 2, 4, 6, 8};

typedef struct {
  int16_t pred;
  int8_t index;
} ima_state_t;

static ima_state_t enc_state;
static ima_state_t dec_state;

static int16_t clamp16(int32_t v) {
  if (v > 32767)
    return 32767;
  if (v < -32768)
    return -32768;
  return (int16_t)v;
}

static uint8_t ima_encode_nibble(ima_state_t *st, int16_t sample) {
  int step = ima_step_table[st->index];
  int32_t diff = (int32_t)sample - st->pred;
  uint8_t nibble = 0;
  if (diff < 0) {
    nibble = 8;
    diff = -diff;
  }
  if (diff >= step) {
    nibble |= 4;
    diff -= step;
  }
  if (diff >= (step >> 1)) {
    nibble |= 2;
    diff -= (step >> 1);
  }
  if (diff >= (step >> 2)) {
    nibble |= 1;
  }

  int32_t diffq = step >> 3;
  if (nibble & 4)
    diffq += step;
  if (nibble & 2)
    diffq += step >> 1;
  if (nibble & 1)
    diffq += step >> 2;
  if (nibble & 8)
    st->pred = clamp16(st->pred - diffq);
  else
    st->pred = clamp16(st->pred + diffq);

  st->index = (int8_t)(st->index + ima_index_table[nibble]);
  if (st->index < 0)
    st->index = 0;
  if (st->index > 88)
    st->index = 88;
  return nibble & 0x0F;
}

static int16_t ima_decode_nibble(ima_state_t *st, uint8_t nibble) {
  int step = ima_step_table[st->index];
  int32_t diffq = step >> 3;
  if (nibble & 4)
    diffq += step;
  if (nibble & 2)
    diffq += step >> 1;
  if (nibble & 1)
    diffq += step >> 2;
  if (nibble & 8)
    st->pred = clamp16(st->pred - diffq);
  else
    st->pred = clamp16(st->pred + diffq);

  st->index = (int8_t)(st->index + ima_index_table[nibble & 0x0F]);
  if (st->index < 0)
    st->index = 0;
  if (st->index > 88)
    st->index = 88;
  return st->pred;
}

static void ima_encode_block(const int16_t *pcm, uint8_t *out) {
  for (uint32_t i = 0; i < CROS_FRAME_SAMPLES; i += 2) {
    uint8_t lo = ima_encode_nibble(&enc_state, pcm[i]);
    uint8_t hi = ima_encode_nibble(&enc_state, pcm[i + 1]);
    out[i / 2] = (uint8_t)(lo | (hi << 4));
  }
}

static void ima_decode_block(const uint8_t *in, int16_t *pcm) {
  for (uint32_t i = 0; i < CROS_ADPCM_BYTES; i++) {
    uint8_t b = in[i];
    pcm[i * 2] = ima_decode_nibble(&dec_state, b & 0x0F);
    pcm[i * 2 + 1] = ima_decode_nibble(&dec_state, b >> 4);
  }
}

static int16_t process_sample(int16_t s) {
  int32_t v = ((int32_t)s * CROS_GAIN_Q15) >> 15;
  /* Soft knee toward lim threshold — loud nail scrapes were killing the link. */
  if (v > CROS_LIM_THRESH) {
    v = CROS_LIM_THRESH + ((v - CROS_LIM_THRESH) >> 2);
  } else if (v < -CROS_LIM_THRESH) {
    v = -CROS_LIM_THRESH - ((-CROS_LIM_THRESH - v) >> 2);
  }
  return clamp16(v);
}

bool cros_tws_is_poor_side(void) {
#if CROS_POOR_IS_RIGHT
  return app_tws_is_right_side();
#else
  return app_tws_is_left_side();
#endif
}

bool cros_tws_is_enabled(void) { return enabled; }

void cros_tws_on_audio_tx_done(void) {
  tx_busy = 0;
  tx_busy_age = 0;
}

static void send_adpcm_frame(const int16_t *pcm) {
  if (!app_tws_ibrt_tws_link_connected()) {
    tx_drops++;
    return;
  }
  /* One packet in flight. If tx_done is missed, age out after ~30 ms. */
  if (tx_busy) {
    if (++tx_busy_age < 3) {
      tx_drops++;
      return;
    }
    tx_busy = 0;
    tx_busy_age = 0;
  }

  tx_pkt[0] = CROS_PKT_MAGIC;
  tx_pkt[1] = (uint8_t)enc_state.index;
  tx_pkt[2] = (uint8_t)(enc_state.pred & 0xFF);
  tx_pkt[3] = (uint8_t)((enc_state.pred >> 8) & 0xFF);
  ima_encode_block(pcm, &tx_pkt[4]);

  tx_busy = 1;
  tx_busy_age = 0;
  if (app_ibrt_cros_audio_send_now(tx_pkt, CROS_PKT_BYTES) != 0) {
    tx_busy = 0;
    tx_drops++;
  } else {
    tx_frames++;
    if ((tx_frames & 0x7F) == 0) {
      TRACE(2, "[cros_tws] tx=%u drops=%u", tx_frames, tx_drops);
    }
  }
}

static uint32_t capture_handler(uint8_t *buf, uint32_t len) {
  int16_t *pcm = (int16_t *)buf;
  uint32_t samples = len / sizeof(int16_t);

  for (uint32_t i = 0; i < samples; i++) {
    pcm_acc[pcm_acc_count++] = process_sample(pcm[i]);
    if (pcm_acc_count >= CROS_FRAME_SAMPLES) {
      send_adpcm_frame(pcm_acc);
      pcm_acc_count = 0;
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
  return len;
}

static int start_tx(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  if (tx_running) {
    return 0;
  }

  memset(&enc_state, 0, sizeof(enc_state));
  pcm_acc_count = 0;
  tx_busy = 0;

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
  tx_frames = tx_drops = 0;
  TRACE(0, "[cros_tws] TX gated-ADPCM START");
  return 0;
}

static int start_rx(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  if (rx_running) {
    return 0;
  }

  memset(&dec_state, 0, sizeof(dec_state));
  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));
  {
    static uint8_t silence[CROS_PREBUF_BYTES];
    memset(silence, 0, sizeof(silence));
    app_audio_pcmbuff_put(silence, CROS_PREBUF_BYTES);
  }

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
  rx_pkts = underruns = rx_drops = 0;
  TRACE(0, "[cros_tws] RX latency-clamped START");
  return 0;
}

static void stop_tx(void) {
  if (!tx_running) {
    return;
  }
  af_stream_stop(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  af_stream_close(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  tx_running = false;
  tx_busy = 0;
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
  tx_busy = 0;
  inited = true;
  TRACE(1, "[cros_tws] init v0.2.2 (poor=%s)",
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
    if (want) {
      apply_enabled(true);
    }
    return;
  }
  enabled = want;
  apply_enabled(want);
}

void cros_tws_on_peer_audio(uint8_t *data, uint16_t len) {
  int buffered;

  if (!enabled || cros_tws_is_poor_side() || !rx_running || !data) {
    return;
  }
  if (len < CROS_PKT_BYTES || data[0] != CROS_PKT_MAGIC) {
    rx_drops++;
    return;
  }

  /* Latency clamp: if we already have >60 ms queued, drop this packet. */
  buffered = app_audio_pcmbuff_length();
  if (buffered > (int)CROS_MAX_BUF_BYTES) {
    app_audio_pcmbuff_discard(
        (uint16_t)(buffered - (int)(CROS_FRAME_BYTES * 2)));
    rx_drops++;
  }

  dec_state.index = (int8_t)data[1];
  if (dec_state.index < 0)
    dec_state.index = 0;
  if (dec_state.index > 88)
    dec_state.index = 88;
  dec_state.pred = (int16_t)(data[2] | (data[3] << 8));

  ima_decode_block(&data[4], decode_pcm);

  if (app_audio_pcmbuff_put((uint8_t *)decode_pcm, CROS_FRAME_BYTES) != 0) {
    app_audio_pcmbuff_discard((uint16_t)(CROS_FRAME_BYTES / 2));
    app_audio_pcmbuff_put((uint8_t *)decode_pcm, CROS_FRAME_BYTES);
  }
  rx_pkts++;
  if ((rx_pkts & 0x7F) == 0) {
    TRACE(3, "[cros_tws] rx=%u underrun=%u drop=%u", rx_pkts, underruns,
          rx_drops);
  }
}
