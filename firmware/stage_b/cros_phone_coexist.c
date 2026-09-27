/***************************************************************************
 * iOS / phone HFP coexist (see cros_phone_coexist.h).
 ***************************************************************************/
#include "cros_phone_coexist.h"

#include "cros_bt_log.h"
#include "cros_cue.h"
#include "cros_sco_probe.h"
#include "cros_tws.h"

#include "me_api.h"

#include <string.h>

extern int app_bt_start_custom_function_in_bt_thread(uint32_t param0,
                                                     uint32_t param1,
                                                     uint32_t func);

static uint8_t g_paused_for_phone;
static uint8_t g_pause_queued;

static void pause_bt(void *a, void *b) {
  (void)a;
  (void)b;
  g_pause_queued = 0;
  if (!cros_tws_is_enabled()) {
    CROS_LOG(0, "[cros_phone] pause skipped — BiCROS already off");
    return;
  }
  CROS_LOG_ACK(0, "[cros_phone] pause BiCROS — phone wants HFP/SCO");
  g_paused_for_phone = 1;
  cros_tws_stop();
}

static void queue_pause(const char *why) {
  CROS_LOG_ACK(0, "[cros_phone] queue pause (%s)", why ? why : "?");
  if (g_pause_queued) {
    return;
  }
  g_pause_queued = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)pause_bt);
}

void cros_phone_on_hf_audio(int connected) {
  if (connected) {
    /*
     * Real AG HF audio — phone opened the call path.
     * Peer-SCO media uses hfp_ibrt_sco_audio_connected mock (no HF_EVENT).
     */
    if (cros_tws_is_enabled()) {
      queue_pause("HF_AUDIO_CONNECTED");
    }
    return;
  }
  if (g_paused_for_phone) {
    g_paused_for_phone = 0;
    CROS_LOG_ACK(0, "[cros_phone] HF_AUDIO_DISCONNECTED — phone released; "
                    "quad-tap to re-enable BiCROS");
    cros_cue_ready();
  }
}

void cros_phone_on_sco_event(uint8_t evt, uint8_t err, const uint8_t *rem6) {
  (void)err;
  if (evt != BTIF_BTEVENT_SCO_CONNECT_IND &&
      evt != BTIF_BTEVENT_SCO_CONNECT_CNF) {
    return;
  }
  if (!cros_tws_is_enabled()) {
    return;
  }
  if (cros_sco_rem_is_peer(rem6)) {
    return; /* our bud↔bud SCO */
  }
  queue_pause(evt == BTIF_BTEVENT_SCO_CONNECT_IND ? "SCO_CONNECT_IND"
                                                   : "SCO_CONNECT_CNF");
}

int cros_phone_paused(void) { return g_paused_for_phone ? 1 : 0; }
