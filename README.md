# PineBuds Pro — CROS + Industrial Damping Firmware

> ## ⚠️ Work in progress — not functional
>
> **This project does not yet provide working CROS, BiCROS, or industrial noise damping.**  
> Builds may boot experimental code (for example a local mic→speaker loopback) that is unfinished, unsafe for daily use, and **not** a hearing aid or hearing protection.  
> Do **not** rely on it. Flash at your own risk; always keep a stock firmware backup and a factory restore path.

Custom firmware research for the [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro): explore **contralateral mic routing (CROS)** for single-sided hearing loss scenarios while keeping stock TWS / media / call behavior, and a separate track for **loud-environment attenuation** (not certified PPE).

This repository holds design docs, experimental patches, and build tooling. The BES “Little Whale” SDK is **not** vendored here (shared-source / all-rights-reserved); bootstrap pulls [OpenPineBuds](https://github.com/pine64/OpenPineBuds) locally.

## Goals (planned)

| Mode | Intent | Status |
|------|--------|--------|
| **Stock** | A2DP / AVRCP / HFP, TWS pairing, touch controls | Upstream OpenPineBuds baseline |
| **CROS / BiCROS** | Poor-side mic → good-side speaker over the bud↔bud link | **Not implemented** (design only) |
| **Industrial damp** | Aggressive attenuation / limiting for high ambient SPL | **Not implemented** (research track) |

Current experimental builds may enable a **same-bud FF mic loopback** for audio-path bring-up only. That is not CROS.

## Disclaimer

- DIY / own-risk tinkering — **no clinical claims**
- **Not** a medical device, hearing aid, or certified hearing protection
- On-chip flash has limited erase cycles; flash sparingly

## Quick start

**Flash host: Windows** — see [Windows flashing](docs/windows-flash.md) and [bestool setup](docs/bestool-windows.md).

```powershell
# After building bestool.exe and noting your two COM ports:
.\backup.ps1 -Port0 COM5 -Port1 COM6
.\flash.ps1 -Port0 COM5 -Port1 COM6
```

Build on Linux / WSL / CI:

```bash
./scripts/setup-public-git.sh   # optional; anonymous git identity for contributors
./scripts/bootstrap-sdk.sh      # OpenPineBuds + ARM GCC; syncs experimental sources + patches
./scripts/build.sh              # default: experimental loopback flag on
# → vendor/OpenPineBuds/out/open_source/open_source.bin
# Baseline without loopback: STAGE_A=0 ./scripts/build.sh
```

Or download a versioned zip from [`flash-packages/`](flash-packages/) (includes bin + helpers + `FLASH.md`). See [CHANGELOG.md](CHANGELOG.md) and [VERSION](VERSION).

Always back up stock firmware before flashing. Factory images: [PINE64 wiki](https://wiki.pine64.org/wiki/PineBuds_Pro#Firmware_images).

## Documentation

- [Changelog](CHANGELOG.md)
- [Flash packages](flash-packages/README.md)
- [Windows flashing](docs/windows-flash.md)
- [bestool on Windows](docs/bestool-windows.md)
- [Hardware overview](docs/hardware.md)
- [Development environment](docs/development.md)
- [Phone BT log sink (TOTA/SPP)](docs/bt-log-sink.md) — prefer before UART soldering
- [CROS transport notes](docs/cros-transport.md)
- [CROS architecture (design)](docs/architecture-cros.md)
- [Industrial noise damping (design)](docs/architecture-noise-damping.md)
- [Public git policy](docs/public-git.md)
- [References](docs/references.md)

Android log reader scaffold: [`android/cros-log/`](android/cros-log/).

## License

Project docs and scripts: [LICENSE](LICENSE). The Bestechnic SDK under `vendor/OpenPineBuds` remains under BES / OpenPineBuds terms — see [NOTICE](NOTICE).
