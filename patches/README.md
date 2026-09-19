# Patch queue

Drop `*.patch` files here (generated against `vendor/OpenPineBuds` at the
pinned bootstrap ref). `scripts/bootstrap-sdk.sh` applies them in sorted order
(skips if already applied).

Keep patches minimal and feature-scoped.

| Patch | Purpose |
|-------|---------|
| `0001-stage-a-ff-loopback.patch` | Wire `CROS_STAGE_A`: build `apps/cros_loopback`, boot auto-start, quad-tap toggle, FF mic as ASRMIC |

Stage A C sources live in `firmware/stage_a/` (synced into the vendor tree by bootstrap; not in the patch).
