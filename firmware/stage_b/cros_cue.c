/***************************************************************************
 * BiCROS status cues.
 *
 * v0.3.59 used media_PlayAudio during SCO (CONNECTED after OPENED). Ear log
 * 084801: ENABLED cue fired → SPP/taps died; case reset required. Same class
 * of failure as v0.3.1 (prompt AF vs CROS AF).
 *
 * v0.3.60: while peer SCO / voice is up, mix short square beeps into the
 * good-ear SCO PCM path only — never start APP_PLAY_BACK_AUDIO. Media
 * prompts are allowed only when SCO is fully down (READY / OPEN_FAIL).
 *
 * Patterns (16 kHz mono, good ear):
 *   ENABLED   — 1× medium  (~880 Hz)
 *   DISABLED  — 2× short
 *   READY     — media PAIRING_SUC if SCO down, else 1× high short
 *   NOT_YET   — 3× staccato
 *   OPEN_FAIL — media PAIRING_FAIL if SCO down, else 1× long low
 ***************************************************************************/
#include "cros_cue.h"

#include "cros_bt_log.h"
#include "cros_sco_probe.h"

#include "app_media_player.h"
#include "resources.h"

enum {
  CROS_CUE_RATE = 16000,
  CROS_CUE_AMP = 9000,
};

enum cros_cue_kind {
  CROS_CUE_KIND_NONE = 0,
  CROS_CUE_KIND_ENABLED,
  CROS_CUE_KIND_DISABLED,
  CROS_CUE_KIND_READY,
  CROS_CUE_KIND_NOT_YET,
  CROS_CUE_KIND_OPEN_FAIL,
};

static volatile uint8_t g_kind;
static volatile uint8_t g_beeps_left;
static volatile uint16_t g_beep_samples;
static volatile uint16_t g_gap_samples;
static volatile uint16_t g_phase_samples;
static volatile uint16_t g_freq_hz;
static volatile uint8_t g_in_gap;
static uint32_t g_tone_ph;

static int sco_live(void) {
  /* Prefer dedicated probe helper; fall back to log-hold (armed/up/closing). */
  if (cros_sco_voice_is_up()) {
    return 1;
  }
  return cros_sco_log_hold();
}

static void sco_start(uint8_t kind, uint8_t beeps, uint16_t freq_hz,
                      uint16_t beep_ms, uint16_t gap_ms) {
  g_kind = kind;
  g_beeps_left = beeps;
  g_freq_hz = freq_hz;
  g_beep_samples = (uint16_t)((CROS_CUE_RATE * (uint32_t)beep_ms) / 1000u);
  g_gap_samples = (uint16_t)((CROS_CUE_RATE * (uint32_t)gap_ms) / 1000u);
  g_phase_samples = g_beep_samples;
  g_in_gap = 0;
}

static void play_media_safe(AUD_ID_ENUM id, const char *tag) {
  CROS_LOG_ACK(0, "[cros_cue] %s media id=%d", tag, (int)id);
  media_PlayAudio_standalone_locally(id, 0);
}

void cros_cue_mix_sco_pcm(uint8_t *buf, uint32_t len) {
  int16_t *s;
  uint32_t n;
  uint32_t i;
  uint32_t half;

  if (!buf || len < 2 || g_kind == CROS_CUE_KIND_NONE || g_beeps_left == 0) {
    return;
  }
  s = (int16_t *)buf;
  n = len / sizeof(int16_t);
  half = g_freq_hz ? (CROS_CUE_RATE / (2u * (uint32_t)g_freq_hz)) : 8u;
  if (half < 2u) {
    half = 2u;
  }

  for (i = 0; i < n; i++) {
    int32_t y = s[i];
    if (g_beeps_left == 0) {
      g_kind = CROS_CUE_KIND_NONE;
      break;
    }
    if (!g_in_gap) {
      /* Square wave into playback. */
      {
        int16_t tone;
        g_tone_ph++;
        tone = ((g_tone_ph / half) & 1u) ? (int16_t)CROS_CUE_AMP
                                         : (int16_t)(-CROS_CUE_AMP);
        y += tone;
        if (y > 32767) {
          y = 32767;
        } else if (y < -32768) {
          y = -32768;
        }
        s[i] = (int16_t)y;
      }
      if (g_phase_samples > 0) {
        g_phase_samples--;
      }
      if (g_phase_samples == 0) {
        g_beeps_left--;
        if (g_beeps_left == 0) {
          g_kind = CROS_CUE_KIND_NONE;
        } else {
          g_in_gap = 1;
          g_phase_samples = g_gap_samples;
        }
      }
    } else {
      if (g_phase_samples > 0) {
        g_phase_samples--;
      }
      if (g_phase_samples == 0) {
        g_in_gap = 0;
        g_phase_samples = g_beep_samples;
      }
    }
  }
}

int cros_cue_sco_busy(void) {
  return (g_kind != CROS_CUE_KIND_NONE && g_beeps_left > 0) ? 1 : 0;
}

void cros_cue_enabled(void) {
  CROS_LOG_ACK(0, "[cros_cue] ENABLED (sco-pcm)");
  /* Never media_PlayAudio here — ear 084801 wedge. */
  sco_start(CROS_CUE_KIND_ENABLED, 1, 880, 180, 80);
}

void cros_cue_disabled(void) {
  CROS_LOG_ACK(0, "[cros_cue] DISABLED (sco-pcm)");
  sco_start(CROS_CUE_KIND_DISABLED, 2, 660, 90, 70);
}

void cros_cue_ready(void) {
  if (sco_live()) {
    CROS_LOG_ACK(0, "[cros_cue] READY (sco-pcm fallback)");
    sco_start(CROS_CUE_KIND_READY, 1, 1200, 120, 60);
    return;
  }
  play_media_safe(AUD_ID_BT_PAIRING_SUC, "READY");
}

void cros_cue_not_yet(void) {
  CROS_LOG_ACK(0, "[cros_cue] NOT_YET (sco-pcm)");
  sco_start(CROS_CUE_KIND_NOT_YET, 3, 990, 50, 45);
}

void cros_cue_open_fail(void) {
  if (sco_live()) {
    CROS_LOG_ACK(0, "[cros_cue] OPEN_FAIL (sco-pcm fallback)");
    sco_start(CROS_CUE_KIND_OPEN_FAIL, 1, 420, 350, 80);
    return;
  }
  play_media_safe(AUD_ID_BT_PAIRING_FAIL, "OPEN_FAIL");
}
