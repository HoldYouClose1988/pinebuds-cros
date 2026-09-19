/***************************************************************************
 * Stage A: FF (feed-forward) mic → local speaker loopback.
 *
 * Goal: prove MIC1 ambient path + DAC work on one bud before TWS CROS.
 * - 16 kHz mono
 * - Soft digital gain + peak clip
 * - Auto-start after boot when CROS_STAGE_A is enabled
 * - Quad-tap toggles on/off
 *
 * Wear the bud (or seal the tip) for clap tests; open-air desk tests can howl.
 ***************************************************************************/
#include "cros_loopback.h"

#include "app_audio.h"
#include "app_utils.h"
#include "audioflinger.h"
#include "cmsis_os.h"
#include "hal_trace.h"
#include "string.h"
#include "tgt_hardware.h"

#ifdef ANC_APP
#include "app_anc.h"
#endif

#define CROS_LB_SAMPLE_RATE AUD_SAMPRATE_16000
#define CROS_LB_BITS AUD_BITS_16
#define CROS_LB_FRAME_SAMPLES 256
#define CROS_LB_CHANNELS 1
#define CROS_LB_BYTES_PER_SAMPLE 2
#define CROS_LB_FRAME_BYTES                                                    \
  (CROS_LB_FRAME_SAMPLES * CROS_LB_CHANNELS * CROS_LB_BYTES_PER_SAMPLE)
/* ping-pong: af wants 2x frame in data_size */
#define CROS_LB_DMA_BYTES (CROS_LB_FRAME_BYTES * 2)
#define CROS_LB_RING_BYTES (CROS_LB_FRAME_BYTES * 8)

/* Q15 gain: ~0.45 (-7 dB-ish) — conservative for first desk/wear tests */
#define CROS_LB_GAIN_Q15 14800
#define CROS_LB_PLAY_VOL 10

#define CROS_LB_STREAM_ID AUD_STREAM_ID_0

static uint8_t capture_dma_buf[CROS_LB_DMA_BYTES];
static uint8_t playback_dma_buf[CROS_LB_DMA_BYTES];
static uint8_t pcm_ring[CROS_LB_RING_BYTES];

static bool running;
static bool inited;
static uint32_t cap_frames;
static uint32_t play_frames;
static uint32_t underruns;

static int16_t apply_gain_clip(int16_t s) {
  int32_t v = ((int32_t)s * CROS_LB_GAIN_Q15) >> 15;
  if (v > 32767)
    v = 32767;
  if (v < -32768)
    v = -32768;
  return (int16_t)v;
}

static uint32_t capture_handler(uint8_t *buf, uint32_t len) {
  int16_t *pcm = (int16_t *)buf;
  uint32_t samples = len / sizeof(int16_t);
  for (uint32_t i = 0; i < samples; i++) {
    pcm[i] = apply_gain_clip(pcm[i]);
  }
  if (app_audio_pcmbuff_put(buf, (uint16_t)len) != 0) {
    /* ring full — drop oldest half-frame worth */
    app_audio_pcmbuff_discard((uint16_t)(len / 2));
    app_audio_pcmbuff_put(buf, (uint16_t)len);
  }
  cap_frames++;
  if ((cap_frames & 0x7F) == 0) {
    TRACE(3, "[cros_lb] cap=%u play=%u underrun=%u", cap_frames, play_frames,
          underruns);
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
  play_frames++;
  return len;
}

void cros_loopback_init(void) {
  if (inited) {
    return;
  }
  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));
  running = false;
  inited = true;
  TRACE(0, "[cros_lb] init (Stage A FF loopback)");
}

static int open_streams(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  memset(&cfg, 0, sizeof(cfg));
  cfg.bits = CROS_LB_BITS;
  cfg.sample_rate = CROS_LB_SAMPLE_RATE;
  cfg.channel_num = AUD_CHANNEL_NUM_1;
  cfg.device = AUD_STREAM_USE_INT_CODEC;
  cfg.vol = CODEC_SADC_VOL;
  /* ASRMIC remapped to FF MIC1+VMIC2 in tgt_hardware when CROS_STAGE_A=1 */
  cfg.io_path = AUD_INPUT_PATH_ASRMIC;
  cfg.handler = capture_handler;
  cfg.data_ptr = capture_dma_buf;
  cfg.data_size = CROS_LB_DMA_BYTES;
  cfg.chan_sep_buf = false;

  ret = af_stream_open(CROS_LB_STREAM_ID, AUD_STREAM_CAPTURE, &cfg);
  if (ret) {
    TRACE(2, "[cros_lb] capture open fail %d", ret);
    return ret;
  }

  memset(&cfg, 0, sizeof(cfg));
  cfg.bits = CROS_LB_BITS;
  cfg.sample_rate = CROS_LB_SAMPLE_RATE;
  cfg.channel_num = AUD_CHANNEL_NUM_1;
  cfg.device = AUD_STREAM_USE_INT_CODEC;
  cfg.vol = CROS_LB_PLAY_VOL;
  cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
  cfg.handler = playback_handler;
  cfg.data_ptr = playback_dma_buf;
  cfg.data_size = CROS_LB_DMA_BYTES;
  cfg.chan_sep_buf = false;

  ret = af_stream_open(CROS_LB_STREAM_ID, AUD_STREAM_PLAYBACK, &cfg);
  if (ret) {
    TRACE(2, "[cros_lb] playback open fail %d", ret);
    af_stream_close(CROS_LB_STREAM_ID, AUD_STREAM_CAPTURE);
    return ret;
  }

  af_stream_start(CROS_LB_STREAM_ID, AUD_STREAM_CAPTURE);
  af_stream_start(CROS_LB_STREAM_ID, AUD_STREAM_PLAYBACK);
  return 0;
}

int cros_loopback_start(void) {
  if (!inited) {
    cros_loopback_init();
  }
  if (running) {
    return 0;
  }

  TRACE(0, "[cros_lb] START");
  app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);
  af_set_priority(AF_USER_TEST, osPriorityHigh);

#ifdef ANC_APP
  if (app_anc_work_status()) {
    TRACE(0, "[cros_lb] disabling ANC for FF mic access");
    app_anc_disable();
  }
#endif

  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));
  cap_frames = play_frames = underruns = 0;

  if (open_streams() != 0) {
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    return -1;
  }

  running = true;
  return 0;
}

int cros_loopback_stop(void) {
  if (!running) {
    return 0;
  }
  TRACE(0, "[cros_lb] STOP");

  af_stream_stop(CROS_LB_STREAM_ID, AUD_STREAM_PLAYBACK);
  af_stream_stop(CROS_LB_STREAM_ID, AUD_STREAM_CAPTURE);
  af_stream_close(CROS_LB_STREAM_ID, AUD_STREAM_PLAYBACK);
  af_stream_close(CROS_LB_STREAM_ID, AUD_STREAM_CAPTURE);

  app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
  af_set_priority(AF_USER_TEST, osPriorityAboveNormal);
  running = false;
  return 0;
}

int cros_loopback_toggle(void) {
  if (running) {
    return cros_loopback_stop();
  }
  return cros_loopback_start();
}

bool cros_loopback_is_running(void) { return running; }
