# Progress log

Living changelog for the PineBuds Pro CROS / SITE firmware effort.
Newest entries at the top. DIY / tinker project — no clinical or PPE claims.

## 2026-09-19

### Done
- Scaffolded repo: docs, bootstrap/build scripts, Cloud Agent `environment.json`
- Validated local OpenPineBuds build → `open_source.bin` (~883 KB)
- Install script proven idempotent (ran twice)
- Snapshot + draft environment build **succeeded** (`bld-20260919-d4c96646-…`); fresh Cloud Agent verify **PASS**
- Earlier draft also succeeded (`bld-20260919-fd017b3b-…`)
- Agreed product framing: Stock / CROS / SITE; user-configurable poor ear; Android companion later
- Agreed CROS architecture: bud↔bud low-rate voice over TWS (not phone); FF mic TX → good-ear RX
- Flash budget awareness: treat ~500 erase cycles as scarce; prefer NV + app for settings
- **Stage A firmware ready to flash:**
  - `firmware/stage_a/` — FF (MIC1) → local speaker loopback (16 kHz mono, soft gain, clip)
  - Auto-starts after boot; **quad-tap** toggles on/off
  - Patch `patches/0001-stage-a-ff-loopback.patch` wired into OpenPineBuds apps/config
  - `STAGE_A=1` default in `./scripts/build.sh` → `open_source.bin` (~884 KB) with `cros_loopback_*` linked
  - Monday hardware checklist: [monday-checklist.md](monday-checklist.md)
  - **Flash packages:** versioned zips under [`flash-packages/`](../flash-packages/) (`VERSION` + [`CHANGELOG.md`](../CHANGELOG.md) + `BESTOOL.md` in each zip); grab [`pinebuds-cros-LATEST.zip`](../flash-packages/pinebuds-cros-LATEST.zip)
  - **v0.1.1:** fixed ASRMIC path table to use `CFG_HW_AUD_INPUT_PATH_ASRMIC_DEV` (0.1.0 mapped ASRMIC to talk mic by mistake)

### Blocked / next
- [x] GitHub remote linked (deploy key + push)
- [x] **Save** Cloud Agent environment proposal in UI
- [x] Stage A: single-bud FF mic → speaker loopback (**built** in cloud; **flash** on Windows when buds arrive)
- [x] Flash ZIP pipeline (`scripts/package-flash.sh` → `flash-packages/` → GitHub)
- [x] Semver (`VERSION`) + changelog + bestool instructions in every zip (**v0.1.0**)
- [x] ASRMIC table wiring fix (**v0.1.1**) — discard 0.1.0 for clap tests
- [x] Scrub account/email identifiers from tracked files before public release (**v0.1.2**)
- [ ] Monday: backup stock → flash **v0.1.2+** → clap/quad-tap test → report results
- Windows is the primary flash workstation ([docs/windows-flash.md](windows-flash.md))

### Design decisions (locked for now)
| Topic | Decision |
|-------|----------|
| Modes | Stock, CROS, SITE |
| Poor ear | User-configurable |
| CROS transport | TWS/IBRT between buds, not phone |
| Companion app | Android first (BLE datapath); after audio pipe works |
| Flash workstation | Windows (`bestool` or PINE64 `dld_main`) |
| Stage A UX | Auto-start loopback + quad-tap toggle |
| Disclaimers | Own-risk DIY; not a hearing aid or hearing protection |
