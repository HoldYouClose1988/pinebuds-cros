/***************************************************************************
 * Runtime BiCROS config (poor side / mix / EQ) over TOTA + IBRT.
 *
 * TOTA RX only copies the command and arms a timer — never call
 * tws_ctrl_send_cmd / heavy work from the SPP RX path (wedges BT).
 * Mix ceiling −12 dB (0 dB howls). Signed ints parsed by hand.
 ***************************************************************************/
#include "cros_cfg.h"

#include "cros_bt_log.h"
#include "cros_tws.h"

#include "app_ibrt_customif_cmd.h"
#include "cmsis_os.h"

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
  CROS_CFG_PKT_LEN = 4,
  CROS_CFG_CMD_MAX = 128,
};

extern void cros_sco_sidetone_set_gain_db(int db);
extern void cros_sco_reapply_shape(void);
/* Defined in cros_sco_probe.c — also declared in cros_sco_probe.h. */
extern int cros_sco_cfg_hold(void);

static uint8_t g_poor_is_right = (CROS_POOR_IS_RIGHT != 0);
static int8_t g_mix_db = -20;
static int8_t g_bass_db = 0;
static int8_t g_treble_db = 0;

static const int16_t k_db_to_q14[13] = {
    8192,  9192,  10313, 11572, 12983, 14568, 16384,
    18409, 20675, 23210, 26054, 29241, 32767,
};

static int32_t g_bass_q14 = 16384;
static int32_t g_treble_q14 = 16384;
static int32_t g_lp_state;

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
  CROS_LOG(0,
           "[cros_cfg] %s poor=%s mix=%ddB bass=%d treble=%d role=%s", why,
           g_poor_is_right ? "RIGHT" : "LEFT", (int)g_mix_db, (int)g_bass_db,
           (int)g_treble_db, cros_tws_is_poor_side() ? "POOR/TX" : "GOOD/RX");
}

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
  tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_CFG, pkt, CROS_CFG_PKT_LEN);
}

static void schedule_peer_sync(void) {
  if (cros_sco_cfg_hold()) {
    /* Local apply still done; peer picks up on next safe sync / re-enable. */
    CROS_LOG(0, "[cros_cfg] skip peer sync while SCO up");
    return;
  }
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)sync_to_peer_bt);
}

static void apply_audio_local(int poor_changed, int mix_changed, int eq_changed) {
  if (eq_changed) {
    refresh_eq_gain();
    g_lp_state = 0;
  }
  if (mix_changed) {
    cros_sco_sidetone_set_gain_db(g_mix_db);
  }
  if (poor_changed) {
    cros_sco_reapply_shape();
  }
}

void cros_cfg_apply_audio(void) {
  apply_audio_local(1, 1, 1);
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

static void apply_set(int poor, int mix, int bass, int treble, int have_poor,
                      int have_mix, int have_bass, int have_treble, int do_sync) {
  int poor_changed = 0;
  int mix_changed = 0;
  int eq_changed = 0;

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
  if (!poor_changed && !mix_changed && !eq_changed && !do_sync) {
    log_status("unchanged");
    return;
  }
  if (poor_changed || mix_changed || eq_changed) {
    apply_audio_local(poor_changed, mix_changed, eq_changed);
  }
  if (do_sync) {
    schedule_peer_sync();
  }
  log_status(do_sync ? "set" : "peer");
}

static void parse_set_args(char *args, int do_sync) {
  int poor = 0, mix = 0, bass = 0, treble = 0;
  int have_poor = 0, have_mix = 0, have_bass = 0, have_treble = 0;
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
    }
  }
  apply_set(poor, mix, bass, treble, have_poor, have_mix, have_bass, have_treble,
            do_sync);
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
  g_poor_is_right = (CROS_POOR_IS_RIGHT != 0);
  g_mix_db = -20;
  g_bass_db = 0;
  g_treble_db = 0;
  g_lp_state = 0;
  g_pending_len = 0;
  g_pending_busy = 0;
  refresh_eq_gain();
  cros_sco_sidetone_set_gain_db(g_mix_db);
  if (!g_cmd_timer) {
    g_cmd_timer = osTimerCreate(osTimer(CROS_CFG_CMD), osTimerOnce, NULL);
  }
  log_status("init");
}

int cros_cfg_poor_is_right(void) { return g_poor_is_right ? 1 : 0; }
int8_t cros_cfg_mix_db(void) { return g_mix_db; }
int8_t cros_cfg_bass_db(void) { return g_bass_db; }
int8_t cros_cfg_treble_db(void) { return g_treble_db; }

void cros_cfg_process_sco_pcm(uint8_t *buf, uint32_t len) {
  int16_t *s;
  uint32_t n;
  uint32_t i;
  if (!buf || len < 2) {
    return;
  }
  if (g_bass_db == 0 && g_treble_db == 0) {
    return;
  }
  if (cros_tws_is_poor_side()) {
    return;
  }
  s = (int16_t *)buf;
  n = len / sizeof(int16_t);
  for (i = 0; i < n; i++) {
    int32_t x = s[i];
    g_lp_state += (x - g_lp_state) >> 4;
    {
      int32_t low = g_lp_state;
      int32_t high = x - g_lp_state;
      int32_t y = (low * g_bass_q14 + high * g_treble_q14) >> 14;
      if (y > 32767) {
        y = 32767;
      } else if (y < -32768) {
        y = -32768;
      }
      s[i] = (int16_t)y;
    }
  }
}

void cros_cfg_on_peer(const uint8_t *data, uint16_t len) {
  int8_t mix;
  int8_t bass;
  int8_t treble;
  if (!data || len < CROS_CFG_PKT_LEN) {
    return;
  }
  mix = (int8_t)data[1];
  bass = (int8_t)data[2];
  treble = (int8_t)data[3];
  apply_set(data[0] ? 1 : 0, mix, bass, treble, 1, 1, 1, 1, 0);
}

void cros_cfg_on_tota_string(uint8_t *param, uint32_t param_len) {
  uint32_t n;
  if (!param || param_len == 0) {
    return;
  }
  /* Copy only — return to SPP RX immediately. */
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
