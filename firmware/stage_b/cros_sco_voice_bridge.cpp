/***************************************************************************
 * C bridge to SCO voice helpers (app_bt_stream / audio manager / sidetone).
 ***************************************************************************/
#include <stdbool.h>
#include <stdint.h>

enum {
  CROS_VOL_CTRL_SET = 0,
  CROS_TGT_VOL_MUTE = 0,
  CROS_TGT_VOL_15 = 15,
  CROS_SIDETONE_DB_MIN = -30,
  /* Match cros_cfg ceiling — never drive HW sidetone to 0 dB. */
  CROS_SIDETONE_DB_MAX = -12,
};

/* C++-mangled in app_bt_stream.cpp */
extern int bt_sco_player_forcemute(bool mic_mute, bool spk_mute);

extern "C" int app_audio_manager_ctrl_volume(int volume_ctrl,
                                            uint16_t volume_level);
extern "C" int app_bt_stream_volumeset(int8_t vol);
extern "C" uint8_t app_bt_stream_hfpvolume_get(void);
extern "C" void hal_codec_sidetone_enable(void);
extern "C" void hal_codec_sidetone_disable(void);
/* Patched HAL (0010); weak so older trees still link. */
extern "C" void hal_codec_sidetone_set_gain_db(int gain_db)
    __attribute__((weak));

static int g_sidetone_on;
static int g_sidetone_db = -20;

extern "C" int cros_sco_forcemute(int mic_mute, int spk_mute) {
  return bt_sco_player_forcemute(mic_mute != 0, spk_mute != 0);
}

extern "C" int cros_sco_set_hfp_volume(int level) {
  if (level < CROS_TGT_VOL_MUTE) {
    level = CROS_TGT_VOL_MUTE;
  }
  if (level > CROS_TGT_VOL_15) {
    level = CROS_TGT_VOL_15;
  }
  app_audio_manager_ctrl_volume(CROS_VOL_CTRL_SET, (uint16_t)level);
  app_bt_stream_volumeset((int8_t)level);
  return level;
}

extern "C" int cros_sco_get_hfp_volume(void) {
  return (int)app_bt_stream_hfpvolume_get();
}

extern "C" void cros_sco_sidetone_set_gain_db(int db) {
  if (db < CROS_SIDETONE_DB_MIN) {
    db = CROS_SIDETONE_DB_MIN;
  }
  if (db > CROS_SIDETONE_DB_MAX) {
    db = CROS_SIDETONE_DB_MAX;
  }
  /* HW step is 2 dB. */
  if (db & 1) {
    db -= 1;
  }
  g_sidetone_db = db;
  /*
   * Update gain register in place while sidetone stays enabled.
   * Do NOT disable→enable: that momentarily opens a howling window.
   */
  if (hal_codec_sidetone_set_gain_db) {
    hal_codec_sidetone_set_gain_db(db);
  }
}

/* HW codec sidetone: local mic → local speaker (independent of SCO TX mute). */
extern "C" void cros_sco_sidetone_set(int on) {
  if (on) {
    if (hal_codec_sidetone_set_gain_db) {
      hal_codec_sidetone_set_gain_db(g_sidetone_db);
    }
    hal_codec_sidetone_enable();
    g_sidetone_on = 1;
  } else {
    hal_codec_sidetone_disable();
    g_sidetone_on = 0;
  }
}
