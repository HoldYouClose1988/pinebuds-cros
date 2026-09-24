# PineBuds Pro — CROS firmware (experimental)

> ## ⚠️ DIY / own-risk — not a hearing aid
>
> Experimental contralateral routing on [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro).
> It is **not** a medical device, prescribed CROS/BiCROS, or certified hearing protection.
> Flash only if you can restore stock firmware. Keep a backup.

Custom OpenPineBuds-based firmware: **poor-side FF mic → good-side speaker** over the bud↔bud link, while stock TWS / media / calls remain the baseline when CROS is off. A separate **industrial damping** track is still design-only.

The BES SDK is **not** vendored here; `./scripts/bootstrap-sdk.sh` pulls [OpenPineBuds](https://github.com/pine64/OpenPineBuds) locally.

## Current status (v0.3.17)

| Mode | Status |
|------|--------|
| **Stock TWS** | Upstream OpenPineBuds baseline when CROS is off |
| **Stage B CROS** | **Working experimentally** — 50 ms IMA-ADPCM on BESAUD **extra L2CAP** (`0x0b0e`), with cmd-path fallback |
| **Phone logs** | TOTA SPP (`TOTA=1`) + [android/cros-log](android/cros-log/); auto-quiets during extra media so logging does not kill the pipe |
| **Industrial damp** | Not implemented (research docs only) |

**Ear-validated (v0.3.16+):** multi-minute runs with Capture on. Clap **start→start**: v0.3.19 ≈323 ms, v0.3.20 ≈336 ms with heavy dropouts on jitter floor 3. **v0.3.21** restores floor **4** for stability; latency tuning continues without thinning the floor.

Default mapping: **RIGHT = poor (mic / TX)**, **LEFT = good (speaker / RX)**. Quad-tap either bud toggles CROS (needs TWS link).

Latest flash zip: [`flash-packages/pinebuds-cros-LATEST.zip`](flash-packages/pinebuds-cros-LATEST.zip) · [CHANGELOG](CHANGELOG.md) · [VERSION](VERSION)

## How it works (short)

```
RIGHT (poor)                         LEFT (good)
────────────                         ───────────
FF mic → 50 ms ADPCM ──extra L2CAP──► decode → speaker
                 ╲                   (cmd path if extra not READY)
                  └─ MODE sync on IBRT custom cmd
```

Bring-up history and transport notes: [docs/cros-transport.md](docs/cros-transport.md).

## Quick start (Windows flash)

See [Windows flashing](docs/windows-flash.md) and [bestool](docs/bestool-windows.md).

1. Download **[pinebuds-cros-LATEST.zip](flash-packages/pinebuds-cros-LATEST.zip)** (includes `bestool.exe`).
2. Backup once, then flash **both** buds:

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6
.\flash.ps1  -Port0 COM5 -Port1 COM6
```

3. Seat both buds in the case ~30–60 s so TWS re-pairs.
4. Wear both; **quad-tap** to toggle CROS. Scratch/speak near the **right** outer face — hear it in the **left** ear.
5. Optional logs: build [android/cros-log](android/cros-log/), pair the master bud, **Capture logs** on. After `peer READY` the app goes quiet on purpose (`[cros_log] quiet=1`) so SPP does not contend with extra audio; transitions still appear.

Avoid phone music while testing CROS (A2DP fights the stream).

## Build from source

```bash
./scripts/setup-public-git.sh   # optional anonymous git identity
./scripts/bootstrap-sdk.sh      # OpenPineBuds + ARM GCC; syncs firmware/ + patches
TOTA=1 ./scripts/build.sh       # Stage B CROS + TOTA log sink
# → vendor/OpenPineBuds/out/open_source/open_source.bin
./scripts/package-flash.sh      # optional Windows zip under flash-packages/
```

`STAGE_A=0` / `STAGE_B=0` / `TOTA=0` as needed. Details: [docs/development.md](docs/development.md).

## Documentation

| Doc | Topic |
|-----|--------|
| [CHANGELOG.md](CHANGELOG.md) | Version history / ear results |
| [flash-packages/](flash-packages/) | Downloadable bins |
| [docs/cros-transport.md](docs/cros-transport.md) | Cmd vs extra L2CAP, lessons learned |
| [docs/bt-log-sink.md](docs/bt-log-sink.md) | TOTA/SPP phone logging |
| [docs/architecture-cros.md](docs/architecture-cros.md) | Longer-term CROS/BiCROS design |
| [docs/windows-flash.md](docs/windows-flash.md) / [bestool-windows.md](docs/bestool-windows.md) | Flashing |
| [docs/hardware.md](docs/hardware.md) | Hardware overview |
| [docs/architecture-noise-damping.md](docs/architecture-noise-damping.md) | Industrial damp (design only) |

## Known limits (honest)

- Not clinical; no gain prescription, no safety certification
- Codec is simple ADPCM — “phone call” character, not hi-fi
- Extra-path jitter buffer trades delay for stability (tuning next)
- Phone SPP logging must stay quiet during extra media (v0.3.16+); heavy log spam can still stress the ACL (`HCI_NUM_ACL_BUFFERS` is only 6 in this SDK tree)
- BiCROS / media mix / user-selectable poor side: not done

## License

Project docs and scripts: [LICENSE](LICENSE). SDK under `vendor/OpenPineBuds`: BES / OpenPineBuds terms — see [NOTICE](NOTICE).
