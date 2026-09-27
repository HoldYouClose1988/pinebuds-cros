/***************************************************************************
 * BiCROS audible status cues.
 *
 * Voice up: mix beeps into good-ear SCO PCM (never media_PlayAudio).
 * Voice down: stock standalone prompts (READY / OPEN_FAIL / DISABLED /
 * NOT_YET when teardown already stopped voice).
 ***************************************************************************/
#ifndef CROS_CUE_H
#define CROS_CUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cros_cue_enabled(void);
void cros_cue_disabled(void);
void cros_cue_ready(void);
void cros_cue_not_yet(void);
void cros_cue_open_fail(void);

/* Mix active SCO-path cue into good-ear playback PCM (16-bit LE). */
void cros_cue_mix_sco_pcm(uint8_t *buf, uint32_t len);
int cros_cue_sco_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_CUE_H */
