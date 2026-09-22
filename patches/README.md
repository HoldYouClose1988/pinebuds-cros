# Patch queue

Drop `*.patch` files here (generated against `vendor/OpenPineBuds` at the
pinned bootstrap ref). `scripts/bootstrap-sdk.sh` applies them in sorted order
(skips if already applied).

Keep patches minimal and feature-scoped.

| Patch | Purpose |
|-------|---------|
| `0001-stage-a-ff-loopback.patch` | Wire experimental `CROS_STAGE_A`: local FF mic loopback, quad-tap, FF mic as ASRMIC |
| `0002-stage-b-cros-tws.patch` | Wire experimental `CROS_STAGE_B`: cross-bud CROS over IBRT custom cmds, larger TWS ctrl buf |
| `0003-cros-direct-audio-send.patch` | Direct BESAUD send helper (skip tws_ctrl mailbox backlog) |
| `0004-cros-besaud-extra-hook.patch` | BESAUD disconnect cleanup for optional extra L2CAP (create gated off in firmware) |

Sources:
- Stage A: `firmware/stage_a/` → `apps/cros_loopback/`
- Stage B: `firmware/stage_b/` → `apps/cros_tws/` (`cros_tws` + `cros_besaud_extra`)
