# PineBuds Pro — CROS firmware (experimental)

> ## ⚠️ DIY / own-risk — not a hearing aid
>
> Experimental contralateral routing on [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro).
> It is **not** a medical device, prescribed CROS/BiCROS, or certified hearing protection.
> Flash only if you can restore stock firmware. Keep a backup.

Poor-side mic → good-side speaker over a bud↔bud link, plus local good-ear mic
mix (BiCROS). Stock TWS / media / calls remain when CROS is off.

## Current (v0.3.65 + app 0.4.2)

| | |
|--|--|
| **Flash** | **[v0.3.65](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.65)** |
| **App** | [CROS Control 0.4.2](android/cros-log/) — knobs, status, Help FAQ |
| **Audio baseline** | v0.3.61 — SCO mSBC BiCROS ≈ **140 ms** |
| **Day to day** | Quad-tap toggles BiCROS (no app needed after Apply) |
| **Default ears** | RIGHT = poor (mic), LEFT = good (phone / master) |

| Mode | Status |
|------|--------|
| SCO BiCROS | Ear PASS — mSBC 16 kHz, ~140 ms |
| App knobs | Mix / EQ / SCO DAC / noise / poor — saved on buds (NV) |
| Poor = phone master | Enable **refused** (known crash) |
| Extra L2CAP CROS | Legacy freeze at v0.3.27 (~330 ms) |
| Industrial damp | Not implemented |

## Quick start (Windows)

1. Download **[pinebuds-cros-v0.3.65.zip](https://github.com/HoldYouClose1988/pinebuds-cros/releases/download/v0.3.65/pinebuds-cros-v0.3.65.zip)** (or from [`flash-packages/`](flash-packages/)).
2. Backup once, then flash **both** buds:

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6
.\flash.ps1  -Port0 COM5 -Port1 COM6
```

3. Seat both in the case ~30–60 s so TWS re-pairs.
4. Wear both; **quad-tap** to toggle BiCROS. Speak near the **right** outer face — hear it in the **left** ear.
5. Optional: build/install [android/cros-log](android/cros-log/) for knobs and Help.

Full flash notes: [docs/windows-flash.md](docs/windows-flash.md).

## How it works (short)

```
RIGHT (poor)                              LEFT (good)
────────────                              ───────────
mic ──► mSBC / SCO ─────────────────────► mSBC ──┐
   (mic ON, spk OFF,                      (spk ON) ├──► speaker
    sidetone OFF)                         local mic ──┘  (HW sidetone)
                                          (SCO TX muted)
```

Architecture + hard constraints: [docs/architecture-cros.md](docs/architecture-cros.md).

## Build from source

```bash
./scripts/bootstrap-sdk.sh   # pulls OpenPineBuds + ARM GCC locally (not vendored in git)
./scripts/build.sh           # SPEECH_SIDETONE=1 for BiCROS
./scripts/package-flash.sh   # TOTA=1 by default
```

See [docs/development.md](docs/development.md). Doc index: [docs/](docs/).

## Repo layout

| Path | Purpose |
|------|---------|
| `firmware/stage_b/` | BiCROS firmware sources |
| `android/cros-log/` | Phone control / support-log app |
| `flash-packages/` | Current + audio-baseline zips only |
| `docs/` | Current docs; bring-up journals in `docs/archive/` |
| `docs/BRANCHES.md` | **Android vs iOS firmware tracks** |
| `patches/` | OpenPineBuds integration patches |
| `CHANGELOG.md` | Full version history |
| GitHub Releases | All historical flash zips |

**Tracks:** [`main` / `android`](docs/BRANCHES.md) = Android-stable **v0.3.65**.  
[`cursor/ios-coexist-3d85`](docs/BRANCHES.md) = iPhone coexist work (**v0.4.x**).

## License

See [LICENSE](LICENSE) and [NOTICE](NOTICE).
