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

- **Now (v0.3.1):** cmd-path 50 ms ADPCM restored after v0.3.0 extra-create broke TWS.
  Triple-beep on activate. Extra L2CAP code remains behind `CROS_EXTRA_L2CAP=1` (off).
- **Next:** safer deferred extra-channel probe (not on BESAUD-connected hot path).
