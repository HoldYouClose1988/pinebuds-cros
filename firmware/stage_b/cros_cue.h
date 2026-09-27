/***************************************************************************
 * BiCROS audible status cues — stock AUD_ID prompts only (no custom PCM).
 *
 * History: v0.3.1 triple WARNING on activate raced AF stream bring-up and
 * killed CROS audio; v0.3.2 delayed streams 1.2 s after the cue; later the
 * activate cue was removed. For SCO BiCROS we fire ENABLED only after voice
 * + shape are up (short settle), never on the tap itself.
 ***************************************************************************/
#ifndef CROS_CUE_H
#define CROS_CUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Confirmed OPENED + voice/shape — not the enable tap. */
void cros_cue_enabled(void);
/* Quad-tap / peer mode off received (teardown may still be running). */
void cros_cue_disabled(void);
/* Safe to re-enable: BTEVENT disconnect, or force cool-down finished. */
void cros_cue_ready(void);
/* Enable requested while closing / await HCI / cool-down / bring-up hold. */
void cros_cue_not_yet(void);
/* open_link accepted but OPENED never arrived (give-up). */
void cros_cue_open_fail(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_CUE_H */
