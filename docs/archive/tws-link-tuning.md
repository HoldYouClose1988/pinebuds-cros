# TWS link tuning lead (closed-library pass)

Behavior notes from inspecting OpenPineBuds closed `.a` objects and open
controller helpers. **No disassembly is published here.** Interop/RE notes only.

## Verified (this tree)

1. `libtws_ibrt_enhanced_stack_RTX.a` holds **12** relocatable, **named** Thumb
   objects (not stripped). Readable with `ar x` + `readelf -sW`.
2. Vendor OTA (`services/ota/ota_common.cpp`) tightens the TWS ACL for transfer
   speed: larger link duration, private poll interval from UI config, sniff off.
   Restore on OTA end.
3. `app_tws_ibrt_set_tws_pravite_interval` is **declared** and present in the
   closed IBRT object; **no callers** in open source or in the extracted libs
   we grepped. It ends in `btif_me_set_tws_poll_interval` → vendor HCI.
4. Open controller helpers (used by OTA) are the practical knobs:
   - `btdrv_reg_op_set_private_tws_poll_interval(poll, poll_in_sco)`
   - `btdrv_reg_op_set_tws_link_duration(slot_num)`
   - `bt_drv_reg_op_set_tpoll(linkid, interval)`
5. UI macros (`app_ibrt_customif_ui.h`): default poll `0x34`, in-SCO default
   `0x9c`, in-SCO short `0x3C`. Comment says “BES internal… DO NOT modify”.
   **Units assumed** 0.625 ms slots → in-SCO default ≈ **97.5 ms** (UNVERIFIED
   until live read-back).

## Not verified yet

- Which poll tier is live with BiCROS / peer SCO up.
- Whether `bt_drv_is_enhanced_ibrt_rom()` gates the HCI private-interval path
  on BES2300YP.
- Whether OTA-style tightening helps ACL (extra L2CAP) enough to beat eSCO,
  or only helps config/SPP while SCO is up.

## Product path

Current BiCROS remains peer **eSCO + mSBC + HW sidetone**. This lead is about
ACL/TWS scheduling under that path (and a possible future ACL-media experiment).

## Task status

| Task | Status |
|------|--------|
| **B** Read-back live poll / duration / tpoll | **In tree** — `cros_link_diag_log()` on enable, SCO OPENED, disable. Needs flash + log capture. |
| **A** SCO hop timing (capture→play) | Not started (existing `cros_lat` is ACL/extra-path only). |
| **C** Flag-gated OTA-style tighten on CROS enable | Not started — wait for B logs. |
| **D** Deeper RE of cmd-thread objects | Deferred |

## How to capture B

1. Build/flash with patch `0015-cros-tws-poll-readback.patch` applied (bootstrap).
2. Enable support log → Connect → Enable BiCROS → wait for OPENED.
3. Share lines tagged `[cros_link]` (`enable`, `sco-opened`, `disable`).

Look for: live `poll` / `poll_in_sco` vs cfg `sco=0x9c`, and whether `tpoll`
moves when SCO comes up.
