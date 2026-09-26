/***************************************************************************
 * C bridge to SCO voice helpers (app_bt_stream / audio manager).
 ***************************************************************************/
#include <stdbool.h>
#include <stdint.h>

enum {
  CROS_VOL_CTRL_SET = 0,
  CROS_TGT_VOL_MUTE = 0,
  CROS_TGT_VOL_15 = 15,
};

/* C++-mangled in app_bt_stream.cpp */
extern int bt_sco_player_forcemute(bool mic_mute, bool spk_mute);

/* These are extern "C" in the SDK headers / unmangled. */
extern "C" int app_audio_manager_ctrl_volume(int volume_ctrl,
                                            uint16_t volume_level);
extern "C" int app_bt_stream_volumeset(int8_t vol);
extern "C" uint8_t app_bt_stream_hfpvolume_get(void);

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
