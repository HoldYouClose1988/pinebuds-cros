# Investigation — “clean link” was HFP volume, not ANC / teardown

**Date:** 2026-09-27  
**Trigger:** Ear note after v0.3.52/53 — clean BiCROS coincided with a LEFT
touch the user first read as ANC. A/B showed **volume**, not ANC.

**Verdict:** **sco=8 + noise=3** holds (ear). Shipped in **v0.3.55** (was 11
in 0.3.54). App label: **SCO DAC gain**. Separate **Music (A2DP)** slider.

## Gesture map (what our flash actually does)

Active key table is `vendor/OpenPineBuds/apps/main/key_handler.cpp` via
`app_key_init()` (not the unused `app_ibrt_ui_test_key_init`).

| Gesture | LEFT (good) | RIGHT (poor) |
|---------|-------------|--------------|
| Hold (long press) | **ANC on/off** (synced over IBRT) | **ANC on/off** |
| Triple tap | **Volume down** | Volume up |
| Quad tap | CROS / BiCROS toggle | same |

So **triple-tap LEFT ≠ ANC** — it is `hfp_vol` down. ANC is **hold** (did
nothing for this noise).

## Verdict (ear A/B — 2026-09-27)

**Volume wins.** Hold/ANC did nothing. Triple LEFT (vol down) cleaned it.
**vol=11 + noise=3** holds steady. Shipped as defaults in **v0.3.54**.

### Why turning volume “up” sounded like link noise

Your theory is right: we are **not** changing music/system volume.

| Knob | Field | Stream | What it does |
|------|-------|--------|--------------|
| App **Volume** / bud ± during BiCROS | `hfp_vol` | HFP **SCO** player | Codec **DAC digital gain** on call playback |
| Phone media volume / A2DP | `a2dp_vol` | A2DP music | Separate NV; idle while BiCROS SCO is up |
| App **Mix** | sidetone dB | HW sidetone | Local good-ear mic → speaker (**independent** of `hfp_vol`) |

`cros_sco_set_hfp_volume` → `app_audio_manager_ctrl_volume` + `app_bt_stream_volumeset`
→ `stream_cfg.vol` on the SCO playback AF stream → `codec_dac_vol[level]`.

Open-source DAC table (third field = dB):

| Level | DAC gain |
|-------|----------|
| 11 | **−12 dB** (new default) |
| 13 | **−6 dB** (old default) |
| 15 | **0 dB** |

So 13→11 is **6 dB quieter** on the contralateral SCO path only. Volume does
not *create* RF/link hash — it **amplifies the existing SCO/mSBC noise floor**
(codec quantization, PLC, mic hiss from the poor bud) until it rides under
speech as “hash.” Noise filter 3 then soft-gates residual hiss on that same
PCM. Sidetone stays at mix −20 dB, so after the drop local ear can feel a bit
louder relative to CROS — nudge mix if needed.

## What ANC does when enabled (ruled out for this noise)

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

## Ranked explanations for “clean” (historical — volume confirmed)

1. **Volume down on LEFT / lower `hfp_vol` — CONFIRMED**  
   Default 13 (−6 dB) made SCO floor audible; 11 (−12 dB) + noise 3 holds.

2. **Ambient ANC on the good ear — ruled out for this report**  
   Hold did nothing in the A/B.

3. **ANC on RIGHT / mic routing — not needed**  

4. **SCO teardown / CLOSED wait (v0.3.52–53)**  
   Still useful for re-arm without case; not the in-session noise fix.

5. **App noise filter** — helpful at 3 once vol is sane; default now 3.

## Ear experiments (completed)

| # | Action | Result |
|---|--------|--------|
| A | Hold LEFT (ANC) | No change |
| B | Triple LEFT (vol down) | Cleaned |
| — | App vol=11 + noise=3 | Holds steady |

## Firmware follow-ups

**Done in v0.3.54:** default `vol=11`, `noise=3` (fw + app seekbars).

Optional later:

1. Persist vol/noise in NV across reboot.
2. Nudge default mix if quieter SCO makes sidetone dominate.
3. Keep CLOSED-wait teardown for re-arm.

## Code pointers

- Gestures: `apps/main/key_handler.cpp` (`send_enable_disable_anc`, triple/vol)
- Vol path: `cros_sco_voice_bridge.cpp` → `hfp_vol` / `app_bt_stream_volumeset`
- DAC table: `config/open_source/tgt_hardware.c` `codec_dac_vol[]`
- Defaults: `firmware/stage_b/cros_cfg.c` (`CROS_VOL_DEFAULT`, `CROS_NOISE_DEFAULT`)
- BiCROS shape: `firmware/stage_b/cros_sco_probe.c` (`cros_sco_apply_cros_mute`)
- SCO/sidetone mic: talk `CH4`; ANC FF/FB `CH0`/`CH2` (`docs/hardware.md`)
