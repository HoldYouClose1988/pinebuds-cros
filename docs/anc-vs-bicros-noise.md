# Investigation — “clean link” was likely ANC (or volume), not SCO teardown

**Date:** 2026-09-27  
**Trigger:** Ear note after v0.3.52/53 — the second BiCROS session sounded clean;
user later realized they had **tapped the LEFT bud three times**, which they
believe **enabled ANC**, and that is what cleaned the noise (not teardown).

This is a research note. No firmware change yet — confirm on-ear first.

## Gesture map (what our flash actually does)

Active key table is `vendor/OpenPineBuds/apps/main/key_handler.cpp` via
`app_key_init()` (not the unused `app_ibrt_ui_test_key_init`).

| Gesture | LEFT (good) | RIGHT (poor) |
|---------|-------------|--------------|
| Hold (long press) | **ANC on/off** (synced over IBRT) | **ANC on/off** |
| Triple tap | **Volume down** | Volume up |
| Quad tap | CROS / BiCROS toggle | same |

So **triple-tap LEFT ≠ ANC** in this tree. ANC is **hold**. Modes are
**ON ↔ OFF only** (`ANC_COEF_NUM=1`; talk-through not enabled). ANC fade may
play Alexa-start/stop prompts — useful ear cue that ANC actually toggled.

Two live hypotheses until the next wear test separates them:

1. **Hold → ANC** (user miscounted taps as “three”, or held briefly).
2. **Triple → volume down** on the good ear (quieter SCO playback / less
   audible hiss), mislabeled as ANC.

## What ANC does when enabled

- Build has `ANC_APP=1`, `ANC_FF_ENABLED`, `ANC_FB_ENABLED`, `APP_ANC_KEY=1`.
  Symbols (`app_anc_key`, `anc_open`, …) are in the v0.3.53 image.
- `app_anc_enable()` opens **feed-forward + feed-back** filters into the DAC,
  forces speaker enable, loads coefs (open-source calibration is imperfect but
  the engine does run).
- Mic map: ANC FF=`CH0` (MIC1), FB=`CH2` (MIC3). BiCROS SCO capture + HW
  sidetone use talk mic **`CH4`** — **no shared ADC** with ANC. So ANC is
  unlikely to “clean the SCO mic path”; it cancels ambient at the speaker.
- `CODEC_ANC_BOOST` is off in open_source → no intentional ≈−2 dB DAC duck
  from ANC on this image.
- Toggle is **TWS-synced** (`app_anc_status_sync` / `APP_TWS_CMD_SYNC_ANC_STATUS`)
  — enabling from LEFT turns ANC on **both** buds.

## How this intersects BiCROS

| Path | Role |
|------|------|
| BiCROS LEFT | SCO RX → speaker + **HW sidetone** (local mic mix, CH4) |
| BiCROS RIGHT | Talk/SCO mic TX (CH4), speaker forcemuted |
| ANC LEFT | Hybrid cancel of **ambient** into the same speaker (CH0/CH2) |
| ANC RIGHT | Same engine on poor bud (mics + possible speaker re-enable) |

Important CROS interaction:

```c
/* cros_tws.c apply ON */
if (app_anc_work_status()) {
  app_anc_disable();  /* "for FF mic access" — Stage A leftover */
}
```

- We **kill ANC only at CROS enable**, and only if software thought it was on.
  Reason string is Stage A (FF for loopback); SCO BiCROS does not need FF.
- `app_anc_disable()` closes the hardware path but **may leave
  `anc_work_status` ON** → next hold can be a no-op “off” before a second
  hold turns ANC on again. Worth watching in the A/B.
- If ANC was **off** at quad-tap (typical): auto-disable skipped → **hold can
  enable ANC while BiCROS is up**. Matches “BiCROS running → then touch LEFT
  → suddenly clean.”

## Ranked explanations for “clean”

1. **Ambient ANC on the good ear (most likely if hold)**  
   Room / HVAC / seal leak under BiCROS sounded like “link noise.” ANC
   cancels that locally. SCO link unchanged. Explains why it felt like the
   *same* session got clean without a new OPENED. Mic paths don’t overlap.

2. **Volume down on LEFT (if true triple-tap)**  
   Default good-ear HFP vol is **13**. Triple LEFT issues
   `IBRT_ACTION_LOCAL_VOLDN`. Lower playback gain shrinks SCO hiss / artifacts
   that ride the digital gain. Sidetone is a separate HW path — may stay loud.

3. **ANC on RIGHT changes uplink / mic routing (secondary)**  
   Sync turns ANC on the poor bud too. That opens FF/FB ADCs and can call
   `analog_aud_codec_speaker_enable(true)` — possibly fighting our POOR
   forcemute. Weak for “what LEFT heard”; still check mute after ANC sync.

4. **SCO teardown / CLOSED wait (v0.3.52–53) — downgraded**  
   Still useful for re-arm without case, but **not** the clean-up mechanism
   for in-session noise if ANC/volume was the real change.

5. **App noise filter** — soft gate + HF rolloff on good-ear SCO PCM
   (`noise=0..5`, default 0). Separate from ANC; leave at 0 until A/B done.

## Ear experiments (do these next)

Keep BiCROS up the whole time. Phone log on. Prefer **DISABLE via quad-tap**
only between full trials.

| # | Action | Expect if ANC | Expect if volume |
|---|--------|---------------|------------------|
| A | BiCROS on, noisy. **Hold** LEFT ~1 s | Clean; ANC prompt / UART on | Unchanged (unless hold misfires) |
| B | BiCROS on, noisy. **Triple** LEFT only | Unchanged | Cleaner / quieter SCO |
| C | After A: **hold** again (ANC off) | Noise returns (may need 2 holds if status sticky) | — |
| D | App **Volume** slider down (no bud taps) | Unchanged | Same clean as B |
| E | ANC on **before** quad-tap CROS | May log `disabling ANC for FF mic access`; noise may return | — |
| F | ANC on **after** CROS (A) vs `noise=3` alone | ANC wins without noise knob | — |
| G | Hold RIGHT vs hold LEFT | Same (synced) | — |

Phone / UART cues to capture:

- `[cros_tws] disabling ANC for FF mic access` (only at CROS enable)
- ANC traces: `app_anc_key`, `anc_work_status`, `SYNC_ANC_STATUS`
- Volume: `IBRT_ACTION_LOCAL_VOLDN` / hfp vol change
- Alexa-start/stop voice report on ANC fade (if present)

## Likely firmware follow-ups (after A/B)

If **ANC** is the cleaner:

1. Stop auto-disabling ANC at CROS enable under SCO BiCROS (Stage A leftover;
   FF not needed). Or only disable on poor side if FF TX returns.
2. If keeping force-disable: also clear `anc_work_status` / use the proper
   stop path so one hold re-enables.
3. Optional: auto-enable ANC on good ear when BiCROS opens (or Android toggle).
4. Log `anc_work_status` on ENABLE / OPENED / shape apply.
5. Re-apply POOR forcemute after ANC sync if RIGHT speaker reopens.

If **volume** is the cleaner:

1. Lower default `CROS_SCO_HFP_VOL` / app default below 13.
2. Document LEFT triple = vol down during BiCROS.

## Code pointers

- Gestures: `apps/main/key_handler.cpp` (`send_enable_disable_anc`, triple/vol)
- ANC enable path: `apps/anc/src/app_anc.c` → `app_anc_enable` / `app_anc_key`
- CROS kills ANC: `firmware/stage_b/cros_tws.c` (`apply_enabled`)
- BiCROS shape + vol: `firmware/stage_b/cros_sco_probe.c` (`cros_sco_apply_cros_mute`)
- SCO/sidetone mic: talk `CH4`; ANC FF/FB `CH0`/`CH2` (`docs/hardware.md`)
