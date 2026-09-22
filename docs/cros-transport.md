# CROS transport notes (Stage B)

## What we learned on the control channel

Custom `APP_IBRT_CUSTOM_CMD_*` over BESAUD **works** for CROS audio, but it is a
**command/control** path, not an isochronous media pipe:

| Cadence | Result (ear test) |
|---------|-------------------|
| ~20 ms cmds | Audible chop |
| ~60 ms cmds | Smooth, but robotic + ~1 s delay |
| ~40 ms continuous ADPCM via `tws_ctrl` (v0.2.7) | Usable bring-up path |
| ~40 ms + `send_now` from osTimer (v0.2.6) | **Hung TX bud** (solid blue, quad-tap dead) |

So the ceiling is largely **cmds/sec**, not room RF. Tuning the sender alone
cannot turn this into a stock-quality TWS audio link. Audio must leave the
ticker through **`tws_ctrl_send_cmd`** (or a future dedicated L2CAP), never
direct BESAUD `send_now` from osTimer context.

## Important correction about “TWS audio sync”

A2DP stereo between buds is **not** “left bud relays PCM to right.”
Both buds sniff the **phone’s** ACL; BESAUD carries **sync/control**.
Piggybacking CROS onto `app_tws_ibrt_audio_sync_*` / A2DP decoder inject is the
wrong model for mic→peer speaker.

## Best next pipe: BESAUD extra L2CAP

Unused extra channel already exists in the tree:

| Piece | Where |
|-------|--------|
| CID | `L2CAP_BESAUD_EXTRA_CHAN_ID` `0x0b0e` (`l2cap_i.h`) |
| Create/send wrappers | `tws_besaud_create_extra_channel`, `tws_besaud_extra_channel_send_data` (`app_tws_besaud.h`) |
| Lower API | `l2cap_create_besaud_extra_channel`, `l2cap_send_data` (closed impl, open decl) |
| Closed libs | `libtws_ibrt_enhanced_stack*_RTX.a` |

**Catch:** stock RX callbacks in the closed blob appear to **TRACE and discard**
payload. A probe must register **our own** L2CAP notify/datarecv via
`l2cap_create_besaud_extra_channel`, not rely on the stock receive stub.

MTU is ~679 B (same ballpark as the 672 B cmd max) — the win is a **dedicated
channel** and hopefully better scheduling than the cmd queue, not magic bitrate.

## Smallest probe (after v0.2.7 activate restored)

1. On TWS BESAUD up, both buds: `l2cap_create_besaud_extra_channel(peer, 0x0b0e, notify, recv)`.
2. On channel open: store handle; TX with `l2cap_send_data`.
3. Burst counter/ADPCM every 10–20 ms for a few seconds; gate on TX-done if available.
4. Success = peer `recv` sees steady cadence **without** custom cmds → move CROS onto it.

## Non-starters (for now)

- SCO/eSCO between buds (phone call sniffer path)
- Feeding mic PCM into A2DP SBC store APIs
- Calling `app_ibrt_send_cmd_without_rsp` / `send_now` from osTimer
- More cmd-path micro-tuning past a known-good cadence

## Status

- **Now:** cmd-path CROS via `tws_ctrl` (v0.2.7); activate/quad-tap restored after 0.2.6 hang.
- **Next:** BESAUD extra L2CAP probe once ear delay/chop on 0.2.7 is characterized.
