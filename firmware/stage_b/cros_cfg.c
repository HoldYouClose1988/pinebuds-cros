/***************************************************************************
 * Runtime BiCROS config (poor / mix / EQ / sco / a2dp / noise) over TOTA +
 * IBRT.
 *
 * TOTA RX only copies the command and arms a timer — never call
 * tws_ctrl_send_cmd / heavy work from the SPP RX path (wedges BT).
 * Mix ceiling −12 dB (0 dB howls). Signed ints parsed by hand.
 *
 * vol= / sco= 0..15 → HFP/SCO DAC gain on good ear (NOT music).
 *   Ear: 13 too hot; 11 better; 8 + noise=3 preferred.
 * a2dp=0..15 → A2DP music volume (separate NV / DAC when music plays).
 * noise=0..5 → soft gate + mild HF rolloff on good-ear SCO (link hiss).
 *
 * v0.3.63: persist knobs (incl. poor side) in BES NV extension
 * system_info.flag_value[8] — survives case/reboot; both buds save on Apply
 * and on peer sync RX. Boot logs "[cros_cfg] NV load …" or defaults.
 ***************************************************************************/
#include "cros_cfg.h"

#include "cros_bt_log.h"
#include "cros_cue.h"
#include "cros_tws.h"

#include "app_ibrt_customif_cmd.h"
#include "app_tws_if.h"
#include "cmsis_os.h"

#if defined(NEW_NV_RECORD_ENABLED)
#include "nvrecord_extension.h"
#endif

#include <string.h>

extern int tws_ctrl_send_cmd(uint32_t cmd_code, uint8_t *p_buff, uint16_t length);
extern int app_bt_start_custom_function_in_bt_thread(uint32_t param0,
                                                     uint32_t param1,
                                                     uint32_t func);

#ifndef CROS_POOR_IS_RIGHT
#define CROS_POOR_IS_RIGHT 1
#endif

enum {
  CROS_MIX_DB_MIN = -30,
  CROS_MIX_DB_MAX = -12,
  CROS_EQ_DB_MIN = -6,
  CROS_EQ_DB_MAX = 6,
  CROS_VOL_MIN = 0,
  CROS_VOL_MAX = 15,
  /* Ear 2026-09-27: 8 + noise 3 preferred (11 still a bit hot). */
  CROS_VOL_DEFAULT = 8,
  CROS_A2DP_DEFAULT = 12,
  CROS_NOISE_MIN = 0,
  CROS_NOISE_MAX = 5,
  CROS_NOISE_DEFAULT = 3,
  CROS_CFG_PKT_LEN = 7,
  CROS_CFG_PKT_LEN_V2 = 6, /* vol+noise, no a2dp */
  CROS_CFG_PKT_LEN_LEGACY = 4,
  CROS_CFG_CMD_MAX = 128,
  /* Packed into unused system_info.flag_value[8] (BES NV extension). */
  CROS_NV_MAGIC = 0xC7,
};

extern void cros_sco_sidetone_set_gain_db(int db);
extern void cros_sco_reapply_shape(void);
extern int cros_sco_set_hfp_volume(int level);
extern int cros_sco_get_hfp_volume(void);
extern int cros_a2dp_set_volume(int level);
extern int cros_a2dp_get_volume(void);
extern int cros_sco_cfg_hold(void);

static uint8_t g_poor_is_right = (CROS_POOR_IS_RIGHT != 0);
static int8_t g_mix_db = -20;
static int8_t g_bass_db = 0;
static int8_t g_treble_db = 0;
static uint8_t g_vol = CROS_VOL_DEFAULT;
static uint8_t g_a2dp = CROS_A2DP_DEFAULT;
static uint8_t g_noise = CROS_NOISE_DEFAULT;
static uint8_t g_nv_loaded; /* 1 if boot restored from flash */

static const int16_t k_db_to_q14[13] = {
    8192,  9192,  10313, 11572, 12983, 14568, 16384,
    18409, 20675, 23210, 26054, 29241, 32767,
};

static int32_t g_bass_q14 = 16384;
static int32_t g_treble_q14 = 16384;
static int32_t g_lp_state;
static int32_t g_nf_env;
static int32_t g_nf_lp;

static char g_pending_cmd[CROS_CFG_CMD_MAX];
static volatile uint8_t g_pending_len;
static volatile uint8_t g_pending_busy;
static osTimerId g_cmd_timer;

static int8_t clamp_i8(int v, int lo, int hi) {
  if (v < lo) {
    return (int8_t)lo;
  }
  if (v > hi) {
    return (int8_t)hi;
  }
  return (int8_t)v;
}

static uint8_t clamp_u8(int v, int lo, int hi) {
  if (v < lo) {
    return (uint8_t)lo;
  }
  if (v > hi) {
    return (uint8_t)hi;
  }
  return (uint8_t)v;
}

static int8_t snap_mix_db(int v) {
  int8_t m = clamp_i8(v, CROS_MIX_DB_MIN, CROS_MIX_DB_MAX);
  if (m & 1) {
    m = (int8_t)(m - 1);
  }
  if (m > CROS_MIX_DB_MAX) {
    m = (int8_t)CROS_MIX_DB_MAX;
  }
  return m;
}

static int parse_int(const char *s, int *ok) {
  int neg = 0;
  int v = 0;
  int digits = 0;
  if (!s) {
    if (ok) {
      *ok = 0;
    }
    return 0;
  }
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  if (*s == '-') {
    neg = 1;
    s++;
  } else if (*s == '+') {
    s++;
  }
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    digits = 1;
    s++;
  }
  if (ok) {
    *ok = digits;
  }
  return neg ? -v : v;
}

static int16_t db_to_q14(int8_t db) {
  if (db < CROS_EQ_DB_MIN) {
    db = CROS_EQ_DB_MIN;
  }
  if (db > CROS_EQ_DB_MAX) {
    db = CROS_EQ_DB_MAX;
  }
  return k_db_to_q14[db - CROS_EQ_DB_MIN];
}

static void refresh_eq_gain(void) {
  g_bass_q14 = db_to_q14(g_bass_db);
  g_treble_q14 = db_to_q14(g_treble_db);
}

static void log_status(const char *why) {
  CROS_LOG_ACK(0,
               "[cros_cfg] %s poor=%s mix=%ddB bass=%d treble=%d sco=%u "
               "a2dp=%u noise=%u role=%s",
               why, g_poor_is_right ? "RIGHT" : "LEFT", (int)g_mix_db,
               (int)g_bass_db, (int)g_treble_db, (unsigned)g_vol,
               (unsigned)g_a2dp, (unsigned)g_noise,
               cros_tws_is_poor_side() ? "POOR/TX" : "GOOD/RX");
}

#if defined(NEW_NV_RECORD_ENABLED)
/*
 * flag_value[8] layout (magic CROS_NV_MAGIC):
 *  [0] magic
 *  [1] poor_is_right
 *  [2] mix_db (int8)
 *  [3] bass_db (int8)
 *  [4] treble_db (int8)
 *  [5] sco/vol
 *  [6] a2dp
 *  [7] noise
 */
static int cros_cfg_nv_load(void) {
  NV_EXTENSION_RECORD_T *ext;
  uint8_t *f;
  int8_t mix, bass, treble;

  ext = nv_record_get_extension_entry_ptr();
  if (!ext) {
    return 0;
  }
  f = ext->system_info.flag_value;
  if (f[0] != (uint8_t)CROS_NV_MAGIC) {
    return 0;
  }
  g_poor_is_right = f[1] ? 1 : 0;
  mix = (int8_t)f[2];
  bass = (int8_t)f[3];
  treble = (int8_t)f[4];
  g_mix_db = snap_mix_db((int)mix);
  g_bass_db = clamp_i8((int)bass, CROS_EQ_DB_MIN, CROS_EQ_DB_MAX);
  g_treble_db = clamp_i8((int)treble, CROS_EQ_DB_MIN, CROS_EQ_DB_MAX);
  g_vol = clamp_u8((int)f[5], CROS_VOL_MIN, CROS_VOL_MAX);
  g_a2dp = clamp_u8((int)f[6], CROS_VOL_MIN, CROS_VOL_MAX);
  g_noise = clamp_u8((int)f[7], CROS_NOISE_MIN, CROS_NOISE_MAX);
  return 1;
}

static void cros_cfg_nv_save(const char *why) {
  NV_EXTENSION_RECORD_T *ext;
  uint8_t *f;
  uint32_t lock;

  ext = nv_record_get_extension_entry_ptr();
  if (!ext) {
    CROS_LOG(0, "[cros_cfg] NV save skip — no extension (%s)", why ? why : "?");
    return;
  }
  lock = nv_record_pre_write_operation();
  f = ext->system_info.flag_value;
  f[0] = (uint8_t)CROS_NV_MAGIC;
  f[1] = g_poor_is_right ? 1 : 0;
  f[2] = (uint8_t)(int8_t)g_mix_db;
  f[3] = (uint8_t)(int8_t)g_bass_db;
  f[4] = (uint8_t)(int8_t)g_treble_db;
  f[5] = g_vol;
  f[6] = g_a2dp;
  f[7] = g_noise;
  nv_record_extension_update();
  nv_record_post_write_operation(lock);
  nv_record_flash_flush();
  CROS_LOG_ACK(0,
               "[cros_cfg] NV save (%s) poor=%s mix=%ddB bass=%d treble=%d "
               "sco=%u a2dp=%u noise=%u",
               why ? why : "?", g_poor_is_right ? "RIGHT" : "LEFT",
               (int)g_mix_db, (int)g_bass_db, (int)g_treble_db, (unsigned)g_vol,
               (unsigned)g_a2dp, (unsigned)g_noise);
}
#else
static int cros_cfg_nv_load(void) { return 0; }
static void cros_cfg_nv_save(const char *why) {
  (void)why;
  CROS_LOG(0, "[cros_cfg] NV save unavailable (NEW_NV_RECORD off)");
}
#endif

static void sync_to_peer_bt(void *a, void *b) {
  uint8_t pkt[CROS_CFG_PKT_LEN];
  (void)a;
  (void)b;
  if (cros_sco_cfg_hold()) {
    CROS_LOG(0, "[cros_cfg] peer sync deferred — SCO hold");
    return;
  }
  pkt[0] = g_poor_is_right;
  pkt[1] = (uint8_t)(int8_t)g_mix_db;
  pkt[2] = (uint8_t)(int8_t)g_bass_db;
  pkt[3] = (uint8_t)(int8_t)g_treble_db;
  pkt[4] = g_vol;
  pkt[5] = g_noise;
  pkt[6] = g_a2dp;
  tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_CFG, pkt, CROS_CFG_PKT_LEN);
}

static void schedule_peer_sync(void) {
  if (cros_sco_cfg_hold()) {
    CROS_LOG(0, "[cros_cfg] skip peer sync while SCO up");
    return;
  }
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)sync_to_peer_bt);
}

static void apply_audio_local(int poor_changed, int mix_changed, int eq_changed,
                              int vol_changed, int a2dp_changed) {
  if (eq_changed) {
    refresh_eq_gain();
    g_lp_state = 0;
  }
  if (mix_changed) {
    cros_sco_sidetone_set_gain_db(g_mix_db);
  }
  if (poor_changed) {
    cros_sco_reapply_shape();
  } else if (vol_changed && !cros_tws_is_poor_side()) {
    cros_sco_set_hfp_volume((int)g_vol);
  }
  if (a2dp_changed) {
    cros_a2dp_set_volume((int)g_a2dp);
  }
}

void cros_cfg_apply_audio(void) {
  apply_audio_local(1, 1, 1, 1, 1);
}

static int parse_poor(const char *v) {
  if (!v) {
    return -1;
  }
  if (v[0] == 'R' || v[0] == 'r' || (v[0] == '1' && v[1] == '\0')) {
    return 1;
  }
  if (v[0] == 'L' || v[0] == 'l' || (v[0] == '0' && v[1] == '\0')) {
    return 0;
  }
  return -1;
}

static void apply_set(int poor, int mix, int bass, int treble, int vol,
                      int noise, int a2dp, int have_poor, int have_mix,
                      int have_bass, int have_treble, int have_vol,
                      int have_noise, int have_a2dp, int do_sync) {
  int poor_changed = 0;
  int mix_changed = 0;
  int eq_changed = 0;
  int vol_changed = 0;
  int noise_changed = 0;
  int a2dp_changed = 0;

  if (have_poor && poor != (int)g_poor_is_right) {
    g_poor_is_right = poor ? 1 : 0;
    poor_changed = 1;
  }
  if (have_mix) {
    int8_t m = snap_mix_db(mix);
    if (m != g_mix_db) {
      g_mix_db = m;
      mix_changed = 1;
    }
  }
  if (have_bass) {
    int8_t b = clamp_i8(bass, CROS_EQ_DB_MIN, CROS_EQ_DB_MAX);
    if (b != g_bass_db) {
      g_bass_db = b;
      eq_changed = 1;
    }
  }
  if (have_treble) {
    int8_t t = clamp_i8(treble, CROS_EQ_DB_MIN, CROS_EQ_DB_MAX);
    if (t != g_treble_db) {
      g_treble_db = t;
      eq_changed = 1;
    }
  }
  if (have_vol) {
    uint8_t v = clamp_u8(vol, CROS_VOL_MIN, CROS_VOL_MAX);
    if (v != g_vol) {
      g_vol = v;
      vol_changed = 1;
    }
  }
  if (have_noise) {
    uint8_t n = clamp_u8(noise, CROS_NOISE_MIN, CROS_NOISE_MAX);
    if (n != g_noise) {
      g_noise = n;
      noise_changed = 1;
      g_nf_env = 0;
      g_nf_lp = 0;
    }
  }
  if (have_a2dp) {
    uint8_t a = clamp_u8(a2dp, CROS_VOL_MIN, CROS_VOL_MAX);
    if (a != g_a2dp) {
      g_a2dp = a;
      a2dp_changed = 1;
    }
  }
  if (!poor_changed && !mix_changed && !eq_changed && !vol_changed &&
      !noise_changed && !a2dp_changed) {
    /* Phone Apply with same knobs still re-commits NV (confirm save path). */
    if (do_sync) {
      cros_cfg_nv_save("unchanged");
    }
    log_status("unchanged");
    return;
  }
  if (poor_changed || mix_changed || eq_changed || vol_changed ||
      a2dp_changed) {
    apply_audio_local(poor_changed, mix_changed, eq_changed, vol_changed,
                      a2dp_changed);
  }
  /* Persist on phone Apply and on peer RX so both buds keep poor/mix/…. */
  cros_cfg_nv_save(do_sync ? "set" : "peer");
  if (do_sync) {
    schedule_peer_sync();
  }
  log_status(do_sync ? "set" : "peer");
}

static void parse_set_args(char *args, int do_sync) {
  int poor = 0, mix = 0, bass = 0, treble = 0, vol = 0, noise = 0, a2dp = 0;
  int have_poor = 0, have_mix = 0, have_bass = 0, have_treble = 0;
  int have_vol = 0, have_noise = 0, have_a2dp = 0;
  char *p = args;
  while (p && *p) {
    char *tok;
    char *eq;
    char *key;
    char *val;
    int ok = 0;
    int iv;
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    if (!*p) {
      break;
    }
    tok = p;
    while (*p && *p != ' ' && *p != '\t') {
      p++;
    }
    if (*p) {
      *p++ = '\0';
    }
    eq = strchr(tok, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    key = tok;
    val = eq + 1;
    if (strcmp(key, "poor") == 0) {
      int pr = parse_poor(val);
      if (pr >= 0) {
        poor = pr;
        have_poor = 1;
      }
    } else if (strcmp(key, "mix") == 0) {
      iv = parse_int(val, &ok);
      if (ok) {
        mix = iv;
        have_mix = 1;
      }
    } else if (strcmp(key, "bass") == 0) {
      iv = parse_int(val, &ok);
      if (ok) {
        bass = iv;
        have_bass = 1;
      }
    } else if (strcmp(key, "treble") == 0) {
      iv = parse_int(val, &ok);
      if (ok) {
        treble = iv;
        have_treble = 1;
      }
    } else if (strcmp(key, "vol") == 0 || strcmp(key, "sco") == 0) {
      iv = parse_int(val, &ok);
      if (ok) {
        vol = iv;
        have_vol = 1;
      }
    } else if (strcmp(key, "a2dp") == 0 || strcmp(key, "music") == 0) {
      iv = parse_int(val, &ok);
      if (ok) {
        a2dp = iv;
        have_a2dp = 1;
      }
    } else if (strcmp(key, "noise") == 0) {
      iv = parse_int(val, &ok);
      if (ok) {
        noise = iv;
        have_noise = 1;
      }
    }
  }
  apply_set(poor, mix, bass, treble, vol, noise, a2dp, have_poor, have_mix,
            have_bass, have_treble, have_vol, have_noise, have_a2dp, do_sync);
}

static void cros_cfg_run_pending(void) {
  char local[CROS_CFG_CMD_MAX];
  uint8_t n;
  char *p;

  n = g_pending_len;
  if (n == 0 || n >= CROS_CFG_CMD_MAX) {
    g_pending_busy = 0;
    return;
  }
  memcpy(local, g_pending_cmd, n);
  local[n] = '\0';
  g_pending_len = 0;
  g_pending_busy = 0;

  p = local;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (strncmp(p, "cros get", 8) == 0) {
    log_status("get");
    return;
  }
  if (strncmp(p, "cros status", 11) == 0) {
    /* One-shot status for the phone UI — no continuous log tee required.
     * phone= = physical side of THIS bud (the one answering SPP = usually
     * IBRT master). App warns if poor is set to that same side. */
    CROS_LOG_ACK(0,
                 "[cros_cfg] status enabled=%u poor=%s mix=%ddB bass=%d "
                 "treble=%d sco=%u a2dp=%u noise=%u phone=%s fw=0.3.65",
                 cros_tws_is_enabled() ? 1u : 0u,
                 g_poor_is_right ? "RIGHT" : "LEFT", (int)g_mix_db,
                 (int)g_bass_db, (int)g_treble_db, (unsigned)g_vol,
                 (unsigned)g_a2dp, (unsigned)g_noise,
                 app_tws_is_right_side() ? "RIGHT" : "LEFT");
    return;
  }
  if (strncmp(p, "cros set", 8) == 0) {
    p += 8;
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    parse_set_args(p, 1);
    return;
  }
  CROS_LOG(0, "[cros_cfg] ignore cmd");
}

static void cros_cfg_cmd_timer_cb(void const *arg) {
  (void)arg;
  cros_cfg_run_pending();
}
osTimerDef(CROS_CFG_CMD, cros_cfg_cmd_timer_cb);

void cros_cfg_init(void) {
  int cur_a2dp;
  g_poor_is_right = (CROS_POOR_IS_RIGHT != 0);
  g_mix_db = -20;
  g_bass_db = 0;
  g_treble_db = 0;
  g_vol = CROS_VOL_DEFAULT;
  g_noise = CROS_NOISE_DEFAULT;
  cur_a2dp = cros_a2dp_get_volume();
  if (cur_a2dp >= CROS_VOL_MIN && cur_a2dp <= CROS_VOL_MAX) {
    g_a2dp = (uint8_t)cur_a2dp;
  } else {
    g_a2dp = CROS_A2DP_DEFAULT;
  }
  g_nv_loaded = 0;
  if (cros_cfg_nv_load()) {
    g_nv_loaded = 1;
  }
  g_lp_state = 0;
  g_nf_env = 0;
  g_nf_lp = 0;
  g_pending_len = 0;
  g_pending_busy = 0;
  refresh_eq_gain();
  cros_sco_sidetone_set_gain_db(g_mix_db);
  if (!g_cmd_timer) {
    g_cmd_timer = osTimerCreate(osTimer(CROS_CFG_CMD), osTimerOnce, NULL);
  }
  if (g_nv_loaded) {
    log_status("NV load");
  } else {
    log_status("init defaults");
  }
}

int cros_cfg_poor_is_right(void) { return g_poor_is_right ? 1 : 0; }
int8_t cros_cfg_mix_db(void) { return g_mix_db; }
int8_t cros_cfg_bass_db(void) { return g_bass_db; }
int8_t cros_cfg_treble_db(void) { return g_treble_db; }
int cros_cfg_vol(void) { return (int)g_vol; }
int cros_cfg_a2dp(void) { return (int)g_a2dp; }
int cros_cfg_noise(void) { return (int)g_noise; }

void cros_cfg_on_abs_volume(int tgt_level) {
  int user;
  uint8_t next;

  if (!cros_tws_is_enabled()) {
    return;
  }
  /*
   * a2dp_volume_set() passes TGT_VOLUME_LEVEL_* (MUTE=1, LEVEL_0=2 … 15=17).
   * Our sco= / a2dp= UI is 0..15 (same mapping as hfp_volume_get: tgt-2).
   * Mirror into both: rocker drives BiCROS SCO now; a2dp= stays aligned for
   * music after CROS off (NV write is in a2dp_volume_local_set caller).
   */
  if (tgt_level <= 1) {
    user = 0;
  } else {
    user = tgt_level - 2;
  }
  if (user < (int)CROS_VOL_MIN) {
    user = (int)CROS_VOL_MIN;
  }
  if (user > (int)CROS_VOL_MAX) {
    user = (int)CROS_VOL_MAX;
  }
  next = (uint8_t)user;
  g_vol = next;
  g_a2dp = next;
  if (!cros_tws_is_poor_side()) {
    int applied = cros_sco_set_hfp_volume((int)g_vol);
    int now = cros_sco_get_hfp_volume();
    CROS_LOG_ACK(0, "[cros_cfg] absvol apply sco=%d dac=%d hfp_vol=%d",
                 (int)g_vol, applied, now);
  }
  cros_cfg_nv_save("absvol");
  log_status("absvol");
}

void cros_cfg_process_sco_pcm(uint8_t *buf, uint32_t len) {
  int16_t *s;
  uint32_t n;
  uint32_t i;
  int do_eq;
  int noise;
  int cue;
  if (!buf || len < 2) {
    return;
  }
  if (cros_tws_is_poor_side()) {
    return;
  }
  do_eq = (g_bass_db != 0 || g_treble_db != 0);
  noise = (int)g_noise;
  cue = cros_cue_sco_busy();
  if (!do_eq && noise == 0 && !cue) {
    return;
  }
  s = (int16_t *)buf;
  n = len / sizeof(int16_t);
  for (i = 0; i < n; i++) {
    int32_t x = s[i];
    int32_t y = x;

    if (do_eq) {
      g_lp_state += (x - g_lp_state) >> 4;
      {
        int32_t low = g_lp_state;
        int32_t high = x - g_lp_state;
        y = (low * g_bass_q14 + high * g_treble_q14) >> 14;
      }
    }

    if (noise > 0) {
      int32_t ax = y >= 0 ? y : -y;
      int32_t thresh;
      int32_t floor_q14;
      int32_t gain_q14;
      /* Envelope ~1–2 ms at 16 kHz. */
      g_nf_env += (ax - g_nf_env) >> 3;
      /* Soft gate: higher noise → higher threshold + lower floor. */
      thresh = 120 + noise * 140;
      floor_q14 = 16384 - noise * 2200;
      if (floor_q14 < 2048) {
        floor_q14 = 2048;
      }
      if (g_nf_env < thresh) {
        gain_q14 = (g_nf_env << 14) / (thresh ? thresh : 1);
        if (gain_q14 < floor_q14) {
          gain_q14 = floor_q14;
        }
        y = (y * gain_q14) >> 14;
      }
      /* Mild HF rolloff for hiss (stronger with noise level). */
      if (noise >= 2) {
        int shift = (noise >= 4) ? 2 : 3;
        g_nf_lp += (y - g_nf_lp) >> shift;
        {
          int32_t blend = noise * 2048; /* Q14, max ~0.625 at noise=5 */
          if (blend > 10240) {
            blend = 10240;
          }
          y = y + (((g_nf_lp - y) * blend) >> 14);
        }
      }
    }

    if (y > 32767) {
      y = 32767;
    } else if (y < -32768) {
      y = -32768;
    }
    s[i] = (int16_t)y;
  }
  /* After EQ/noise so the cue is not soft-gated away. */
  cros_cue_mix_sco_pcm(buf, len);
}

void cros_cfg_on_peer(const uint8_t *data, uint16_t len) {
  int8_t mix;
  int8_t bass;
  int8_t treble;
  int have_vol = 0;
  int have_noise = 0;
  int have_a2dp = 0;
  int vol = 0;
  int noise = 0;
  int a2dp = 0;
  if (!data || len < CROS_CFG_PKT_LEN_LEGACY) {
    return;
  }
  mix = (int8_t)data[1];
  bass = (int8_t)data[2];
  treble = (int8_t)data[3];
  if (len >= CROS_CFG_PKT_LEN_V2) {
    vol = data[4];
    noise = data[5];
    have_vol = 1;
    have_noise = 1;
  }
  if (len >= CROS_CFG_PKT_LEN) {
    a2dp = data[6];
    have_a2dp = 1;
  }
  apply_set(data[0] ? 1 : 0, mix, bass, treble, vol, noise, a2dp, 1, 1, 1, 1,
            have_vol, have_noise, have_a2dp, 0);
}

void cros_cfg_on_tota_string(uint8_t *param, uint32_t param_len) {
  uint32_t n;
  if (!param || param_len == 0) {
    return;
  }
  n = param_len;
  if (n >= CROS_CFG_CMD_MAX) {
    n = CROS_CFG_CMD_MAX - 1;
  }
  memcpy(g_pending_cmd, param, n);
  while (n > 0 && (g_pending_cmd[n - 1] == '\n' || g_pending_cmd[n - 1] == '\r' ||
                   g_pending_cmd[n - 1] == ' ' || g_pending_cmd[n - 1] == '\t')) {
    n--;
  }
  g_pending_cmd[n] = '\0';
  g_pending_len = (uint8_t)n;
  g_pending_busy = 1;
  if (!g_cmd_timer) {
    g_cmd_timer = osTimerCreate(osTimer(CROS_CFG_CMD), osTimerOnce, NULL);
  }
  if (g_cmd_timer) {
    osTimerStop(g_cmd_timer);
    osTimerStart(g_cmd_timer, 10);
  }
}
