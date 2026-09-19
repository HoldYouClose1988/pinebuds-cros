# Feature: CROS for SSHL

**Status: design / not functional.** Cross-bud CROS audio is not implemented.

See [architecture](../../docs/architecture-cros.md).

## Bring-up roadmap

| Milestone | Goal | Status |
|-----------|------|--------|
| Local loopback | Same-bud FF mic → speaker (path proof) | Experimental code in tree; **unvalidated on hardware in this public tree** |
| Mode sync | Mode bit + TWS sync | Not started |
| Cross-bud CROS | Poor-ear TX → good-ear RX over TWS | Not started |

## Acceptance sketch (future)

- [ ] Mode bit persisted + synced over TWS
- [ ] Poor-side FF mic capture while CROS on
- [ ] Audio frames received on good side and audible
- [ ] Stock A2DP works with CROS off
- [ ] Call start disables or ducks CROS cleanly
