# PineBuds Pro — CROS firmware (experimental)

> ## ⚠️ DIY / own-risk — not a hearing aid
>
> Experimental contralateral routing on [PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro).
> It is **not** a medical device, prescribed CROS/BiCROS, or certified hearing protection.
> Flash only if you can restore stock firmware. Keep a backup.

Custom OpenPineBuds-based firmware: **poor-side mic → good-side speaker** over a
bud↔bud link (plus local good-ear mic mix). Stock TWS / media / calls remain when
CROS is off. A separate **industrial damping** track is still design-only.

The BES SDK is **not** vendored here; `./scripts/bootstrap-sdk.sh` pulls [OpenPineBuds](https://github.com/pine64/OpenPineBuds) locally.

## Milestone (2026-09-26) — usable **BiCROS** ≈ **140 ms**

Ear-validated on hardware:

| | |
|--|--|
| **Transport** | Bud↔bud **SCO/eSCO** + stock HFP **mSBC 16 kHz** voice path |
| **Latency** | Clap ≈ **140 ms** (vs ≈ **330 ms** on extra L2CAP v0.3.27) |
| **Shape** | Poor mic → good speaker; good mic mixed locally (BiCROS) |
| **Knobs (v0.3.48)** | App: mix / EQ / volume / noise / poor side over TOTA |
| **Usability** | “Good quality phone call” sound, great latency, **mixing confirmed** |
| **Flash** | **[v0.3.64](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.64)** + app **0.4.2** (fw **0.3.65** source ready — `phone=` on status) |

Write-up: [CHANGELOG](CHANGELOG.md) · [latency scorecard](docs/latency-and-next.md) ·
[next steps](docs/latency-and-next.md#where-we-are--suggested-next-2026-09-26).

| Path | Clap (ear) | Role |
|------|------------|------|
| **SCO mSBC BiCROS (v0.3.61 baseline)** | ≈ **140 ms** | **Product baseline** |
| Extra L2CAP (v0.3.27) | ≈ **322–330 ms** | Legacy / fallback |

## Current status (v0.3.65 source + app 0.4.2; flash still v0.3.64)

**Audio baseline = v0.3.61.** **v0.3.63** NV knobs. **v0.3.64** `cros status`.
**App 0.4.2** warmer Help/FAQ + poor-ear warn uses phone-connected side.
**v0.3.65** (source) adds `phone=` to status — flash when next package is built.

| Mode | Status |
|------|--------|
| **Stock TWS** | Upstream OpenPineBuds when CROS is off |
| **SCO BiCROS** | **Ear PASS** — mSBC 16 kHz, ~140 ms, CROS + sidetone mix |
| **App knobs** | Mix / EQ / SCO DAC / noise / poor via [android/cros-log](android/cros-log/) (v0.3.8) |
| **Extra L2CAP CROS** | v0.3.27 freeze (~330 ms) — keep as fallback |
| **POOR/TX as IBRT master** | CROS **refused** (known crash) — poor side must be TWS **slave** |
| **Phone / tablet volume** | **Rocker** → BiCROS SCO DAC while CROS on; music (A2DP) when off |
| **Phone logs** | TOTA SPP + [android/cros-log](android/cros-log/) |
| **Industrial damp** | Not implemented (design only) |

### How we got here (SCO)

1. **v0.3.35** — first peer SCO **OPENED**; left up with extra → wedge  
2. **v0.3.36** — auto-close after OPENED; extra held  
3. **v0.3.37–38** — SCO alone held; silence under SCO (no ACL fight)  
4. **v0.3.39** — CVSD voice on peer SCO → **~140 ms**  
5. **v0.3.40** — asymmetric CROS mute (poor TX / good RX)  
6. **v0.3.41** — good-side HFP volume bump  
7. **v0.3.42** — **mSBC 16 kHz** usable call quality  
8. **v0.3.43** — **BiCROS** LEFT mic HW sidetone mix — **ear PASS**
9. **v0.3.44** — App knobs: mix / EQ / selectable poor side
10. **v0.3.45** — Fix knob howling (signed parse + mix ≤ −12 dB)
11. **v0.3.46** — Split SCO register/open + pause TOTA flush (Apply/taps)
12. **v0.3.47** — Phone ack for Apply/Get under SCO hold
13. **v0.3.48** — App volume + link-noise filter; DISABLE ack
14. **v0.3.49** — Tee resumes when SCO is down (no post-close mute)
15. **v0.3.50** — Ack-only under SCO + skip double open_link (taps/dropouts)
16. **v0.3.51** — Curated milestone ack queue (ENABLE/OPENED/shape on phone)
17. **v0.3.52** — Clean SCO teardown so BiCROS re-enables without case reset
18. **v0.3.53** — Wait for real SCO CLOSED (soft 4s×3); defer ENABLE while closing
19. **v0.3.54** — Default vol 11 + noise 3 (ear: 13 was amplifying SCO floor)
20. **v0.3.55** — SCO DAC default 8; Music (A2DP) knob; label fix
21. **v0.3.56** — Phone volume rocker → BiCROS SCO DAC while CROS on
22. **v0.3.57** — Persist hfp_vol so AbsVol rocker actually moves DAC
23. **v0.3.58** — Re-arm via BTEVENT disconnect + force cool-down
24. **v0.3.59** — Audible BiCROS status cues (stock tones) — **wedged AF (084801)**
25. **v0.3.60** — Status cues via SCO-PCM mix (no media_PlayAudio while SCO up)
26. **v0.3.61** — DISABLED/NOT_YET audible (media when voice down; cue-hold)
27. **v0.3.62** — Cue PASS confirmed; 75 s hold-escape vs stuck NOT_YET
28. **v0.3.63** — Knobs (incl. poor side) persist in bud NV + app confirm
29. **v0.3.64** — `cros status` + CROS Control 0.4.0 (status banner, log hidden)
30. **v0.3.65** — `phone=` on status + CROS Control 0.4.2 (copy rewrite, smart poor warn)

**v1.0 note:** configurable poor side must keep the IBRT-master×TX guard — see
[architecture-cros.md](docs/architecture-cros.md#hard-constraint-v0333--must-keep-for-v10).

Default mapping: **RIGHT = poor (mic / TX)**, **LEFT = good (speaker / RX)**.
Quad-tap toggles CROS (needs TWS link). **Poor side must not be IBRT master**
(keep LEFT as master with default mapping).

Latest zip: [`flash-packages/pinebuds-cros-v0.3.64.zip`](flash-packages/pinebuds-cros-v0.3.64.zip) · [CHANGELOG](CHANGELOG.md) · [VERSION](VERSION)

## Looking for review / next work

**Next:** installer. AbsVol left as-is.

Flash **[v0.3.64](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.64)** + build app **0.4.0** from `android/cros-log`.
Audio-only fallback: **[v0.3.61](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.61)**.

## How it works (short)

**Current path (v0.3.45) — peer SCO mSBC + BiCROS:**

```
RIGHT (poor)                              LEFT (good)
────────────                              ───────────
mic ──► mSBC / SCO ─────────────────────► mSBC ──┐
   (mic ON, spk OFF,                      (spk ON) ├──► speaker
    sidetone OFF)                         local mic ──┘  (HW sidetone)
                                          (SCO TX muted)
```

**Legacy path (v0.3.27) — extra L2CAP (~330 ms):**

```
RIGHT (poor)                         LEFT (good)
────────────                         ───────────
FF mic → 50 ms ADPCM ──extra L2CAP──► decode → speaker
```

Bring-up history: [docs/cros-transport.md](docs/cros-transport.md).

## Quick start (Windows flash)

See [Windows flashing](docs/windows-flash.md) and [bestool](docs/bestool-windows.md).

1. Download **[pinebuds-cros-v0.3.45.zip](flash-packages/pinebuds-cros-v0.3.45.zip)** (includes `bestool.exe`).
2. Backup once, then flash **both** buds:

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6
.\flash.ps1  -Port0 COM5 -Port1 COM6
```

3. Seat both buds in the case ~30–60 s so TWS re-pairs.
4. Wear both; **quad-tap** to toggle CROS. Speak near the **right** outer face — hear it in the **left** ear; left-side speech should also appear in the left ear (BiCROS mix).
5. Optional logs: [android/cros-log](android/cros-log/) → **Capture logs** on.

Avoid phone music while testing CROS (A2DP fights the stream). Prefer not starting
Phone SCO during the probe. Use **bud** volume keys (tablet HFP volume does not apply).

## Build from source

```bash
./scripts/bootstrap-sdk.sh
./scripts/build.sh          # includes SPEECH_SIDETONE=1 for BiCROS
./scripts/package-flash.sh  # TOTA=1 by default for phone logs
```

See [scripts](scripts/) and [docs](docs/).

## License

See [LICENSE](LICENSE) and [NOTICE](NOTICE).
