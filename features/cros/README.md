# Feature: CROS for SSHL

**Status: experimental Stage B in tree.** Cross-bud CROS (poor FF mic → good speaker over TWS) is implemented for bring-up; not a finished product.

See [architecture](../../docs/architecture-cros.md) and [transport notes](../../docs/cros-transport.md).

## Bring-up roadmap

| Milestone | Goal | Status |
|-----------|------|--------|
| Local loopback | Same-bud FF mic → speaker (path proof) | Stage A — validated on hardware |
| Mode sync | Mode bit + TWS sync | Stage B — mode byte over IBRT custom cmd |
| Cross-bud CROS | Poor-ear TX → good-ear RX over TWS | Stage B — **extra L2CAP** (v0.3) + cmd fallback |

## Stage B usage (v0.3)

1. Flash **both** buds with the same package (`CROS_STAGE_B=1`).
2. Leave in case ~30s so TWS re-pairs (also creates BESAUD extra channel).
3. Wear both. **Quad-tap either bud** to toggle CROS on/off (synced to peer).
4. Default: **RIGHT = poor (mic)**, **LEFT = good (speaker)**. Scratch/outer sound at the right bud should be heard in the left ear.

Build flag `CROS_POOR_IS_RIGHT=0` swaps sides.

## Acceptance sketch

- [x] Mode bit synced over TWS
- [x] Poor-side FF mic capture while CROS on
- [ ] Glass-to-glass latency measured / tuned
- [ ] Stock A2DP verified with CROS off
- [ ] Call start disables or ducks CROS cleanly
- [ ] Compressed codec (mSBC) if raw PCM saturates ACL
