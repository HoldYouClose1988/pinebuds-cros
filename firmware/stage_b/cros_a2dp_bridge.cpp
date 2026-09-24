/* C wrappers for C++ A2DP helpers used by cros_tws.c */
#include "app_a2dp.h"
#include "btapp.h"

extern void a2dp_suspend_music_force(void);
extern void btapp_a2dp_suspend_music(enum BT_DEVICE_ID_T stream_id);

extern "C" int cros_a2dp_music_ongoing(void) {
  return a2dp_is_music_ongoing() ? 1 : 0;
}

extern "C" void cros_a2dp_suspend_for_cros(void) {
  /* Pause the phone (AVRCP) and stop local SBC render. */
  btapp_a2dp_suspend_music(BT_DEVICE_ID_1);
  a2dp_suspend_music_force();
}
