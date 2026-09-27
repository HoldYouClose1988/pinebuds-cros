# Feature: CROS / BiCROS for SSHL

**Status: ear-validated BiCROS on peer SCO (v0.3.45).** DIY / experimental — not a medical device.

See [architecture](../architecture-cros.md), [latency + next](latency-and-next.md), and [transport notes](cros-transport.md).

## Current path (v0.3.45)

| | |
|--|--|
| Transport | Bud↔bud **SCO/eSCO** + stock HFP **mSBC 16 kHz** |
| Latency | Clap ≈ **140 ms** |
| Shape | RIGHT (poor) mic → LEFT (good) speaker |
| BiCROS | LEFT mic mixed locally via **HW sidetone** (−20 dB) |
| Guard | POOR/TX must **not** be IBRT master |

Flash: [v0.3.45](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.45).

Legacy extra-L2CAP path (≈330 ms): freeze at **v0.3.27**.

## Bring-up roadmap

| Milestone | Goal | Status |
|-----------|------|--------|
| Local loopback | Same-bud FF mic → speaker | Stage A — validated |
| Mode sync | Mode bit + TWS sync | Done (IBRT custom cmd) |
| Extra L2CAP CROS | Poor TX → good RX over ACL | **v0.3.27** freeze (~330 ms) |
| Peer SCO + media | OPENED + HFP voice on peer SCO | **v0.3.39** ~140 ms |
| CROS mute shape | Asymmetric mic/spk | **v0.3.40** |
| mSBC 16 kHz | Usable call-like quality | **v0.3.42** |
| BiCROS mix | Good-ear local mic + SCO | **v0.3.45 PASS** |

## Usage (v0.3.45)

1. Flash **both** buds with the same package.
2. Leave in case ~30–60 s so TWS re-pairs.
3. Wear both. **Quad-tap** to toggle CROS.
4. Default: **RIGHT = poor**, **LEFT = good** (keep LEFT as IBRT master).
5. RIGHT speech → LEFT ear; LEFT speech → LEFT ear (local mix). Use **bud** volume keys.

Build flag `CROS_POOR_IS_RIGHT=0` swaps sides — keep the master×TX refuse for v1.0.

## Acceptance sketch

- [x] Mode bit synced over TWS
- [x] Peer SCO OPENED + media (mSBC)
- [x] Glass-to-glass latency ~140 ms (clap)
- [x] CROS shape (poor TX / good RX)
- [x] BiCROS local mix (HW sidetone)
- [ ] Longer wear / mix-gain presets
- [ ] EQ / smoothing without latency cost
- [ ] Configurable poor side (with master×TX guard)
- [ ] Stock A2DP / call edge cases polished
