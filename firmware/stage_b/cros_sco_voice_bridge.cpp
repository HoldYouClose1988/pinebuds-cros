/***************************************************************************
 * C bridge to C++ bt_sco_player_forcemute (app_bt_stream.cpp).
 ***************************************************************************/
#include <stdbool.h>

extern int bt_sco_player_forcemute(bool mic_mute, bool spk_mute);

extern "C" int cros_sco_forcemute(int mic_mute, int spk_mute) {
  return bt_sco_player_forcemute(mic_mute != 0, spk_mute != 0);
}
