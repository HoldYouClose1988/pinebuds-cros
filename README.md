# PineBuds Pro — CROS firmware (experimental)

> ## ⚠️ DIY / own-risk — not a hearing aid
>
> Experimental contralateral routing on [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro).
> It is **not** a medical device, prescribed CROS/BiCROS, or certified hearing protection.
> Flash only if you can restore stock firmware. Keep a backup.

Custom OpenPineBuds-based firmware: **poor-side FF mic → good-side speaker** over the bud↔bud link, while stock TWS / media / calls remain the baseline when CROS is off. A separate **industrial damping** track is still design-only.

The BES SDK is **not** vendored here; `./scripts/bootstrap-sdk.sh` pulls [OpenPineBuds](https://github.com/pine64/OpenPineBuds) locally.

## Breakthrough (2026-09-26) — bud↔bud SCO media ≈ **140 ms**

Peer **SCO/eSCO** between the buds carries live CVSD voice. Ear clap ≈ **140 ms**
vs ≈ **330 ms** on the older extra-L2CAP path — roughly **half the delay**, link held
steady. Quality is still call-path rough (CVSD); that is next. Details:
[CHANGELOG 0.3.39](CHANGELOG.md#0339--2026-09-26) · [latency scorecard](docs/latency-and-next.md) ·
[release](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.41).

| Path | Clap (ear) | Role |
|------|------------|------|
| **SCO CVSD (v0.3.41)** | ≈ **140 ms** | **Latency path** (quality TBD) |
| Extra L2CAP (v0.3.27) | ≈ **322–330 ms** | Daily / quality baseline until SCO is wear-ready |

## Current status (v0.3.41) — SCO CROS shape (poor→good)

| Mode | Status |
|------|--------|
| **Stock TWS** | Upstream OpenPineBuds baseline when CROS is off |
| **SCO CROS (bud↔bud)** | **~140 ms** + CROS shape; **v0.3.41** louder HFP vol on good side |
| **Stage B CROS (extra L2CAP)** | **Usable baseline (v0.3.27)** — still daily wear until SCO quality lands |
| **POOR/TX as IBRT master** | CROS **refused** (known crash) — poor side must be TWS **slave** |
| **Phone logs** | TOTA SPP + [android/cros-log](android/cros-log/); **quiets** during media |
| **Industrial damp** | Not implemented (design only) |

### How we got here (SCO)

1. **v0.3.35** — first peer SCO **OPENED** (`slave_open=1`); left up with extra → wedge  
2. **v0.3.36** — auto-close after OPENED; extra held  
3. **v0.3.37–38** — SCO alone held (~2 min silence); cmd ACL under SCO chops  
4. **v0.3.39** — HFP CVSD voice on peer SCO → **~140 ms clap**  
5. **v0.3.40** — asymmetric mute: poor mic→SCO, good SCO→speaker (CROS shape)  
6. **v0.3.41** — bump good-side HFP/SCO volume (was very quiet)

**v1.0 note:** configurable poor side must keep the IBRT-master×TX guard — see
[architecture-cros.md](docs/architecture-cros.md#hard-constraint-v0333--must-keep-for-v10).

### Extra-pipe baseline (still useful)

**v0.3.27** remains the freeze for **extra L2CAP** when you want stable daily audio
and Capture-friendly wear. Most of its ~330 ms is a **200 ms jitter floor** forced by
bursty ACL — do not thin that floor again. Full lever history:
[docs/latency-and-next.md](docs/latency-and-next.md).

| Metric (extra) | Result |
|----------------|--------|
| Clap delay | ≈ **322–330 ms** |
| Cutouts | **None** on long Capture-on wear; `underrun=41` / ~5 min |
| Known ceiling | ACL bursty; closed BT stack blocks ACL-pool bump |

Default mapping: **RIGHT = poor (mic / TX)**, **LEFT = good (speaker / RX)**.
Quad-tap toggles CROS (needs TWS link). **Poor side must not be IBRT master**
(with default mapping: keep LEFT as master).

Latest zip: [`flash-packages/pinebuds-cros-v0.3.41.zip`](flash-packages/pinebuds-cros-v0.3.41.zip) · [CHANGELOG](CHANGELOG.md) · [VERSION](VERSION)

## Looking for review

**SCO latency (~140 ms) + CROS shape (0.3.40) are in.** Eyes wanted on **direction**
(poor→good only, not call duplex) and stability. **Quality last** (MSBC / FF mic).
Extra path still at v0.3.27 for daily wear. Constraint for v1.0 configurable poor side:
[architecture-cros.md](docs/architecture-cros.md#hard-constraint-v0333--must-keep-for-v10).

Flash **[v0.3.41](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.41)** for SCO CROS shape;
**[v0.3.27](flash-packages/pinebuds-cros-v0.3.27.zip)** for daily extra-only.

## How it works (short)

**Latency / CROS path (v0.3.41) — peer SCO + CVSD, asymmetric mute:**

```
RIGHT (poor)                         LEFT (good)
────────────                         ───────────
FF/call mic ──► CVSD / SCO ────────────► CVSD / speaker
   (POOR: mic ON, spk OFF)     (GOOD: mic OFF, spk ON)
```

**Legacy daily path (v0.3.27) — extra L2CAP:**

```
RIGHT (poor)                         LEFT (good)
────────────                         ───────────
FF mic → 50 ms ADPCM ──extra L2CAP──► decode → speaker
                  └─ MODE sync on IBRT custom cmd
```

Bring-up history: [docs/cros-transport.md](docs/cros-transport.md).

## Quick start (Windows flash)

See [Windows flashing](docs/windows-flash.md) and [bestool](docs/bestool-windows.md).

1. Download **[pinebuds-cros-v0.3.41.zip](flash-packages/pinebuds-cros-v0.3.41.zip)** (includes `bestool.exe`) — SCO ~140 ms.
   For daily extra-only quality, use **[v0.3.27](flash-packages/pinebuds-cros-v0.3.27.zip)** instead.
2. Backup once, then flash **both** buds:

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6
.\flash.ps1  -Port0 COM5 -Port1 COM6
```

3. Seat both buds in the case ~30–60 s so TWS re-pairs.
4. Wear both; **quad-tap** to toggle CROS. Speak near the **right** outer face — hear it in the **left** ear.
5. Optional logs: [android/cros-log](android/cros-log/) → **Capture logs** on.

Avoid phone music while testing CROS (A2DP fights the stream). On SCO builds, prefer
not starting Phone SCO during the probe.

## Build from source

```bash
./scripts/bootstrap-sdk.sh
./scripts/build.sh
./scripts/package-flash.sh   # TOTA=1 by default for phone logs
```

See [scripts](scripts/) and [docs](docs/).

## License

See [LICENSE](LICENSE) and [NOTICE](NOTICE).
