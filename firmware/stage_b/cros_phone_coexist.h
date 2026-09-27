/***************************************************************************
 * iOS / phone HFP coexist — pause BiCROS when the phone wants the voice link.
 *
 * BiCROS rides peer eSCO + HFP voice player. iPhone expects exclusive HFP
 * eSCO for call-like audio; keeping peer SCO up causes ACL drops (~1–11 min)
 * and half-routed system sounds. On phone HF audio / non-peer SCO connect,
 * tear BiCROS down and let stock HFP own the link. User re-enables with
 * quad-tap after READY (no auto-restart).
 *
 * Android-stable track (v0.3.65) does not include this policy.
 ***************************************************************************/
#ifndef CROS_PHONE_COEXIST_H
#define CROS_PHONE_COEXIST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Real HF_EVENT_AUDIO_CONNECTED / DISCONNECTED (not IBRT peer-SCO mock). */
void cros_phone_on_hf_audio(int connected);

/*
 * BTEVENT SCO_CONNECT_IND / SCO_CONNECT_CNF / SCO_DISCONNECT.
 * rem6 may be NULL. Non-peer connect while BiCROS on → pause.
 */
void cros_phone_on_sco_event(uint8_t evt, uint8_t err, const uint8_t *rem6);

/* 1 if we paused BiCROS for the phone and have not seen HF audio down yet. */
int cros_phone_paused(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_PHONE_COEXIST_H */
