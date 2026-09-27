/***************************************************************************
 * C bridge to SCO / A2DP volume helpers (app_bt_stream / audio manager /
 * sidetone).
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
  /* app_bt_stream.h bit flags — avoid pulling that C++ header here. */
  CROS_STREAM_HFP_PCM = (1u << 0),
  CROS_STREAM_A2DP_SBC = (1u << 3),
};

struct btdevice_volume {
  int8_t a2dp_vol;
  int8_t hfp_vol;
};

/* C++-mangled in app_bt_stream.cpp */
extern int bt_sco_player_forcemute(bool mic_mute, bool spk_mute);

extern "C" int app_audio_manager_ctrl_volume(int volume_ctrl,
                                            uint16_t volume_level);
extern "C" int app_bt_stream_volumeset(int8_t vol);
extern "C" uint8_t app_bt_stream_hfpvolume_get(void);
extern "C" uint8_t app_bt_stream_a2dpvolume_get(void);
extern "C" bool app_bt_stream_isrun(uint16_t player);
extern "C" struct btdevice_volume *app_bt_stream_volume_get_ptr(void);
extern "C" void nv_record_btdevicevolume_set_a2dp_vol(
    struct btdevice_volume *device_vol, int8_t vol);
extern "C" void nv_record_btdevicevolume_set_hfp_vol(
    struct btdevice_volume *device_vol, int8_t vol);
extern "C" void btapp_a2dp_report_speak_gain(void);
extern "C" void hal_codec_sidetone_enable(void);
extern "C" void hal_codec_sidetone_disable(void);
/* Patched HAL (0010); weak so older trees still link. */
extern "C" void hal_codec_sidetone_set_gain_db(int gain_db)
    __attribute__((weak));

/* Mirrored in app_bt_stream.cpp — keep get() / NV in sync. */
extern struct btdevice_volume current_btdevice_volume;

static int g_sidetone_on;
static int g_sidetone_db = -20;

static int clamp_vol(int level) {
  if (level < CROS_TGT_VOL_MUTE) {
    return CROS_TGT_VOL_MUTE;
  }
  if (level > CROS_TGT_VOL_15) {
    return CROS_TGT_VOL_15;
  }
  return level;
}

extern "C" int cros_sco_forcemute(int mic_mute, int spk_mute) {
  return bt_sco_player_forcemute(mic_mute != 0, spk_mute != 0);
}

/*
 * HFP/SCO DAC gain (BiCROS contralateral playback) — not music volume.
 *
 * Must update hfp_vol NV / current_btdevice_volume, not only stream_cfg.vol.
 * AbsVol / TOTA used to call volumeset alone; stock paths (bud keys, SCO
 * swap) re-apply from hfp_vol and snapped the DAC back — rocker looked like
 * a no-op in the ear even though [cros_cfg] absvol sco=N logged.
 *
 * Units: UI / g_vol 0..15 as stream_cfg.vol indices (ear-tuned; same as
 * prior BiCROS defaults). Not stock HFP speaker-gain (+2 TGT) — peer SCO
 * has no AG Absolute Volume.
 */
extern "C" int cros_sco_set_hfp_volume(int level) {
  struct btdevice_volume *vp;

  level = clamp_vol(level);
  vp = app_bt_stream_volume_get_ptr();
  if (vp) {
    nv_record_btdevicevolume_set_hfp_vol(vp, (int8_t)level);
  }
  current_btdevice_volume.hfp_vol = (int8_t)level;

  /* Mailbox marshals to audio thread when called from BT/AVRCP. */
  app_audio_manager_ctrl_volume(CROS_VOL_CTRL_SET, (uint16_t)level);
  app_bt_stream_volumeset((int8_t)level);
  return level;
}

extern "C" int cros_sco_get_hfp_volume(void) {
  return (int)app_bt_stream_hfpvolume_get();
}

/*
 * A2DP / music volume. Store in NV always; apply DAC only when A2DP is the
 * active player (never while HFP SCO/BiCROS owns the DAC).
 */
extern "C" int cros_a2dp_set_volume(int level) {
  struct btdevice_volume *vp;

  level = clamp_vol(level);
  vp = app_bt_stream_volume_get_ptr();
  if (vp) {
    nv_record_btdevicevolume_set_a2dp_vol(vp, (int8_t)level);
  }
  current_btdevice_volume.a2dp_vol = (int8_t)level;

  if (app_bt_stream_isrun((uint16_t)CROS_STREAM_A2DP_SBC) &&
      !app_bt_stream_isrun((uint16_t)CROS_STREAM_HFP_PCM)) {
    app_bt_stream_volumeset((int8_t)level);
  }
  btapp_a2dp_report_speak_gain();
  return level;
}

extern "C" int cros_a2dp_get_volume(void) {
  return (int)app_bt_stream_a2dpvolume_get();
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
