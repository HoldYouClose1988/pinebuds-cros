# PineBuds Pro — CROS firmware (experimental)

> ## ⚠️ DIY / own-risk — not a hearing aid
>
> Experimental contralateral routing on [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro).
> It is **not** a medical device, prescribed CROS/BiCROS, or certified hearing protection.
> Flash only if you can restore stock firmware. Keep a backup.

Custom OpenPineBuds-based firmware: **poor-side FF mic → good-side speaker** over the bud↔bud link, while stock TWS / media / calls remain the baseline when CROS is off. A separate **industrial damping** track is still design-only.

The BES SDK is **not** vendored here; `./scripts/bootstrap-sdk.sh` pulls [OpenPineBuds](https://github.com/pine64/OpenPineBuds) locally.

## Current status (v0.3.30) — SCO READY-trigger + phone SCO reference on **extra baseline v0.3.27**

| Mode | Status |
|------|--------|
| **Stock TWS** | Upstream OpenPineBuds baseline when CROS is off |
| **Stage B CROS (extra L2CAP)** | **Usable baseline (v0.3.27)** — still the audio path |
| **SCO probe (v0.3.30)** | Open on peer READY (+ fallback); BTEVENT tee; phone SCO button in cros-log |
| **Phone logs** | TOTA SPP + [android/cros-log](android/cros-log/); **quiets** during extra media |
| **Latency chase** | §K: compare phone SCO BTEVENT vs peer `open_link` (see [latency-and-next.md](docs/latency-and-next.md)) |
| **Industrial damp** | Not implemented (design only) |

### Extra-pipe baseline (keep / build features on this)

**v0.3.27** remains the freeze point for **extra L2CAP** CROS audio. **v0.3.28–0.3.30**
add SCO link probes only; if SCO is unfruitful, feature work continues on 0.3.27.

| Metric | Result |
|--------|--------|
| Clap delay (start→start) | ≈ **322–330 ms** (mostly 200 ms jitter floor) |
| Cutouts | **None** on long Capture-on wear (2026-09-26); `underrun=41` / ~5 min |
| Capture logs + CROS | OK — quiet mode; no SPP underrun-threshold spam |
| Known ceiling | ACL delivery bursty (`rx_buf` 0–270 ms); closed BT stack blocks ACL-pool bump |

Experiment trail: v0.3.23 media · 0.3.24 B+H · 0.3.25 sniff · **0.3.27 baseline** (skip 0.3.26).

### Latency levers already tried on extra (do not repeat blindly)

| Lever | Outcome |
|-------|---------|
| Jitter floor 4→3 | Cutouts unusable (~2/s) |
| Frames 50→40 ms | No delay win; more chop |
| TX poll 50→10 ms | No delay win; cutouts returned |
| Sniff lock (G) | ACTIVE; no clap win; not the cutout fix |
| ACL buffer header (C) | **Blocked** — pool lives in closed `.a` |
| Quiet underrun SPP (0.3.27) | Stopped 0.3.25-class Capture storms |

Most of the 330 ms is the **200 ms jitter floor** required for stable extra under bursty ACL. Next latency bet is **SCO/eSCO** (§K), not thinning this floor again. Full scorecard: [docs/latency-and-next.md](docs/latency-and-next.md).

Default mapping: **RIGHT = poor (mic / TX)**, **LEFT = good (speaker / RX)**. Quad-tap toggles CROS (needs TWS link).

Latest zip: [`flash-packages/pinebuds-cros-LATEST.zip`](flash-packages/pinebuds-cros-LATEST.zip) · [CHANGELOG](CHANGELOG.md) · [VERSION](VERSION)

## Looking for review

Extra-path CROS is at a **usable baseline (v0.3.27)**. Eyes wanted on **bud↔bud SCO/eSCO**
(`sco_open_link` to TWS peer) and whether anything beats ~330 ms without wrecking IBRT.
See [latency-and-next.md](docs/latency-and-next.md). Flash **v0.3.27** for daily extra;
**v0.3.30** for SCO investigation (phone SCO button + READY open + BTEVENT tee).

## How it works (short)

```
RIGHT (poor)                         LEFT (good)
────────────                         ───────────
FF mic → 50 ms ADPCM ──extra L2CAP──► decode → speaker
                 ╲                   (cmd path if extra not READY)
                  └─ MODE sync on IBRT custom cmd
```

Bring-up history: [docs/cros-transport.md](docs/cros-transport.md).

## Quick start (Windows flash)

See [Windows flashing](docs/windows-flash.md) and [bestool](docs/bestool-windows.md).

1. Download **[pinebuds-cros-LATEST.zip](flash-packages/pinebuds-cros-LATEST.zip)** (includes `bestool.exe`).
2. Backup once, then flash **both** buds:

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6
.\flash.ps1  -Port0 COM5 -Port1 COM6
```

3. Seat both buds in the case ~30–60 s so TWS re-pairs.
4. Wear both; **quad-tap** to toggle CROS. Speak near the **right** outer face — hear it in the **left** ear.
5. Optional logs: [android/cros-log](android/cros-log/) → **Capture logs** on. After `peer READY` expect `[cros_log] quiet=1` (periodic stats suppressed on purpose).

Avoid phone music while testing CROS (A2DP fights the stream).

## Build from source

```bash
./scripts/setup-public-git.sh   # optional anonymous git identity
./scripts/bootstrap-sdk.sh      # OpenPineBuds + ARM GCC; syncs firmware/ + patches
TOTA=1 ./scripts/build.sh       # Stage B CROS + TOTA log sink
# → vendor/OpenPineBuds/out/open_source/open_source.bin
./scripts/package-flash.sh      # optional Windows zip under flash-packages/
```

Details: [docs/development.md](docs/development.md).

## Documentation

| Doc | Topic |
|-----|--------|
| [docs/latency-and-next.md](docs/latency-and-next.md) | **Latency scorecard + brainstorm (start here for review)** |
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
- ~**330 ms** glass-to-glass today; conversational CROS usually wants ≪100 ms
- Codec is simple ADPCM — telephone character, not hi-fi
- Extra path needs deep jitter under current ACL burstiness; thinner buffer = cutouts
- Heavy TOTA logging during extra media can kill the link (quiet mode mitigates)
- BiCROS / media mix / user-selectable poor side: not done

## License

Project docs and scripts: [LICENSE](LICENSE). SDK under `vendor/OpenPineBuds`: BES / OpenPineBuds terms — see [NOTICE](NOTICE).
