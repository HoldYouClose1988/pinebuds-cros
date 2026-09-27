/***************************************************************************
 * Runtime BiCROS config (poor side / mix / EQ) over TOTA + IBRT.
 ***************************************************************************/
#include "cros_cfg.h"

#include "cros_bt_log.h"
#include "cros_tws.h"

#include "app_ibrt_customif_cmd.h"

#include <stdlib.h>
#include <string.h>

extern int tws_ctrl_send_cmd(uint32_t cmd_code, uint8_t *p_buff, uint16_t length);

#ifndef CROS_POOR_IS_RIGHT
#define CROS_POOR_IS_RIGHT 1
#endif

enum {
  CROS_MIX_DB_MIN = -30,
  CROS_MIX_DB_MAX = 0,
  CROS_EQ_DB_MIN = -6,
  CROS_EQ_DB_MAX = 6,
  CROS_CFG_PKT_LEN = 4,
};

extern void cros_sco_sidetone_set_gain_db(int db);
extern void cros_sco_reapply_shape(void);

static uint8_t g_poor_is_right = (CROS_POOR_IS_RIGHT != 0);
static int8_t g_mix_db = -20;
static int8_t g_bass_db = 0;
static int8_t g_treble_db = 0;

/* Q14 linear gains for bass/treble shelves (index = db + 6). */
static const int16_t k_db_to_q14[13] = {
    8192,  9192,  10313, 11572, 12983, 14568, 16384,
    18409, 20675, 23210, 26054, 29241, 32767,
};

static int32_t g_bass_q14 = 16384;
static int32_t g_treble_q14 = 16384;
static int32_t g_lp_state;

static int8_t clamp_i8(int v, int lo, int hi) {
  if (v < lo) {
    return (int8_t)lo;
  }
  if (v > hi) {
    return (int8_t)hi;
  }
  return (int8_t)v;
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

static void sync_to_peer(void) {
  uint8_t pkt[CROS_CFG_PKT_LEN];
  pkt[0] = g_poor_is_right;
  pkt[1] = (uint8_t)g_mix_db;
  pkt[2] = (uint8_t)g_bass_db;
  pkt[3] = (uint8_t)g_treble_db;
  tws_ctrl_send_cmd(APP_IBRT_CUSTOM_CMD_CROS_CFG, pkt, CROS_CFG_PKT_LEN);
}

static void apply_audio_local(void) {
  refresh_eq_gain();
  cros_sco_sidetone_set_gain_db(g_mix_db);
  cros_sco_reapply_shape();
}

void cros_cfg_apply_audio(void) { apply_audio_local(); }

void cros_cfg_init(void) {
  g_poor_is_right = (CROS_POOR_IS_RIGHT != 0);
  g_mix_db = -20;
  g_bass_db = 0;
  g_treble_db = 0;
  g_lp_state = 0;
  refresh_eq_gain();
  cros_sco_sidetone_set_gain_db(g_mix_db);
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
    /* ~250 Hz 1-pole LP at 16 kHz (alpha ≈ 1/16). */
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

static int parse_poor(const char *v) {
  if (!v) {
    return -1;
  }
  if (v[0] == 'R' || v[0] == 'r' || strcmp(v, "1") == 0) {
    return 1;
  }
  if (v[0] == 'L' || v[0] == 'l' || strcmp(v, "0") == 0) {
    return 0;
  }
  return -1;
}

static void apply_set(int poor, int mix, int bass, int treble, int have_poor,
                      int have_mix, int have_bass, int have_treble, int do_sync) {
  int changed = 0;
  if (have_poor && poor != (int)g_poor_is_right) {
    g_poor_is_right = poor ? 1 : 0;
    changed = 1;
  }
  if (have_mix) {
    int8_t m = clamp_i8(mix, CROS_MIX_DB_MIN, CROS_MIX_DB_MAX);
    /* Snap to even dB — HW sidetone step is 2 dB. */
    if (m & 1) {
      m = (int8_t)(m - 1);
    }
    if (m != g_mix_db) {
      g_mix_db = m;
      changed = 1;
    }
  }
  if (have_bass) {
    int8_t b = clamp_i8(bass, CROS_EQ_DB_MIN, CROS_EQ_DB_MAX);
    if (b != g_bass_db) {
      g_bass_db = b;
      changed = 1;
    }
  }
  if (have_treble) {
    int8_t t = clamp_i8(treble, CROS_EQ_DB_MIN, CROS_EQ_DB_MAX);
    if (t != g_treble_db) {
      g_treble_db = t;
      changed = 1;
    }
  }
  if (!changed && !do_sync) {
    log_status("unchanged");
    return;
  }
  apply_audio_local();
  if (do_sync) {
    sync_to_peer();
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
      mix = atoi(val);
      have_mix = 1;
    } else if (strcmp(key, "bass") == 0) {
      bass = atoi(val);
      have_bass = 1;
    } else if (strcmp(key, "treble") == 0) {
      treble = atoi(val);
      have_treble = 1;
    }
  }
  apply_set(poor, mix, bass, treble, have_poor, have_mix, have_bass, have_treble,
            do_sync);
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
  char buf[160];
  uint32_t n;
  char *p;
  if (!param || param_len == 0) {
    return;
  }
  n = param_len;
  if (n >= sizeof(buf)) {
    n = sizeof(buf) - 1;
  }
  memcpy(buf, param, n);
  buf[n] = '\0';
  /* Trim trailing CR/LF/space. */
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ' ||
                   buf[n - 1] == '\t')) {
    buf[--n] = '\0';
  }
  p = buf;
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
}
