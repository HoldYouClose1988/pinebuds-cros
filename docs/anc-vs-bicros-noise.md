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

So **triple-tap LEFT ≠ ANC** in this tree. ANC is **hold**.

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
- Toggle is **TWS-synced** (`app_anc_status_sync` / `APP_TWS_CMD_SYNC_ANC_STATUS`)
  — enabling from LEFT turns ANC on **both** buds.

## How this intersects BiCROS

| Path | Role |
|------|------|
| BiCROS LEFT | SCO RX → speaker + **HW sidetone** (local mic mix) |
| BiCROS RIGHT | Talk/SCO mic TX, speaker forcemuted |
| ANC LEFT | Hybrid cancel of **ambient** into the same speaker |
| ANC RIGHT | Same engine on poor bud (mics + possible speaker re-enable) |

Important CROS interaction:

```c
/* cros_tws.c apply ON */
if (app_anc_work_status()) {
  app_anc_disable();  /* "for FF mic access" */
}
```

We **kill ANC only at CROS enable**. If you turn ANC **on after** BiCROS is
already up, it stays on. That matches “BiCROS running → then touch LEFT →
suddenly clean.”

## Ranked explanations for “clean”

1. **Ambient ANC on the good ear (most likely if hold)**  
   Room / HVAC / seal leak under BiCROS sounded like “link noise.” ANC
   cancels that locally. SCO link unchanged. Explains why it felt like the
   *same* session got clean without a new OPENED.

2. **Volume down on LEFT (if true triple-tap)**  
   Default good-ear HFP vol is **13**. Triple LEFT issues
   `IBRT_ACTION_LOCAL_VOLDN`. Lower playback gain shrinks SCO hiss / artifacts
   that ride the digital gain. Sidetone is a separate HW path — may stay loud.

3. **ANC on RIGHT changes uplink / mic routing (secondary)**  
   Sync turns ANC on the poor bud too. That opens FF/FB ADCs and can call
   `analog_aud_codec_speaker_enable(true)` — possibly fighting our POOR
   forcemute. Could change TX “hash,” but the report was about what LEFT heard.

4. **SCO teardown / CLOSED wait (v0.3.52–53) — downgraded**  
   Still useful for re-arm without case, but **not** the clean-up mechanism
   for in-session noise if ANC/volume was the real change.

5. **App noise filter** — still optional; do not lean on it until ANC/vol are
   A/B’d.

## Ear experiments (do these next)

Keep BiCROS up the whole time. Phone log on. Prefer **DISABLE via quad-tap**
only between full trials.

| # | Action | Expect if ANC | Expect if volume |
|---|--------|---------------|------------------|
| A | BiCROS on, noisy. **Hold** LEFT ~1 s | Clean; UART/`app_anc` on | Unchanged (unless hold misfires) |
| B | BiCROS on, noisy. **Triple** LEFT only | Unchanged | Cleaner / quieter SCO |
| C | After A: **hold** again (ANC off) | Noise returns | — |
| D | App **Volume** slider down (no bud taps) | Unchanged | Same clean as B |
| E | ANC on **before** quad-tap CROS | CROS enable may log `disabling ANC for FF mic access` then noise back | — |
| F | ANC on **after** CROS (A) vs noise filter alone | ANC wins without `noise=` | — |

Phone / UART cues to capture:

- `[cros_tws] disabling ANC for FF mic access` (only at CROS enable)
- ANC traces: `app_anc_key`, `anc_work_status`, `SYNC_ANC_STATUS`
- Volume: `IBRT_ACTION_LOCAL_VOLDN` / hfp vol change

## Likely firmware follow-ups (after A/B)

If **ANC** is the cleaner:

1. Stop auto-disabling ANC on the **good** ear at CROS enable (keep disable on
   poor/FF-TX if still needed).
2. Optional: auto-enable ANC on good ear when BiCROS opens (or expose ANC in
   the Android app).
3. Log `anc_work_status` on ENABLE / OPENED / shape apply.
4. Check whether ANC-on-RIGHT reopens the muted poor speaker; force-mute again
   after ANC sync if so.

If **volume** is the cleaner:

1. Lower default `CROS_SCO_HFP_VOL` / app default below 13.
2. Document LEFT triple = vol down during BiCROS.

## Code pointers

- Gestures: `apps/main/key_handler.cpp` (`send_enable_disable_anc`, triple/vol)
- ANC enable path: `apps/anc/src/app_anc.c` → `app_anc_enable` / `app_anc_key`
- CROS kills ANC: `firmware/stage_b/cros_tws.c` (`apply_enabled`)
- BiCROS shape + vol: `firmware/stage_b/cros_sco_probe.c` (`cros_sco_apply_cros_mute`)
- Mics: `docs/hardware.md` (FF MIC1, FB MIC3, talk MIC5)
