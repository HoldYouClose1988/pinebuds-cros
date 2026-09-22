# CROS transport notes (Stage B)

## What we learned on the control channel

Custom `APP_IBRT_CUSTOM_CMD_*` over BESAUD **works** for CROS audio, but it is a
**command/control** path, not an isochronous media pipe:

| Cadence | Result (ear test) |
|---------|-------------------|
| ~20 ms cmds | Audible chop |
| ~60 ms cmds | Smooth, but robotic + ~1 s delay |
| ~40 ms continuous ADPCM via `tws_ctrl` (v0.2.7) | Usable; some chop |
| ~50 ms continuous ADPCM (v0.3.0) | Ear-preferred on cmd path |
| ~40 ms + `send_now` from osTimer (v0.2.6) | **Hung TX bud** (solid blue, quad-tap dead) |

Cmd-path tuning is **done** — that pipe is as good as it gets. Audio must leave
the ticker through **`tws_ctrl_send_cmd`** (fallback) or the **extra L2CAP**,
never direct BESAUD `send_now` from osTimer context.

## Important correction about “TWS audio sync”

A2DP stereo between buds is **not** “left bud relays PCM to right.”
Both buds sniff the **phone’s** ACL; BESAUD carries **sync/control**.
Piggybacking CROS onto `app_tws_ibrt_audio_sync_*` / A2DP decoder inject is the
wrong model for mic→peer speaker.

## Current pipe: BESAUD extra L2CAP (v0.3.0)

| Piece | Where |
|-------|--------|
| CID | `L2CAP_BESAUD_EXTRA_CHAN_ID` `0x0b0e` (`l2cap_i.h`) |
| Create / send / recv | `firmware/stage_b/cros_besaud_extra.c` via `l2cap_create_besaud_extra_channel` |
| Hook | `BTIF_BTEVENT_BES_AUD_CONNECTED` / `DISCONNECTED` in `app_ibrt_customif_ui.cpp` |
| MODE | Still `APP_IBRT_CUSTOM_CMD_CROS_MODE` on cmd path |
| Audio fallback | `APP_IBRT_CUSTOM_CMD_CROS_AUDIO` via `tws_ctrl` if extra not open |

Stock `tws_besaud_create_extra_channel` registers TRACE-and-discard RX — **not used**.

MTU ~679 B. Send is posted into the BT thread (`app_bt_start_custom_function_in_bt_thread`);
inflight gated by `L2CAP_CHANNEL_TX_HANDLED`.

## Non-starters

- SCO/eSCO between buds (phone call sniffer path)
- Feeding mic PCM into A2DP SBC store APIs
- Calling `app_ibrt_send_cmd_without_rsp` / `send_now` from osTimer
- More cmd-path micro-tuning

## Status

- **Now (v0.3.4):** deferred extra-L2CAP create on CROS activate; cmd fallback; 50 ms ADPCM.
- **Prior art:** no public CROS guide; commercial BES keeps extra-channel API (see Prior art).

## Prior art (web / GitHub survey, 2026-09-22)

**No published PineBuds / OpenPineBuds CROS or bud↔bud mic-relay recipe.** Closest community thread ([OpenPineBuds #69](https://github.com/pine64/OpenPineBuds/discussions/69)) is about recording a mic *to the phone*, not peer playback.

| Source | What it teaches |
|--------|-----------------|
| OpenPineBuds / forks | `tws_besaud_create_extra_channel` / `L2CAP_BESAUD_EXTRA_CHAN_ID` appear in headers only — **no open `.c` callers** found |
| Newer BES dumps (2600/2700) | Same API; CID often **`0xbbee`** instead of our **`0x0b0e`** |
| [OPPO ENCO firmware RE](https://github.com/GreenTeodoro839/OPPOEarPhoneForHyperOS/blob/main/.research/firmware-analysis/20_multi_link.md) | Shipping BES product: BESAUD = bud↔bud control PSM; `l2cap_create_besaud_extra_channel` explicitly described as **extra CIDs for non-control traffic** on top of primary BESAUD |
| Chinese BES blogs (52Bluetooth / CSDN) | Custom IBRT cmds for UI sync (same cmd-path pattern we already maxed out); music is phone ACL sniff + IBRT, not mic relay |
| Commercial “translate earbuds” marketing | Phone in the loop (HFP/A2DP), not a low-latency bud↔bud CROS pipe |

**Takeaway:** We are likely first on the open side for CROS. Extra L2CAP is still the right bet — commercial firmware keeps the API for non-control traffic — but there is no copy-paste guide. Probe carefully (deferred create).

## Options for next session

Survey of OpenPineBuds transports that could carry continuous mic PCM / compressed
frames **bud↔bud** without depending on `APP_IBRT_CUSTOM_CMD_*` / `tws_ctrl`
rate limits. Ranked by “worth probing for CROS” (not by code maturity).

### Unused CIDs / PSMs / test send paths already in tree

| Item | Where | Notes |
|------|-------|-------|
| CID `0x0b0e` | `vendor/OpenPineBuds/services/bt_profiles_enhanced/inc/l2cap_i.h` (`L2CAP_BESAUD_EXTRA_CHAN_ID`) | Only extra CID defined. Stock create hardcodes it. |
| PSM `0x0033` | same header (`PSM_BESAUD`) | Extra channel stamps this PSM on a fixed-CID channel (see disasm of `l2cap_create_besaud_extra_channel`). |
| Stock extra wrappers | closed `app_tws_besaud.o` in `services/ibrt_core/lib/libtws_ibrt_*.a`; decls in `services/ibrt_core/inc/app_tws_besaud.h` | `tws_besaud_create_extra_channel` / `_send_data` / `_receive_data`. RX TRACE string: `"besaud extra channel test data"` — intentional test pipe, discard RX. |
| `l2cap_pts_send_l2cap_data` | `l2cap_i.h`; key hook in `services/bt_app/app_bqb_new_profile.cpp` | PTS/BQB test send; not a streaming API. |
| `app_spp_test_client_open` | `services/bt_app/app_spp.cpp` (`#if 0`) | Dead SPP client→remote open path. |
| `btif_l2cap_send_data_to_peer_tws` | `services/bt_if_enhanced/inc/l2cap_api.h` (when `IS_USE_INTERNAL_ACL_DATA_PATH` is false) | Declared; **not present** in linked `ibrt_libbt_api_*.a` for this build — header-only dead end unless another lib provides it. |
| `btif_me_send_data_to_peer_dev` | `me_api.h` (only if `IS_USE_INTERNAL_ACL_DATA_PATH`) | Opposite `#if` branch; macro unused in open configs. |
| HCI ECC data test | `HCI_DBG_SET_ECC_DATA_TEST_CMD_OPCODE` `0xFCBB`; `btif_me_set_ecc_ibrt_data_test`; evt `HCI_DBG_ECC_DATA_TEST_EVT_SUBCODE` `0x15` in `app_vendor_cmd_evt.h` | Controller “blast patterned ACL” test, not app payload API. |
| Dual-mic recording cmd | `APP_IBRT_CUSTOM_CMD_DMA_AUDIO` in `app_ibrt_customif_cmd.*` under `__DUAL_MIC_RECORDING__` | **Same IBRT cmd pipe** as CROS today; sources for `app_recording_*` missing from tree. Useful as flow-control reference (`tx_done`), not a new pipe. |

No other proprietary PSMs or extra CIDs appear in open headers. Dynamic PSM registration exists (`l2cap_register` / `l2cap_open` in `l2cap_i.h`).

### Ranked candidates

#### 1. Deferred BESAUD extra L2CAP (CID `0x0b0e`) — **primary**

- **Symbols / files:** `l2cap_create_besaud_extra_channel`, `l2cap_send_data`; `apps/cros_tws/cros_besaud_extra.c`; stock `tws_besaud_*extra*` (closed); CID/PSM in `l2cap_i.h`.
- **Designed for:** BES “extra channel test data” beside the BESAUD control channel on the TWS ACL (fixed CID, PSM `0x0033`).
- **CROS fit:** Best match — MTU ~679 B, no `tws_ctrl` mailbox, TX_HANDLED flow control already sketched. Failure mode was **when** we created it (BESAUD-up), not the CID itself.
- **Risk vs failed on-connect create:** **Lower** if deferred until TWS+IBRT idle (e.g. after `app_tws_if_tws_connected_handler` + N seconds, or first CROS activate). Same API, so residual IBRT-race risk remains.
- **Smallest safe probe:** Keep `CROS_EXTRA_L2CAP=0` default. Add a one-shot create from BT thread **≥5–10 s after** stable BESAUD, gated by `tws_besaud_is_connected()` + box-out + no role-switch; log OPEN/CLOSED only; send 1× 16 B ping on activate; no continuous audio until OPEN seen on both buds.

#### 2. Same create API, alternate fixed CID

- **Symbols / files:** `l2cap_create_besaud_extra_channel(remote, channel_id, …)` — `channel_id` is a parameter; stock passes `0x0b0e`.
- **Designed for:** Same fixed-CID helper; only `0x0b0e` named in tree.
- **CROS fit:** Might avoid a conflict if `0x0b0e` collides with something IBRT already expects — speculative.
- **Risk:** **Similar or higher** than (1); peer must agree on CID; unknown if controller/stack whitelists only `0x0b0e`.
- **Smallest safe probe:** Only after (1) still breaks TWS when deferred. Try CID `0x0b0f` (or `0x0b10`) with identical deferred timing; both buds same build.

#### 3. New dynamic L2CAP PSM on the TWS ACL

- **Symbols / files:** `l2cap_register`, `l2cap_open`, `l2cap_send_data` (`l2cap_i.h`); peer BDADDR via `btif_besaud_get_peer_device` / `btif_me_get_remote_device_bdaddr`.
- **Designed for:** Normal connection-oriented L2CAP services (SDP/RFCOMM/AVDTP/…).
- **CROS fit:** True private media channel, independent of BESAUD cmd framing. Needs register-on-both + open-from-one after TWS ACL up.
- **Risk:** **Medium–high** — new PSM negotiation on the TWS link can upset IBRT sniff/role-switch (same class of failure as extra-CID-on-connect, but more protocol surface). Higher than deferred (1).
- **Smallest safe probe:** Register unused dynamic PSM (e.g. `0x1001`) on both sides at boot; do **not** open until manual CROS activate; open once; exchange ping; tear down on mode-off. Never auto-open on BESAUD-up.

#### 4. Voice-over-BLE + BLE datapath (continuous voice sample)

- **Symbols / files:** `services/ble_app/app_vob/voice_over_ble.c` (`__VOICE_OVER_BLE_ENABLED__`); `ble_send_data` → `app_datapath_server_send_data_via_notification`; `services/ble_app/app_datapath/*`; 8 kHz / CVSD-style path, ~5 ms BLE interval constants.
- **Designed for:** SRC↔DST continuous encoded voice over a **dedicated BLE** link (not the classic TWS ACL).
- **CROS fit:** Literally a mic→peer speaker streaming design. Would be a second radio link between buds (or via phone), parallel to IBRT.
- **Risk:** **High** vs (1) — BLE scheduling vs IBRT ACL, power, and dual-connection complexity; feature flag off in open `target.mk`. Latency may still miss &lt;40 ms glass-to-glass.
- **Smallest safe probe:** Enable VOB on a bench build with TWS unpaired first (prove voice stream alone); only then attempt coexist with BESAUD. Do not combine with extra-L2CAP in the same flash.

#### 5. HCI ECC / softbit IBRT ACL test path

- **Symbols / files:** `btif_me_set_ecc_ibrt_data_test` (`me_api.h`); `btm_device_set_ecc_ibrt_data_test`; opcode `0xFCBB`; `app_hci_vender_ecc_data_test_handle`; `register_hci_acl_ecc_softbit_handler_callback` / `btdrv_softbit_enable` (`bt_drv_interface.h`).
- **Designed for:** Controller debug — patterned ACL between IBRT peers / softbit processing (throughput / integrity), not app frames.
- **CROS fit:** Proves the ACL can sustain continuous traffic beyond cmd cadence; might inspire a vendor ACL payload path if softbit RX can be hijacked (unlikely without BES docs).
- **Risk:** **High** — opaque HCI, easy to brick link quality; not a supported app API. Higher than (1)–(3).
- **Smallest safe probe:** From UI test cmd only: enable short `ecc_count` with known pattern while logging `HCI_DBG_ECC_DATA_TEST_EVT`; measure event rate; **do not** put mic audio on it.

#### 6. BTGATT-over-BR/EDR (parallel proprietary L2CAP)

- **Symbols / files:** `services/bt_profiles_enhanced/inc/btgatt.h`, `btgatt_api.h`, `services/bt_app/app_btgatt.cpp`; gated by `GATT_OVER_BR_EDR` (0 in `config/open_source/target.mk`).
- **Designed for:** GATT/ATT over classic ACL (phone OTA / services), structurally a sibling of BESAUD (`btgattcpsm`, send_cmd_packet).
- **CROS fit:** Could open a second proprietary L2CAP to the **peer** BDADDR if stack allows; MTU = `L2CAP_CFG_MTU`.
- **Risk:** **Medium–high** — flag off; mostly phone-facing; enabling may change SDP/GATT and IBRT profile sync. Comparable to (3), less documented for bud↔bud.
- **Smallest safe probe:** Build with `GATT_OVER_BR_EDR=1`; after TWS up, `btif_btgatt_client_create(peer)`; register RX callback; single ping. Abort if mobile GATT/OTA regresses.

#### 7. SPP / RFCOMM to peer (TOTA / GREEN–RED / test client)

- **Symbols / files:** `RFCOMM_CHANNEL_*` in `services/bt_app/app_spp.h` (TOTA, GREEN, RED, AI_VOICE, BES_OTA…); `services/tota/*` (`app_tota_send_data_stream`, MTU-sized packets); interconnection hooks in `app_bt.cpp` (`BTIF_APP_SPP_CLIENT_CCMP_ID` / `RED_ID`) — **interconnection sources largely absent** under `services/interconnection/` (umm_malloc only); `app_spp_test_client_open` (`#if 0`).
- **Designed for:** Phone↔bud serial (OTA, vendor apps, AI voice), not TWS media.
- **CROS fit:** Bulk streaming APIs exist (TOTA stream ~666 B), but opening RFCOMM on the TWS ACL fights IBRT profile sync (`app_tws_profile_sync.h` SPP blobs).
- **Risk:** **High** — profile exchange / role switch; GREEN/RED are phone “interconnection” brands. Worse operational risk than (1).
- **Smallest safe probe:** Prefer not for CROS. If curious: enable dead `app_spp_test_client_open` toward `btif_besaud_get_peer_device()` only with mobile disconnected; ping; expect IBRT pain.

#### 8. (Reference only) Dual-mic `DMA_AUDIO` / OTA `IMAGE_BUFF` on cmd path

- **Symbols / files:** `APP_IBRT_CUSTOM_CMD_DMA_AUDIO`; `IBRT_OTA_TWS_IMAGE_BUFF` in `app_ibrt_ota_cmd.cpp`; buffer max `APP_TWS_CTRL_BUFFER_MAX_LEN` 672 when OTA/recording/CROS (`app_tws_ctrl_thread.h`).
- **Designed for:** Slave→master mic dump and OTA image relay **over the same BESAUD cmd channel**.
- **CROS fit:** Confirms BES expects large without-rsp payloads on cmd path; does **not** beat the ~50 ms usable cadence already measured.
- **Risk:** N/A as “new pipe”; same ceiling as current CROS.
- **Probe:** Skip for transport escape; skim recording `tx_done` pattern if enabling `__DUAL_MIC_RECORDING__` ever becomes possible.

### Explicit non-options (reconfirmed)

| Path | Why not |
|------|---------|
| `app_tws_ibrt_audio_sync_*` / A2DP store | Phone ACL sniff + sync; not mic relay |
| SCO/eSCO | Phone HFP path |
| TOTA / `through_put` / BLE datapath alone | Phone-centric; through_put rides AI/BLE cmds |
| ISO/CIS / LE Audio | `BLE_AUDIO` stubs only; no bud↔bud CIS |
| `btsnoop` / `btif_me_write_dbg_sniffer` | Capture/debug, not a payload pipe |
| `communication_svr` / Knowles UART | Wired debug audio, not RF |
| Manufacturer AD / EIR | Advertising scraps, not continuous audio |

### Suggested order next session

0. **Phone log sink first** — enable stock TOTA SPP + Android reader so CROS probes are not
   blind. See [bt-log-sink.md](bt-log-sink.md). Do **not** use SPP (7) as the audio pipe.
1. Deferred extra-L2CAP probe (candidate 1) — smallest delta from known-good cmd path.
2. If deferred create still kills TWS → dynamic PSM (3) or alternate CID (2), still deferred/manual.
3. If classic ACL stays hostile → VOB bench (4) as a parallel-radio experiment.
4. Treat ECC test (5) / BTGATT (6) / SPP-as-audio (7) as research-only unless 1–3 are exhausted.
