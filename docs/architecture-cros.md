# CROS / SSHL architecture

## Implementation status (firmware)

**Stage B CROS is experimentally working** — baseline flash **v0.3.23** (see root [README](../README.md)):

- RIGHT FF mic → 50 ms IMA-ADPCM → BESAUD extra L2CAP → LEFT speaker
- MODE sync on IBRT custom cmd; cmd-path audio as fallback until peer READY
- Phone TOTA logging with quiet-during-extra (required for coexistence)
- Clap start→start ≈ **330 ms**; jitter floor 4 × 50 ms is required for usable stability today

Still **not** a clinical product. Missing vs this design doc: BiCROS mix, media ducking, user-selectable poor side, ≪100 ms glass-to-glass, prescribed gain/limiting UX.

**Latency dead-ends and next ideas:** [latency-and-next.md](latency-and-next.md).

## Clinical intent (non-medical framing)

**SSHL** here means single-sided hearing loss / single-sided deafness: one ear unaidable, the other usable.

**CROS** (contralateral routing of signals): mic on the poor side wirelessly feeds the good-side receiver so the wearer hears “around” the deaf side.

**BiCROS**: same routing, plus amplification / mix on the better ear when that ear also needs help — including mix of CROS mic + phone A2DP/HFP into the good bud.

This project is **experimental consumer firmware**, not a regulated hearing instrument. Latency, gain, and safety limiting must be treated as engineering constraints, not clinical claims.

## Why PineBuds Pro is a plausible platform

- Independent SoC + mics in each bud (true TWS, not one-mic dongle)
- Existing **bud↔bud** link (BES IBRT / `services/app_tws`) for sync and relay
- FF mic suitable for ambient pickup on the poor side
- OpenPineBuds already distinguishes L/R in hardware and keeps stock-like touch controls

Phone-relay CROS (capture SCO on phone, pan to one ear) exists in hobby apps but adds latency, call-mode quirks, and fails when the phone is away. **On-bud CROS** is the right long-term design.

## Target behavior

```
Impaired side bud                    Better side bud
─────────────────                    ───────────────
FF mic capture  ──PCM/comp over──►   decode → mix → speaker
(optional mute local DAC)            local A2DP/HFP optional
                                     local FF optional (BiCROS)
```

### Modes

| Mode | Poor-side DAC | Good-side mix | Phone |
|------|---------------|---------------|--------|
| Off (stock) | normal TWS | stereo as stock | A2DP/HFP |
| CROS | muted or very low | remote mic only (+ optional sidetone) | optional duck |
| BiCROS | muted | remote mic + local mic + media | full |
| Media priority | CROS ducked | media + attenuated CROS | A2DP focus |

Configurable which physical side is “poor” (NV record + touch gesture). Default: left = poor (common SSD pattern; user-selectable).

## Latency budget

Useful CROS for conversation awareness generally wants **&lt; 30–40 ms** glass-to-glass; BES TWS voice paths (mSBC-class) are the reference. Prefer:

1. Existing IBRT voice/PCM relay primitives if present in the closed BT stack
2. Else a dedicated TWS cmd + compressed frame path (LC3 if available in tree; else mSBC / custom ADPCM)
3. Avoid routing through the phone

Measure with a clap test (FF mic → good speaker) once a prototype path exists.

## Integration points in OpenPineBuds

| Area | Path | Role |
|------|------|------|
| Side detect | `app_tws` / board resistor | Know L vs R |
| Mic open | `hal_aud` / `tgt_hardware` | Open FF on poor side without full ANC if needed |
| Transport | `services/app_tws`, IBRT UI | Reliable framed audio + mode sync |
| Mix / play | `apps/audioplayers`, `app_bt_stream` | Inject CROS into good-side render |
| Persist | `services/nvrecord` | Poor-side choice, gain, mode |
| UX | `apps/key` touch codes | Toggle CROS / cycle gains |

ANC binary blob remains opaque; CROS should **not** depend on finished ANC. FF ADC → software path is enough for v0.

## Stock functionality retention

- Keep A2DP/AVRCP/HFP state machines untouched when CROS is off
- When CROS is on and a call starts: either (a) pause CROS and use HFP as stock, or (b) BiCROS-mix call downlink on good ear only — pick (a) for v1
- Docking / role switch: disable CROS TX if poor bud is charging; if good bud docks, stop playback cleanly
- Touch map: add a dedicated multi-tap or long-press chord for CROS so media controls stay familiar

## Safety / UX constraints

- Soft-knee limiter on CROS path (shared with industrial damp work)
- Max gain cap in NV; no unbounded “hearing aid” boost
- Audible mode prompt (reuse opus alerts under `config/_default_cfg_src_/res/`)
- Clear README: not a substitute for prescribed CROS / BiCROS devices

## Implementation slices

| Slice | Status |
|-------|--------|
| 1. Mode plumbing (NV/touch/TWS sync) | Done (quad-tap + peer MODE cmd; poor side compile-time default RIGHT) |
| 2. Loopback prototype (Stage A) | Done earlier; optional bring-up |
| 3. Cross-bud audio + measure latency | **Done on extra L2CAP** (~330 ms start→start); further cuts blocked on burstiness — see [latency-and-next.md](latency-and-next.md) |
| 4. Good-ear mix with A2DP ducking | Not started |
| 5. BiCROS / presets / power polish | Not started |
