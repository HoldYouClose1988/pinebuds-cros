# Changelog

All notable flash-package / firmware iterations for PineBuds Pro CROS + SITE.
Format: version, date (UTC), then user-facing changes.

**This project is a work in progress and is not a functional CROS product.** DIY / own-risk - not a hearing aid or PPE.

## [0.1.5] — 2026-09-21

### Package
- New flash zip with ASCII-safe `backup.ps1` (fixes Windows PowerShell parse error)
- Same experimental firmware bin as v0.1.3/v0.1.4

## [0.1.4] — 2026-09-21

### Package
- Fix `backup.ps1` parse error on Windows PowerShell (non-ASCII em dash in throw string)

## [0.1.3] — 2026-09-19

### Docs
- Public-facing README with a prominent **work in progress / not functional** banner
- Removed personal bring-up checklist and private progress log
- Feature READMEs state CROS and SITE modes are not implemented

### Package
- Regenerated flash zip with updated `FLASH.md` (experimental loopback notes only; not CROS)

## [0.1.2] — 2026-09-19

### Privacy
- Removed hardcoded account URLs from docs, packaging scripts, and zip manifests (relative paths only)
- Dropped older flash zips that embedded account URLs; republished experimental loopback bin
- Rewrote git history so early commits no longer carry a personal author email

### Firmware
- Same experimental loopback binary lineage as v0.1.1 (ASRMIC → FF mic table fix)

## [0.1.1] — 2026-09-19

### Firmware
- **Fix:** `cfg_audio_input_path_cfg` for `AUD_INPUT_PATH_ASRMIC` now uses `CFG_HW_AUD_INPUT_PATH_ASRMIC_DEV` (was wrongly wired to `MAINMIC_DEV` / talk mic)
- Confirmed `VOICE_DETECTOR_EN=0` on `open_source` so the ASRMIC `#else` branch is active

### Package
- Rebuild with `CROS_STAGE_A=1`

## [0.1.0] — 2026-09-19

First numbered flash package.

### Firmware
- Experimental same-bud FF (MIC1) → speaker loopback (16 kHz mono, soft gain, peak clip)
- Auto-starts after boot; **quad-tap** toggles loopback off/on
- Note: 0.1.0 bin still had the ASRMIC→MAINMIC table bug; use **0.1.1+**

### Package
- Includes `open_source.bin`, `flash.ps1`, `backup.ps1`, `FLASH.md`, `BESTOOL.md`, `CHANGELOG.md`, `VERSION`, `MANIFEST.txt`, `SHA256SUMS`
