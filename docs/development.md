# Development environment

## Upstream projects

| Project | Role |
|---------|------|
| [pine64/OpenPineBuds](https://github.com/pine64/OpenPineBuds) | Primary SDK tree (“Little Whale” 2022-11-28 packaging), Docker image, `bestool`, build scripts |
| [hall/little-buddy](https://github.com/hall/little-buddy) | Alternate packaging (Nix flash); useful reference, less ANC focus |
| [Ralim/bestool](https://github.com/Ralim/bestool) | Flash / image tool used by OpenPineBuds |
| PINE64 wiki | Hardware, factory images, Windows programmer restore |

OpenPineBuds README note: the BES SDK is **not** under an open-source license (shared source / all rights reserved). Keep it in `vendor/` (gitignored) or as a local clone — do not republish SDK sources from this project.

## Official OpenPineBuds flow

```bash
git clone https://github.com/pine64/OpenPineBuds.git
cd OpenPineBuds
./start_dev.sh          # docker compose → ghcr.io/pine64/openpinebuds or local build
./build.sh              # make -j T=open_source DEBUG=1
./backup.sh             # before first experiment
bestool write-image out/open_source/open_source.bin --port /dev/ttyACM0
bestool write-image out/open_source/open_source.bin --port /dev/ttyACM1
```

Container is **privileged** and bind-mounts `/dev` for UART access.

### Toolchain inside their Dockerfile

- Debian bullseye
- `gcc-arm-none-eabi` **9-2019-q4-major** (ARM Keil blob URL)
- `make`, `bc`, `xxd`, `ffmpeg` (alert sound conversion)
- Rust-built `bestool`

## This repository’s Cloud / CI-style flow

Docker is optional. `./scripts/bootstrap-sdk.sh` installs the same GCC tarball under `.tools/`, clones OpenPineBuds into `vendor/OpenPineBuds`, and builds `bestool` if Rust is available.

```bash
./scripts/bootstrap-sdk.sh
./scripts/build.sh
```

Patches live in `patches/` and are applied onto `vendor/OpenPineBuds` during bootstrap (idempotent: skip if already applied). Experimental loopback sources in `firmware/stage_a/` are copied to `apps/cros_loopback/` on every bootstrap.

### Cursor environment

`.cursor/environment.json` `install` runs bootstrap so Cloud Agents get a ready tree. Flashing still requires a physical case on a **Windows** PC with USB — agents can **compile** and review; hardware bring-up is local. See [windows-flash.md](windows-flash.md).

## Build target of interest

- Default OpenPineBuds target: `T=open_source`
- ANC-oriented configs also exist: `config/best2300p_ibrt_anc/` — evaluate when multi-coef ANC is needed
- Alerts: `config/_default_cfg_src_/res/en/`; `AUDIO=cn ./build.sh` for Chinese set

## Debugging tips

- **Preferred (non-destructive):** phone TOTA SPP log sink — [bt-log-sink.md](bt-log-sink.md),
  Android app under `android/cros-log/`. Build with `TOTA=1 ./scripts/build.sh`.
- UART log (destructive / pads): OpenPineBuds `uart_log.sh` / minicom @ 2 Mbaud — defer until
  CROS latency is acceptable
- Force reboot for programming: long-hold rear button in case (~5 s) on OpenPineBuds builds
- If build breaks mysteriously: `rm -rf vendor/OpenPineBuds/out` and rebuild
- Keep a known-good `open_source.bin` and factory package for brick recovery

## Suggested workstation packages

**Windows (flash / backup):**

- WCH CH342 driver
- Rust + `bestool.exe`, or PINE64 `dld_main` programmer
- See [windows-flash.md](windows-flash.md)

**Linux / Cloud (compile):**

```text
git curl make bc xxd bzip2 ffmpeg clang-format
gcc / build-essential (for bestool)
Rust stable (bestool)
optional: docker + compose for parity with upstream
```
