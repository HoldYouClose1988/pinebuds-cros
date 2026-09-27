/***************************************************************************
 * BiCROS status cues via stock media_PlayAudio AUD_IDs.
 *
 * Map (chosen for ear-distinct stock prompts, no new assets):
 *   ENABLED   → AUD_ID_BT_CONNECTED     (link-up)
 *   DISABLED  → AUD_ID_BT_DIS_CONNECT   (link-down)
 *   READY     → AUD_ID_BT_PAIRING_SUC   (clear / success)
 *   NOT_YET   → AUD_ID_BT_WARNING       (reject / busy)
 *   OPEN_FAIL → AUD_ID_BT_PAIRING_FAIL  (attempt failed)
 ***************************************************************************/
#include "cros_cue.h"

#include "cros_bt_log.h"

#include "app_media_player.h"
#include "resources.h"

static void play_local(AUD_ID_ENUM id, const char *tag) {
  CROS_LOG_ACK(0, "[cros_cue] %s id=%d", tag, (int)id);
  /*
   * Local only — poor-side speaker is forcemuted during BiCROS anyway.
   * Merging path matches stock volume-limit WARNING during HFP/SCO.
   */
  media_PlayAudio_locally(id, 0);
}

void cros_cue_enabled(void) {
  play_local(AUD_ID_BT_CONNECTED, "ENABLED");
}

void cros_cue_disabled(void) {
  play_local(AUD_ID_BT_DIS_CONNECT, "DISABLED");
}

void cros_cue_ready(void) {
  /* Prefer standalone after SCO is down so we do not depend on merge. */
  CROS_LOG_ACK(0, "[cros_cue] READY id=%d", (int)AUD_ID_BT_PAIRING_SUC);
  media_PlayAudio_standalone_locally(AUD_ID_BT_PAIRING_SUC, 0);
}

void cros_cue_not_yet(void) {
  play_local(AUD_ID_BT_WARNING, "NOT_YET");
}

void cros_cue_open_fail(void) {
  CROS_LOG_ACK(0, "[cros_cue] OPEN_FAIL id=%d", (int)AUD_ID_BT_PAIRING_FAIL);
  media_PlayAudio_standalone_locally(AUD_ID_BT_PAIRING_FAIL, 0);
}
