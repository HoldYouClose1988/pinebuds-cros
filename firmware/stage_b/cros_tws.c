/***************************************************************************
 * Stage B: poor-side FF mic → TWS → good-side speaker (experimental CROS).
 *
 * v0.3.27 — A2DP-suspend off (misread); underrun logs UART-only while quiet.
 ***************************************************************************/
#include "cros_tws.h"

#include "app_audio.h"
#include "app_ibrt_customif_cmd.h"
#include "app_tws_ibrt.h"
#include "app_tws_if.h"
#include "app_utils.h"
#include "audioflinger.h"
#include "cmsis_os.h"
#include "cros_besaud_extra.h"
#include "cros_bt_log.h"
#include "cros_lat.h"
#include "hal_trace.h"
#include "string.h"
#include "tgt_hardware.h"

#ifdef ANC_APP
#include "app_anc.h"
#endif

extern int tws_ctrl_send_cmd(uint32_t cmd_code, uint8_t *p_buff, uint16_t length);
extern bool app_tws_ibrt_tws_link_connected(void);
extern int app_ibrt_if_tws_sniff_block(uint32_t block_next_sec);
extern int app_ibrt_if_sniff_checker_start(int user);
extern int app_ibrt_if_sniff_checker_stop(int user);
/* Do NOT call app_ibrt_cros_audio_send_now from osTimer — needs BT/ctrl
 * context. v0.2.6 did that and hung the TX (right) bud. */

/* Mirror APP_IBRT_IF_SNIFF_CHECKER_USER_SPP (HFP=0, A2DP=1, SPP=2). */
#define CROS_SNIFF_CHECKER_USER_SPP 2

/* Optional phone-A2DP pause while CROS on. Default OFF — 0.3.25 cutouts were
 * with PC speakers (acoustic), not BT A2DP. Build with CROS_SUSPEND_A2DP=1 if
 * you actually stream music to the buds and want that coexist policy. */
#ifndef CROS_SUSPEND_A2DP
#define CROS_SUSPEND_A2DP 0
#endif
#if CROS_SUSPEND_A2DP
extern int cros_a2dp_music_ongoing(void);
extern void cros_a2dp_suspend_for_cros(void);
#endif

#ifndef CROS_POOR_IS_RIGHT
#define CROS_POOR_IS_RIGHT 1
#endif

#define CROS_SAMPLE_RATE AUD_SAMPRATE_16000
#define CROS_BITS AUD_BITS_16
#define CROS_CAP_SAMPLES 160   /* 10 ms capture quantum */
#define CROS_FRAME_SAMPLES 800 /* 50 ms packet @ 16 kHz */
#define CROS_FRAME_BYTES (CROS_FRAME_SAMPLES * 2)
#define CROS_ADPCM_BYTES (CROS_FRAME_SAMPLES / 2) /* 400 */
#define CROS_HDR_BYTES 5
#define CROS_PKT_BYTES (CROS_HDR_BYTES + CROS_ADPCM_BYTES) /* 405 < 672 */
#define CROS_DMA_BYTES (CROS_CAP_SAMPLES * 2 * 2)
#define CROS_RING_BYTES (CROS_FRAME_BYTES * 12)

#define CROS_GAIN_Q15 16000
#define CROS_LIM_THRESH 20000
#define CROS_PLAY_VOL 12
#define CROS_STREAM_ID AUD_STREAM_ID_0
#define CROS_PKT_MAGIC 0xA5

/* Cmd-path jitter (also used before extra READY). */
#define CROS_JITTER_MIN_FRAMES 2 /* 100 ms */
#define CROS_JITTER_MAX_FRAMES 4 /* 200 ms */
/* Extra floor=4 (200 ms) — validated usable on 0.3.21 (~330 ms clap, rare cutouts). */
#define CROS_EXTRA_JITTER_MIN_FRAMES 4 /* 200 ms floor */
#define CROS_EXTRA_JITTER_MAX_FRAMES 8 /* 400 ms */
/*
 * Keep send tick = frame period. v0.3.22 tried 10 ms poll (same 50 ms frames,
 * ms-scaled stuck watchdog) — cutouts returned, clap stayed ~330 ms. Reverted.
 */
#define CROS_TICK_MS 50
#define CROS_TX_STUCK_MS 200
#define CROS_TX_STUCK_TICKS                                                        \
  ((CROS_TX_STUCK_MS + CROS_TICK_MS - 1) / CROS_TICK_MS) /* 4 @ 50 ms */
#define CROS_JITTER_HEALTHY_MS 7500
#define CROS_JITTER_HEALTHY_TICKS                                                  \
  ((CROS_JITTER_HEALTHY_MS + CROS_TICK_MS - 1) / CROS_TICK_MS) /* 150 @ 50 ms */
/* Refresh sniff block while CROS is on (Capture already blocks via TOTA). */
#define CROS_SNIFF_BLOCK_SEC 120
#define CROS_SNIFF_REFRESH_MS 60000
#define CROS_SNIFF_REFRESH_TICKS                                                   \
  ((CROS_SNIFF_REFRESH_MS + CROS_TICK_MS - 1) / CROS_TICK_MS)
#define CROS_RX_LOG_MASK 0x3F

static uint8_t capture_dma_buf[CROS_DMA_BYTES];
static uint8_t playback_dma_buf[CROS_DMA_BYTES];
static uint8_t pcm_ring[CROS_RING_BYTES];
static uint8_t tx_pkt[CROS_PKT_BYTES];
static int16_t send_pcm[CROS_FRAME_SAMPLES]; /* ticker scratch — not on stack */

static int16_t latest_pcm[CROS_FRAME_SAMPLES];
static volatile uint8_t latest_ready;
static int16_t cap_acc[CROS_FRAME_SAMPLES];
static uint16_t cap_acc_count;

static int16_t decode_pcm[CROS_FRAME_SAMPLES];
static int16_t last_play[CROS_FRAME_SAMPLES];

static bool inited;
static bool enabled;
static bool tx_running;
static bool rx_running;
static volatile uint8_t tx_pending;
static uint8_t tx_seq;
static uint8_t rx_expect_seq;
static bool rx_have_seq;
static uint8_t jitter_target_frames;
static uint16_t healthy_ticks;
static uint16_t sniff_refresh_ticks;
#if CROS_SUSPEND_A2DP
static uint8_t a2dp_paused_for_cros;
#endif
static uint32_t tx_frames;
static uint32_t tx_extra;
static uint32_t tx_cmd;
static uint32_t rx_pkts;
static uint32_t tx_drops;
static uint32_t underruns;
static uint32_t rx_drops;
static uint32_t rx_resyncs;

static uint16_t tx_stuck_ticks;

static void cros_tick(void const *arg);
osTimerDef(CROS_TICK, cros_tick);
static osTimerId cros_tick_id;

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

static int on_extra_media(void) {
  return (cros_besaud_extra_is_open() && cros_besaud_extra_peer_ready()) ? 1 : 0;
}

static uint8_t jitter_min_now(void) {
  return on_extra_media() ? CROS_EXTRA_JITTER_MIN_FRAMES : CROS_JITTER_MIN_FRAMES;
}

static uint8_t jitter_max_now(void) {
  return on_extra_media() ? CROS_EXTRA_JITTER_MAX_FRAMES : CROS_JITTER_MAX_FRAMES;
}

static void jitter_apply_extra_floor(void) {
  uint8_t floor = jitter_min_now();
  if (jitter_target_frames < floor) {
    jitter_target_frames = floor;
    CROS_LOG(1, "[cros_tws] jitter floor=%u (extra media)", (unsigned)floor);
  }
}

static const char *link_mode_name(uint8_t mode) {
  switch (mode) {
  case IBRT_ACTIVE_MODE:
    return "ACTIVE";
  case IBRT_SNIFF_MODE:
    return "SNIFF";
  default:
    return "?";
  }
}

static void log_link_modes(const char *where) {
  ibrt_ctrl_t *ctx = app_tws_ibrt_get_bt_ctrl_ctx();
  if (!ctx) {
    CROS_LOG(0, "[cros_tws] link@%s (no ibrt ctx)", where);
    return;
  }
  CROS_LOG(0, "[cros_tws] link@%s tws=%s(%u) mobile=%s(%u)", where,
        link_mode_name((uint8_t)ctx->tws_mode), (unsigned)ctx->tws_mode,
        link_mode_name((uint8_t)ctx->mobile_mode), (unsigned)ctx->mobile_mode);
}

static void cros_sniff_lock_on(void) {
  app_ibrt_if_tws_sniff_block(CROS_SNIFF_BLOCK_SEC);
  app_ibrt_if_sniff_checker_start(CROS_SNIFF_CHECKER_USER_SPP);
  if (app_tws_ibrt_tws_link_connected()) {
    app_tws_ibrt_exit_sniff_with_tws();
  }
  sniff_refresh_ticks = 0;
  CROS_LOG(0, "[cros_tws] sniff LOCK (block %us + exit_tws + checker)",
        (unsigned)CROS_SNIFF_BLOCK_SEC);
  log_link_modes("lock");
}

static void cros_sniff_lock_off(void) {
  app_ibrt_if_sniff_checker_stop(CROS_SNIFF_CHECKER_USER_SPP);
  /* Expire the timed block immediately. */
  app_ibrt_if_tws_sniff_block(0);
  sniff_refresh_ticks = 0;
  CROS_LOG(0, "[cros_tws] sniff UNLOCK");
  log_link_modes("unlock");
}

/* Optional A2DP pause (off by default — see CROS_SUSPEND_A2DP). */
#if CROS_SUSPEND_A2DP
static void cros_a2dp_coexist_on(void) {
  if (cros_a2dp_music_ongoing()) {
    CROS_LOG(0, "[cros_tws] A2DP streaming — suspend for CROS (airtime/ACL)");
    cros_a2dp_suspend_for_cros();
    a2dp_paused_for_cros = 1;
  } else {
    a2dp_paused_for_cros = 0;
    CROS_LOG(0, "[cros_tws] A2DP idle — no suspend");
  }
}

static void cros_a2dp_coexist_off(void) {
  if (a2dp_paused_for_cros) {
    CROS_LOG(0, "[cros_tws] A2DP was suspended for CROS — press play to resume");
    a2dp_paused_for_cros = 0;
  }
}
#endif

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
  tx_pending = 0;
  cros_lat_note_cmd_tx_done();
}

static uint32_t capture_handler(uint8_t *buf, uint32_t len) {
  int16_t *pcm = (int16_t *)buf;
  uint32_t samples = len / sizeof(int16_t);

  for (uint32_t i = 0; i < samples; i++) {
    cap_acc[cap_acc_count++] = process_sample(pcm[i]);
    if (cap_acc_count >= CROS_FRAME_SAMPLES) {
      memcpy(latest_pcm, cap_acc, sizeof(latest_pcm));
      latest_ready = 1;
      cap_acc_count = 0;
      cros_lat_note_cap_done();
    }
  }
  return len;
}

static uint32_t playback_handler(uint8_t *buf, uint32_t len) {
  if (app_audio_pcmbuff_length() >= (int)len) {
    app_audio_pcmbuff_get(buf, (uint16_t)len);
    cros_lat_note_play_ok((uint32_t)app_audio_pcmbuff_length());
    if (len >= sizeof(last_play)) {
      memcpy(last_play, buf + len - sizeof(last_play), sizeof(last_play));
    }
  } else {
    uint32_t filled = 0;
    while (filled < len) {
      uint32_t chunk = sizeof(last_play);
      if (chunk > len - filled)
        chunk = len - filled;
      memcpy(buf + filled, last_play, chunk);
      filled += chunk;
    }
    underruns++;
    cros_lat_note_underrun();
    healthy_ticks = 0;
    if (jitter_target_frames < jitter_max_now()) {
      jitter_target_frames++;
    }
  }
  return len;
}

static void try_send_latest(void) {
  ima_state_t snap;
  int busy;

  if (!tx_running || !app_tws_ibrt_tws_link_connected()) {
    return;
  }

  busy = (cros_besaud_extra_is_open() && cros_besaud_extra_peer_ready())
             ? (cros_besaud_extra_tx_busy() ? 1 : 0)
             : (tx_pending ? 1 : 0);
  if (busy) {
    tx_stuck_ticks++;
    if (tx_stuck_ticks >= CROS_TX_STUCK_TICKS) {
      cros_besaud_extra_force_clear_pending();
      tx_pending = 0;
      tx_stuck_ticks = 0;
      tx_drops++;
    }
    return;
  }
  tx_stuck_ticks = 0;

  if (!latest_ready) {
    return;
  }

  latest_ready = 0;
  memcpy(send_pcm, latest_pcm, sizeof(send_pcm));

  snap = enc_state;
  tx_pkt[0] = CROS_PKT_MAGIC;
  tx_pkt[1] = tx_seq++;
  tx_pkt[2] = (uint8_t)snap.index;
  tx_pkt[3] = (uint8_t)(snap.pred & 0xFF);
  tx_pkt[4] = (uint8_t)((snap.pred >> 8) & 0xFF);
  ima_encode_block(send_pcm, &tx_pkt[CROS_HDR_BYTES]);

  /* Extra only after peer PONG/audio proves RX — otherwise stay on cmd. */
  if (cros_besaud_extra_is_open() && cros_besaud_extra_peer_ready()) {
    cros_lat_note_send_begin(1);
    if (cros_besaud_extra_send(tx_pkt, CROS_PKT_BYTES) != 0) {
      tx_drops++;
    } else {
      tx_frames++;
      tx_extra++;
      if ((tx_frames & 0x3F) == 0) {
        CROS_LOG_STAT(0, "[cros_tws] tx=%u extra=%u cmd=%u (on EXTRA)", tx_frames,
              tx_extra, tx_cmd);
      }
    }
    return;
  }

  cros_lat_note_send_begin(0);
  tx_pending = 1;
  if (tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_AUDIO, tx_pkt, CROS_PKT_BYTES) !=
      0) {
    tx_pending = 0;
    tx_drops++;
  } else {
    tx_frames++;
    tx_cmd++;
    if ((tx_frames & 0x3F) == 0) {
      CROS_LOG_STAT(0, "[cros_tws] tx=%u extra=%u cmd=%u (on CMD, waiting extra)",
            tx_frames, tx_extra, tx_cmd);
    }
  }
}

static void cros_tick(void const *arg) {
  (void)arg;
  if (!enabled) {
    return;
  }
  if (tx_running) {
    try_send_latest();
  }
  if (rx_running) {
    healthy_ticks++;
    /* Never shrink below extra floor while riding L2CAP — 0.3.14 cliffed
     * after healthy_ticks pulled jitter down to 2–3. */
    if (healthy_ticks > CROS_JITTER_HEALTHY_TICKS &&
        jitter_target_frames > jitter_min_now()) {
      jitter_target_frames--;
      healthy_ticks = 0;
    }
  }
  /* Keep sniff block fresh while CROS is live (Capture-off case). */
  sniff_refresh_ticks++;
  if (sniff_refresh_ticks >= CROS_SNIFF_REFRESH_TICKS) {
    sniff_refresh_ticks = 0;
    app_ibrt_if_tws_sniff_block(CROS_SNIFF_BLOCK_SEC);
    log_link_modes("refresh");
  }
}

static void tick_start(void) {
  if (!cros_tick_id) {
    cros_tick_id = osTimerCreate(osTimer(CROS_TICK), osTimerPeriodic, NULL);
  }
  if (cros_tick_id) {
    osTimerStart(cros_tick_id, CROS_TICK_MS);
  }
}

static void tick_stop(void) {
  if (cros_tick_id) {
    osTimerStop(cros_tick_id);
  }
}

static int start_tx(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  if (tx_running) {
    return 0;
  }

  memset(&enc_state, 0, sizeof(enc_state));
  cap_acc_count = 0;
  latest_ready = 0;
  tx_seq = 0;
  tx_pending = 0;

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
    CROS_LOG(2, "[cros_tws] TX capture open fail %d", ret);
    return ret;
  }
  af_stream_start(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  tx_running = true;
  tx_frames = tx_extra = tx_cmd = tx_drops = 0;
  cros_lat_reset();
  tick_start();
  CROS_LOG(0, "[cros_tws] TX START (50ms ADPCM; cmd until peer READY)");
  return 0;
}

static int start_rx(void) {
  struct AF_STREAM_CONFIG_T cfg;
  int ret;

  if (rx_running) {
    return 0;
  }

  memset(&dec_state, 0, sizeof(dec_state));
  memset(last_play, 0, sizeof(last_play));
  rx_have_seq = false;
  jitter_target_frames = CROS_JITTER_MIN_FRAMES;
  healthy_ticks = 0;
  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));
  {
    uint32_t pre = jitter_target_frames * CROS_FRAME_BYTES;
    static uint8_t silence[CROS_FRAME_BYTES * CROS_EXTRA_JITTER_MAX_FRAMES];
    if (pre > sizeof(silence))
      pre = sizeof(silence);
    memset(silence, 0, pre);
    app_audio_pcmbuff_put(silence, (uint16_t)pre);
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
    CROS_LOG(2, "[cros_tws] RX playback open fail %d", ret);
    return ret;
  }
  af_stream_start(CROS_STREAM_ID, AUD_STREAM_PLAYBACK);
  rx_running = true;
  rx_pkts = underruns = rx_drops = rx_resyncs = 0;
  cros_lat_reset();
  tick_start();
  CROS_LOG(0, "[cros_tws] RX START (50ms cmd until PONG, jitter %u-%u)",
        (unsigned)CROS_JITTER_MIN_FRAMES, (unsigned)CROS_JITTER_MAX_FRAMES);
  return 0;
}

static void stop_tx(void) {
  if (!tx_running) {
    return;
  }
  tick_stop();
  af_stream_stop(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  af_stream_close(CROS_STREAM_ID, AUD_STREAM_CAPTURE);
  tx_running = false;
  cros_lat_dump("TX_STOP");
  cros_besaud_extra_log_l2cap_mode();
  CROS_LOG(0, "[cros_tws] TX STOP");
}

static void stop_rx(void) {
  if (!rx_running) {
    return;
  }
  tick_stop();
  af_stream_stop(CROS_STREAM_ID, AUD_STREAM_PLAYBACK);
  af_stream_close(CROS_STREAM_ID, AUD_STREAM_PLAYBACK);
  rx_running = false;
  cros_lat_dump("RX_STOP");
  cros_besaud_extra_log_l2cap_mode();
  CROS_LOG(0, "[cros_tws] RX STOP");
}

static int apply_enabled(bool on) {
  if (on) {
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_104M);
    af_set_priority(AF_USER_TEST, osPriorityHigh);
#if CROS_SUSPEND_A2DP
    cros_a2dp_coexist_on();
#endif
    cros_sniff_lock_on();
#ifdef ANC_APP
    if (app_anc_work_status()) {
      CROS_LOG(0, "[cros_tws] disabling ANC for FF mic access");
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
  cros_sniff_lock_off();
#if CROS_SUSPEND_A2DP
  cros_a2dp_coexist_off();
#endif
  app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
  af_set_priority(AF_USER_TEST, osPriorityAboveNormal);
  return 0;
}

/* Raw side APIs — not the compiled poor=RIGHT label. */
static void log_side_probe(const char *where) {
  int left = app_tws_is_left_side() ? 1 : 0;
  int right = app_tws_is_right_side() ? 1 : 0;
  CROS_LOG(0,
        "[cros_tws] side@%s left=%d right=%d poor_cfg=%s role=%s", where, left,
        right, CROS_POOR_IS_RIGHT ? "RIGHT" : "LEFT",
        cros_tws_is_poor_side() ? "POOR/TX" : "GOOD/RX");
}

void cros_tws_init(void) {
  if (inited) {
    return;
  }
  cros_bt_log_init();
  cros_besaud_extra_init();
  app_audio_pcmbuff_init(pcm_ring, sizeof(pcm_ring));
  enabled = false;
  tx_running = rx_running = false;
  jitter_target_frames = CROS_JITTER_MIN_FRAMES;
  tx_stuck_ticks = 0;
  inited = true;
  cros_lat_reset();
  CROS_LOG(1, "[cros_tws] init v0.3.27 quiet-underrun+G floor4 (poor_cfg=%s)",
        CROS_POOR_IS_RIGHT ? "RIGHT" : "LEFT");
  log_side_probe("init");
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
    CROS_LOG(0, "[cros_tws] TWS not linked — pair buds in case first");
    return -1;
  }

  enabled = true;
  tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_MODE, &mode, 1);
  cros_besaud_extra_ensure();
  log_side_probe("enable");
  CROS_LOG(2, "[cros_tws] ENABLE extra_open=%d",
        cros_besaud_extra_is_open() ? 1 : 0);
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
  cros_bt_log_set_quiet(0);
  CROS_LOG(0, "[cros_tws] DISABLE");
  return apply_enabled(false);
}

int cros_tws_toggle(void) {
  if (enabled) {
    return cros_tws_stop();
  }
  if (!app_tws_ibrt_tws_link_connected()) {
    CROS_LOG(0, "[cros_tws] toggle ignored — TWS not linked (reseating both?)");
    return -1;
  }
  return cros_tws_start();
}

void cros_tws_on_peer_mode(uint8_t on) {
  bool want = (on != 0);
  CROS_LOG(1, "[cros_tws] peer mode=%d", (int)want);
  log_side_probe("peer_mode");
  if (want == enabled) {
    if (want) {
      cros_besaud_extra_ensure();
      apply_enabled(true);
    } else {
      /* Remote stop while already disabled — still clear quiet. */
      cros_bt_log_set_quiet(0);
    }
    return;
  }
  enabled = want;
  if (want) {
    cros_besaud_extra_ensure();
  } else {
    /* Remote-initiated stop must leave quiet mode (local stop already does). */
    cros_bt_log_set_quiet(0);
  }
  apply_enabled(want);
}

void cros_tws_on_peer_audio(uint8_t *data, uint16_t len) {
  int buffered;
  int max_bytes;
  uint8_t seq;
  bool gap;

  if (!enabled || cros_tws_is_poor_side() || !rx_running || !data) {
    return;
  }
  cros_lat_note_recv_begin();
  if (len < CROS_PKT_BYTES || data[0] != CROS_PKT_MAGIC) {
    rx_drops++;
    return;
  }

  jitter_apply_extra_floor();

  max_bytes = (int)jitter_target_frames * (int)CROS_FRAME_BYTES +
              (int)CROS_FRAME_BYTES;
  buffered = app_audio_pcmbuff_length();
  if (buffered > max_bytes) {
    app_audio_pcmbuff_discard(
        (uint16_t)(buffered - (int)jitter_target_frames * (int)CROS_FRAME_BYTES));
    rx_drops++;
  }

  seq = data[1];
  gap = !rx_have_seq || seq != rx_expect_seq;
  if (gap) {
    /* Lost packet(s): reload predictor from header. */
    dec_state.index = (int8_t)data[2];
    if (dec_state.index < 0)
      dec_state.index = 0;
    if (dec_state.index > 88)
      dec_state.index = 88;
    dec_state.pred = (int16_t)(data[3] | (data[4] << 8));
    rx_resyncs++;
  }
  rx_have_seq = true;
  rx_expect_seq = (uint8_t)(seq + 1);

  ima_decode_block(&data[CROS_HDR_BYTES], decode_pcm);

  buffered = app_audio_pcmbuff_length();
  if (app_audio_pcmbuff_put((uint8_t *)decode_pcm, CROS_FRAME_BYTES) != 0) {
    app_audio_pcmbuff_discard((uint16_t)(CROS_FRAME_BYTES / 2));
    buffered = app_audio_pcmbuff_length();
    app_audio_pcmbuff_put((uint8_t *)decode_pcm, CROS_FRAME_BYTES);
  }
  cros_lat_note_recv_put((uint32_t)buffered);
  rx_pkts++;
  if ((rx_pkts & CROS_RX_LOG_MASK) == 0) {
    CROS_LOG_STAT(0, "[cros_tws] rx=%u underrun=%u resync=%u jitter=%u extra=%d",
          rx_pkts, underruns, rx_resyncs, jitter_target_frames,
          on_extra_media());
  }
  /* Rare event while quiet: underrun cliff. Use STAT so it stays UART-only
   * during quiet — SPP-teeing these mid-storm can feed ACL contention
   * (0.3.25 LEFT session logged thresholds 50…1750 over the air). */
  {
    static uint32_t underrun_armed = 50;
    if (underruns >= underrun_armed) {
      CROS_LOG_STAT(0, "[cros_tws] underrun threshold %u (rx=%u jitter=%u)",
            (unsigned)underrun_armed, (unsigned)rx_pkts,
            (unsigned)jitter_target_frames);
      underrun_armed += 100;
    }
    if (!on_extra_media()) {
      underrun_armed = 50;
    }
  }
}
