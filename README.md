# PineBuds Pro — BiCROS firmware (experimental)

> ## ⚠️ DIY / own-risk — not a hearing aid
>
> Experimental contralateral routing on [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro).
> It is **not** a medical device, prescribed CROS/BiCROS, or certified hearing protection.
> Flash only if you can restore stock firmware. Keep a backup.

## What this is

**BiCROS** for single-sided hearing loss on ~$70 PineBuds Pro: the **poor-side**
earbud’s mic is relayed to the **good-side** earbud’s speaker so you hear what’s
happening on the “deaf” side, while the good ear’s own mic is mixed in locally
so that side still sounds natural.

**ANDROID APK HAS TO BE SIDELOADED. FULL ANDROID APP SOURCE IS IN GITHUB UNDER ANDROID FOLDER**

Android APK is included in release zip. 

| | Default mapping |
|--|--|
| **Poor ear (mic / TX)** | **Right or Left (Configurable)** |
| **Good ear (speaker / RX + local mix)** | **Right or Left (Configurable)** (also the bud paired to the phone) |

Day to day: wear both buds, **quad-tap** to turn BiCROS on or off. After you
**Apply** knobs once in the Android app, settings live on the buds — no phone
app required for normal use. Stock TWS music / calls work when BiCROS is **off**
(disable BiCROS before taking a call).

## How it’s implemented

Custom firmware on top of [OpenPineBuds](https://github.com/pine64/OpenPineBuds)
(BES2300). Product path:

1. **Bud↔bud peer eSCO** carries 16 kHz **mSBC** from poor mic → good speaker
   (clap latency ≈ **140 ms**).
2. Good ear uses the stock **HFP voice player** for that SCO, with **HW sidetone**
   mixing the local mic into the same speaker (BiCROS, not CROS-only).
3. **Quad-tap** toggles mode over TWS; poor-as-phone-master is **refused** (known crash).
4. Optional **CROS Control** Android app (SPP/TOTA) sets mix / EQ / SCO level /
   noise / poor side; knobs persist in bud NV across case/reboot.
5. Audible **status cues** (ENABLED / DISABLED / NOT YET / READY / OPEN FAIL).

```
RIGHT (poor)                              LEFT (good)
────────────                              ───────────
mic ──► mSBC / SCO ─────────────────────► mSBC ──┐
   (mic ON, spk OFF,                      (spk ON) ├──► speaker
    sidetone OFF)                         local mic ──┘  (HW sidetone)
                                          (SCO TX muted)
```

**Supported phone: Android.** iPhone is not a daily driver (peer SCO fights iOS
HFP) — [Why not iPhone?](#why-not-iphone).

More architecture: [docs/architecture-cros.md](docs/architecture-cros.md).

## Current (v0.3.66 flash zip = fw 0.3.65 + app 0.4.5)

| | |
|--|--|
| **Flash** | **[v0.3.66](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.66)** (same BiCROS bin as v0.3.65) |
| **App** | [CROS Control](android/cros-log/) **0.4.5** — `CROScontrol.apk` inside the flash zip (sideload) |
| **Phone** | **Android** (iPhone not supported for daily wear) |
| **Audio baseline** | v0.3.61 — SCO mSBC BiCROS ≈ **140 ms** |
| **Day to day** | Quad-tap toggles BiCROS (no app needed after Apply) |
| **Default ears** | RIGHT = poor (mic), LEFT = good (phone / master) — configurable in app |

| Mode | Status |
|------|--------|
| SCO BiCROS on **Android** | Ear PASS — held ~20 min quiet + loud desk tests |
| App knobs | Mix / EQ / SCO DAC / noise / poor — saved on buds (NV) |
| Poor = phone master | Enable **refused** (known crash) |
| Extra L2CAP CROS | Legacy freeze at v0.3.27 (~330 ms) |
| **iPhone** | **Not supported** for daily wear (link drops) |
| Industrial damp | Not implemented |

## Why not iPhone?

BiCROS uses **bud↔bud eSCO** plus the stock **HFP voice player** (same class of
link iOS uses for calls). That fights how iPhone expects Bluetooth audio to work.

**Why iOS is harder:** Apple’s accessory guidelines treat HFP eSCO as an
*exclusive* call-like pipe (speaker + mic dedicated to that link). Music /
system sounds are supposed to use A2DP. Our peer SCO sits on the reserved voice
path iPhone thinks it owns, so iOS can half-route audio and later kill the ACL.
Android is less strict — which is why it works.

A parked experimental branch (`cursor/ios-coexist-3d85`, release **v0.4.0**)
pauses BiCROS when the phone opens HFP — **not** productized; daily use stays
Android + **v0.3.65**. Details: [docs/iphone.md](docs/iphone.md) ·
[docs/BRANCHES.md](docs/BRANCHES.md).

## Quick start (Windows)

1. Download **[pinebuds-cros-v0.3.66.zip](https://github.com/HoldYouClose1988/pinebuds-cros/releases/download/v0.3.66/pinebuds-cros-v0.3.66.zip)** (or from [`flash-packages/`](flash-packages/)).
2. Unzip, plug the case in USB (need two COM ports — [CH342 driver](http://www.wch-ic.com/downloads/CH343SER_EXE.html) if missing).
3. Run the guided installer (prints flashing instructions + menu):

```powershell
powershell -ExecutionPolicy Bypass -File .\Install.ps1
```

   Choose **Backup THEN flash** the first time. (Advanced: `.\backup.ps1` / `.\flash.ps1` — COM ports optional when exactly two are detected.)

4. Seat both buds in the case ~30–60 s so TWS re-pairs.
5. Pair / use with an **Android** phone. Wear both; **quad-tap** to toggle BiCROS.
6. Sideload **`CROScontrol.apk`** from the zip for knobs and Help (allow unknown apps).

Full flash notes: [docs/windows-flash.md](docs/windows-flash.md).

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
| `firmware/stage_b/` | BiCROS firmware sources (**v0.3.65** Android product bin) |
| `android/cros-log/` | CROS Control app source |
| `android/CROScontrol.apk` | Sideload APK (also inside the flash zip) |
| `flash-packages/` | Current + audio-baseline zips |
| `docs/` | Docs; bring-up journals in `docs/archive/` |
| `docs/iphone.md` | iPhone findings (unsupported) |
| `patches/` | OpenPineBuds integration patches |
| `CHANGELOG.md` | Version history |
| GitHub Releases | Flash zips (firmware + APK + Install.ps1) |

## License

Feel Free to fork and improve. Just give me credit.

See [LICENSE](LICENSE) and [NOTICE](NOTICE).
