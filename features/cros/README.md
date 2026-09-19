# Feature: CROS for SSHL

Status: **Stage A implemented** (local FF loopback); Stage B/C (TWS sync + cross-bud audio) not started.

See [docs/architecture-cros.md](../../docs/architecture-cros.md) and [docs/monday-checklist.md](../../docs/monday-checklist.md).

## Stages

| Stage | Goal | Status |
|-------|------|--------|
| A | Same-bud FF mic → speaker loopback | Code + build ready; await hardware flash |
| B | Mode bit + TWS sync | Not started |
| C | Poor-ear TX → good-ear RX over TWS | Not started |

## Acceptance sketch (v0)

- [x] Stage A: FF capture + local play path builds and links
- [ ] Stage A: clap test pass on worn bud (hardware)
- [ ] Mode bit persisted + synced over TWS
- [ ] Poor-side FF mic capture running while CROS on
- [ ] Audio frames received on good side and audible
- [ ] Stock A2DP works with CROS off
- [ ] Call start disables or ducks CROS cleanly
