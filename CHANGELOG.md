# Changelog

All notable flash-package / firmware iterations for PineBuds Pro CROS + SITE.
Format: version, date (UTC), then user-facing changes. DIY / own-risk — not a hearing aid or PPE.

## [0.1.2] — 2026-09-19

### Privacy
- Removed hardcoded GitHub account URLs from docs, packaging scripts, and zip manifests (relative paths only)
- Dropped older flash zips that embedded account URLs; republished current Stage A bin as this version
- Rewrote git history so the initial commit no longer carries a personal author email

### Firmware
- Same Stage A binary lineage as v0.1.1 (ASRMIC → FF mic table fix)

## [0.1.1] — 2026-09-19

### Firmware
- **Fix:** `cfg_audio_input_path_cfg` for `AUD_INPUT_PATH_ASRMIC` now uses `CFG_HW_AUD_INPUT_PATH_ASRMIC_DEV` (was wrongly wired to `MAINMIC_DEV` / talk mic). Stage A FF (MIC1) remap actually takes effect.
- Confirmed `VOICE_DETECTOR_EN=0` on `open_source` so the ASRMIC `#else` branch is active.

### Package
- Rebuild with `CROS_STAGE_A=1`; zip bin SHA updated in `MANIFEST.txt` / `LATEST.txt`

## [0.1.0] — 2026-09-19

First numbered flash package.

### Firmware
- Stage A: FF (MIC1) → same-bud speaker loopback (16 kHz mono, soft gain, peak clip)
- Auto-starts after boot; **quad-tap** toggles loopback off/on
- ANC path disabled while loopback is starting (reduces fight with local play)
- Note: 0.1.0 bin still had the ASRMIC→MAINMIC table bug; use **0.1.1+** for clap tests on the outer mic.

### Package
- Includes `open_source.bin`, `flash.ps1`, `backup.ps1`, `FLASH.md`, `BESTOOL.md`, `CHANGELOG.md`, `VERSION`, `MANIFEST.txt`, `SHA256SUMS`
- Windows flash helpers default `BinPath` to `.\open_source.bin`

### Docs
- Full **bestool** install + usage instructions in `BESTOOL.md`
- Clap-test steps for Monday hardware bring-up

### Firmware
- **Fix:** `cfg_audio_input_path_cfg` for `AUD_INPUT_PATH_ASRMIC` now uses `CFG_HW_AUD_INPUT_PATH_ASRMIC_DEV` (was wrongly wired to `MAINMIC_DEV` / talk mic). Stage A FF (MIC1) remap actually takes effect.
- Confirmed `VOICE_DETECTOR_EN=0` on `open_source` so the ASRMIC `#else` branch is active.

### Package
- Rebuild with `CROS_STAGE_A=1`; zip bin SHA updated in `MANIFEST.txt` / `LATEST.txt`

## [0.1.0] — 2026-09-19

First numbered flash package.

### Firmware
- Stage A: FF (MIC1) → same-bud speaker loopback (16 kHz mono, soft gain, peak clip)
- Auto-starts after boot; **quad-tap** toggles loopback off/on
- ANC path disabled while loopback is starting (reduces fight with local play)
- Note: 0.1.0 bin still had the ASRMIC→MAINMIC table bug; use **0.1.1+** for clap tests on the outer mic.

### Package
- Includes `open_source.bin`, `flash.ps1`, `backup.ps1`, `FLASH.md`, `BESTOOL.md`, `CHANGELOG.md`, `VERSION`, `MANIFEST.txt`, `SHA256SUMS`
- Windows flash helpers default `BinPath` to `.\open_source.bin`

### Docs
- Full **bestool** install + usage instructions in `BESTOOL.md`
- Clap-test steps for Monday hardware bring-up
