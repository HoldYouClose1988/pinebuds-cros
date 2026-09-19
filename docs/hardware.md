# PineBuds Pro hardware

Source of truth: [PINE64 wiki — PineBuds Pro](https://wiki.pine64.org/wiki/PineBuds_Pro), earbud schematic `20220817`, case schematic `20220915`, BES2300-YP datasheet.

## System layout

```
┌──────────────────────────── Charging case ────────────────────────────┐
│  USB-C ──► CH342DS (USB → dual UART) ──► pogo pins L / R              │
│  SY8821 charge / safety reset (button under lid between seats)        │
│  Battery ~800 mAh                                                     │
└───────────────────────────────────────────────────────────────────────┘
         │ pogo (charge + UART programming)
┌────────▼────────┐                 ┌────────▼────────┐
│  Left bud       │◄──── TWS/IBRT ──►│  Right bud      │
│  BES2300YP      │     Bluetooth    │  BES2300YP      │
│  3× MEMS mics   │                  │  3× MEMS mics   │
│  6 mm driver    │                  │  6 mm driver    │
│  40 mAh LiPo    │                  │  40 mAh LiPo    │
└─────────────────┘                  └─────────────────┘
```

Each bud is a full SoC + RF + codec. The case is primarily power + dual UART bridges for flashing/debug; it is not the audio brain.

## SoC (per bud): BES2300YP

| Item | Value |
|------|--------|
| CPU | Dual-core Cortex-M4F @ up to 300 MHz |
| ROM | 448 KB |
| SRAM | 992 KB (+ 64 KB BT shared) |
| Flash | 4 MB on-chip serial flash |
| BT | 5.2 dual-mode; A2DP, AVRCP, HFP; SBC + AAC |
| Audio | HiFi DAC/ADC, FF/hybrid ANC engine, dual-mic NR, VAD |
| Package | 4.5 × 6.2 mm 80-ball BGA |

BES IBRT (Intelligent Bluetooth Retransmission) is the stereo / TWS link technology between phone, primary, and secondary bud. OpenPineBuds configures L/R from a board resistor rather than only master/slave pairing.

## Microphones (critical for CROS + ANC)

Schematic / OpenPineBuds `config/open_source/tgt_hardware.h` mapping:

| Role | Hardware | Bias | Notes |
|------|----------|------|--------|
| ANC feed-forward | MIC1 | VMIC2 | Outward; picks ambient / “deaf side” scene for CROS |
| ANC feed-back | MIC3 | VMIC2 | Inward / error mic for hybrid ANC |
| Talk / voice | MIC5 | VMIC3 | Call uplink, VAD; lower-power path |

OpenPineBuds channel macros (L-only per bud; each ear runs as mono “left” config):

- `ANC_FF_MIC` → `AUD_CHANNEL_MAP_CH0`
- `ANC_FB_MIC` → `AUD_CHANNEL_MAP_CH2`
- Talk / sidetone path uses `AUD_CHANNEL_MAP_CH4` class mapping in the open_source target

CROS should prefer the **FF mic** for scene pickup on the impaired side (directional awareness). Talk mic is a fallback if FF path is locked to the ANC blob.

## Transducer & enclosure

- 6 mm dynamic driver, IPX4 (splash only — not construction-rated sealing)
- Capacitive touch on outer face (logo area)
- Passive isolation is modest; “industrial damp” cannot rely on seal alone — needs active attenuation + limiting

## Programming / debug

1. Buds seated in case, case USB-C to host → two ACM serial ports (CH342).
2. Wake: remove buds ~3 s, reseat; or long-hold rear button (~5 s) in OpenPineBuds to force reboot for programmer catch.
3. Tooling: [`bestool`](https://github.com/Ralim/bestool) `write-image` / backup helpers from OpenPineBuds.
4. Debug UART baud in OpenPineBuds raised to **2_000_000** to match stock.

Flash endurance is limited (>500 erase cycles quoted). Prefer OTA / careful iteration once a recovery path is verified.

## What stock firmware already does well

- Classic TWS media + calls
- Hybrid ANC / ENC / talk-through hooks in the BES audio graph (calibration still weak in open firmware)
- IBRT role switch when one bud docks

Our work reuses that stack: do not reinvent BT profiles; add a **CROS stream user** on the TWS/IBRT path and a **damping program** on the ANC / playback graph.
