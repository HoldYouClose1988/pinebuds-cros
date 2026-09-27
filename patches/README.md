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
| `0004-cros-besaud-extra-hook.patch` | BESAUD disconnect cleanup for extra L2CAP (create deferred to CROS activate) |
| `0005-tota-force-test-over-the-air.patch` | Make `TOTA=1` actually build on open_source: force `TEST_OVER_THE_AIR=1`, stub ANC tool when `APP_ANC_TEST=0`, fix missing stdint includes |
| `0006-cros-sniff-lock.patch` | While `cros_tws_is_enabled()`, `app_ibrt_if_tws_sniff_allowed()` returns false (G) |
| `0007-cros-sco-btevent-log.patch` | Tee `BTEVENT_SCO_*` from `app_bt_global_handle` (IBRT skips sniff_manager) |
| `0009-cros-tota-string-rx.patch` | Route unencrypted `OP_TOTA_STRING` RX → `cros_cfg` (phone knobs) |
| `0010-cros-sidetone-gain.patch` | Runtime `hal_codec_sidetone_set_gain_db` for BiCROS mix |
| `0011-cros-sco-eq-hook.patch` | SCO playback post-handler → soft bass/treble EQ |
| `0012-cros-ibrt-cfg-cmd.patch` | Peer sync `APP_IBRT_CUSTOM_CMD_CROS_CFG` |
| `0013-cros-absvol-to-sco.patch` | While BiCROS on, AVRCP Absolute Volume → SCO DAC (`cros_cfg_on_abs_volume`) |
| `0014-cros-sco-btevent-teardown.patch` | `BTEVENT_SCO_DISCONNECT` → `cros_sco_on_hci_disconnect` (peer SCO teardown) |
| `0015-cros-phone-coexist.patch` | **iOS track:** HF audio / non-peer SCO → `cros_phone_*` (pause BiCROS) |

Sources:
- Stage A: `firmware/stage_a/` → `apps/cros_loopback/`
- Stage B: `firmware/stage_b/` → `apps/cros_tws/` (`cros_tws` + `cros_besaud_extra` + `cros_bt_log` + `cros_cfg`)

Phone log sink + knobs (no new patch beyond 0009): `TOTA=1 ./scripts/build.sh` registers stock
TOTA SPP (RFCOMM 12). See [docs/archive/bt-log-sink.md](../docs/archive/bt-log-sink.md).
