# PineBuds Pro — CROS + Industrial Damping Firmware

Custom firmware effort for the [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro): add **CROS for SSHL** (contralateral routing of signals for single-sided hearing loss) while keeping stock TWS / media / call behavior, and explore a **loud-noise damping** mode for high-SPL environments such as construction sites.

This repository holds design docs, feature specs, patches, and Cloud Agent build tooling. The BES “Little Whale” SDK is **not** vendored here (shared-source / all-rights-reserved); bootstrap pulls [OpenPineBuds](https://github.com/pine64/OpenPineBuds) locally.

## Goals

| Mode | Intent |
|------|--------|
| **Stock** | A2DP / AVRCP / HFP, TWS pairing, touch controls, ANC path as upstream provides |
| **CROS / BiCROS** | Deaf-side mic → good-side speaker over the bud-to-bud link; optional mix with phone audio |
| **Industrial damp** | Aggressive attenuation / limiting for continuous high ambient SPL (research track; not certified PPE) |

## Quick start (local)

**Flash host: Windows** (see [Windows flashing](docs/windows-flash.md)). Cloud Agents compile only — they cannot reach your USB case.

```powershell
# One-time: install WCH CH342 driver, build bestool.exe (Rust)
# Then, with open_source.bin on hand and your COM ports:
.\scripts\backup.ps1 -Port0 COM5 -Port1 COM6
.\scripts\flash.ps1 -BinPath .\open_source.bin -Port0 COM5 -Port1 COM6
```

Build the firmware on Linux / Cloud Agent / WSL (Stage A FF loopback is **on by default**):

```bash
./scripts/bootstrap-sdk.sh   # clones OpenPineBuds + ARM GCC 9-2019q4; syncs stage_a + patches
./scripts/build.sh           # STAGE_A=1 → open_source.bin with FF mic→speaker loopback
# → vendor/OpenPineBuds/out/open_source/open_source.bin
# Stock-only build: STAGE_A=0 ./scripts/build.sh
```

Always back up stock firmware before the first flash. Factory restore images are documented on the [PINE64 wiki](https://wiki.pine64.org/wiki/PineBuds_Pro#Firmware_images) and in the Windows guide.

## Stage A (this week)

Prove one-bud audio path before CROS: FF (MIC1) → same-bud speaker. Auto-starts; quad-tap toggles. See [Monday checklist](docs/monday-checklist.md).

### Download a flash package (Windows)

Versioned zips land on GitHub after each iteration (see [CHANGELOG.md](CHANGELOG.md), current [VERSION](VERSION)):

- **[pinebuds-cros-LATEST.zip](flash-packages/pinebuds-cros-LATEST.zip)** — newest build  
- [All packages](flash-packages/)

Unzip → follow **`BESTOOL.md`** → `.\backup.ps1` once → `.\flash.ps1 -Port0 COMx -Port1 COMy`.

Cloud agents bump `VERSION` / `CHANGELOG.md`, then run `./scripts/package-flash.sh`.

## Docs

- [Progress log](docs/PROGRESS.md)
- [Monday checklist — Stage A flash & clap test](docs/monday-checklist.md)
- [Flash packages](flash-packages/README.md)
- [Changelog](CHANGELOG.md)
- [bestool on Windows](docs/bestool-windows.md)
- [Windows flashing](docs/windows-flash.md) (primary flash workstation)
- [Hardware overview](docs/hardware.md)
- [Development environment](docs/development.md)
- [CROS / SSHL architecture](docs/architecture-cros.md)
- [Industrial noise damping](docs/architecture-noise-damping.md)
- [References](docs/references.md)

## Status

**Stage A ready to flash** when hardware arrives. Upstream OpenPineBuds still has incomplete ANC; full CROS (bud↔bud) and SITE damping are next after the loopback clap test passes.

## License

Project docs and scripts in this repo: see `LICENSE`. The Bestechnic SDK inside `vendor/OpenPineBuds` remains under BES / OpenPineBuds terms — see `NOTICE`.
