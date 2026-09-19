# Industrial / construction-site noise damping

## Intent

Add a firmware **program** that reduces listener exposure and listening fatigue in continuous high-SPL environments (jobsites, shops, generators), while remaining usable for talk-through / CROS awareness when needed.

This is **not** NRR-rated hearing protection. PineBuds Pro are IPX4 consumer buds with light passive seal. Treat the feature as assisted attenuation + limiting, and keep foam plugs / earmuffs as the real PPE layer when required.

## What the hardware can do

| Mechanism | Stock / SDK support | Use for damping |
|-----------|---------------------|-----------------|
| Passive tip seal | User tips only | Baseline; tip fit matters more than firmware |
| Hybrid ANC (FF + FB) | BES ANC engine + coef tables | Primary continuous reduction for low–mid energy |
| Talk-through / ambient | `ANC_TALK_THROUGH` hooks | Awareness without full open ear |
| Playback limiter | App volume + DSP EQ | Cap media + CROS injection level |
| Noise tracker / adaptive vol | SDK `notes.txt` `vol_state_process` | Inverse: reduce gain as ambient rises |
| Howling / wind | `anc_wnr.c` | Keep ANC stable outdoors |

OpenPineBuds ANC is still a work in progress (engine can enable; calibration vs factory is incomplete). Industrial mode should degrade gracefully: **limiter + EQ shelf + muted media** even if ANC coefs are mediocre.

## Target acoustic behavior

Construction noise is often:

- Broadband + strong low-mid (engines, HVAC, impact)
- High crest factor (impacts, nailers)
- Long duration (dose matters more than peak alone)

Firmware response stack (priority order):

1. **Brick-wall / soft-knee output limiter** on DAC path (always on in this program)
2. **High ANC coef set** tuned for 50–500 Hz emphasis (once we can load multi-coef `ANC_COEF_NUM > 1`)
3. **Speech band talk-through** optional: attenuate &lt;300 Hz and &gt;4 kHz less aggressively than mid band, or use a separate talk-through coef
4. **CROS interaction**: when both modes on, CROS feed is pre-limited so the good ear is not used as an unprotected mic dump of the jobsite

## Program UX

Working name: **SITE** mode.

- Enter: touch chord (TBD) or case-button sequence while worn
- LED / voice prompt: “Site mode”
- Levels: Low / Med / High attenuation presets (three ANC coef indices + three limiter thresholds)
- Exit restores previous ANC/CROS/media state

## Engineering plan

### Phase A — software safety net (no ANC dependency)

- Central `audio_safety` gain stage on render + CROS inject
- Absolute peak cap (e.g. digital full-scale −6 to −12 dBFS headroom) and RMS-driven AGC that **reduces** gain as level rises
- Unit-testable on host with WAV fixtures later; on-target via UART meters

### Phase B — ANC presets

- Enable multi-curve ANC (`ANC_COEF_NUM`, talk-through define) in `tgt_hardware`
- Extract / port factory coefs via OpenPineBuds `dev_tools/anc_decoder` where legal/useful
- Field tune with controlled pink noise + phone SPL meter (approximate)

### Phase C — dose awareness (optional)

- Use mic level estimate to suggest “take a break” haptic/voice after prolonged high ambient — advisory only
- Log histograms over UART for development builds

## Interaction with stock features

| Feature | SITE mode policy |
|---------|------------------|
| A2DP | Allowed but pre-limited; optional auto-pause above threshold |
| HFP calls | Prefer pause SITE limiting duck only; intelligible call &gt; max atten |
| ANC hold toggle | Rebind to cycle SITE presets when SITE active |
| CROS | Allowed with shared limiter |

## Risks

- Over-attenuating speech → isolation / safety hazard on a site (missed alarms). Mitigate with talk-through preset and max atten ceiling.
- ANC howling with poor seal → keep WNR / howl detect paths enabled.
- User treating buds as PPE → document clearly in README and voice prompt on first enable.
