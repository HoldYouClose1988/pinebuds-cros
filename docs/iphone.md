# iPhone / iOS — not supported for daily BiCROS

**Product path = Android + firmware v0.3.65.** This page records why iPhone failed
in the field so we do not re-litigate it every time.

## Short answer

BiCROS rides **peer eSCO + HFP voice player**. iOS expects that pipe for
*calls* and treats it as exclusive. Keeping BiCROS up while paired to an iPhone
leads to weird audio routing and ACL drops. Android does not enforce the same
model, so the same firmware holds for long wear tests.

## Measurements (2026-09-27)

| Test | Phone | Load | Outcome |
|------|-------|------|---------|
| Desk quiet ~20 min | Android (CROS Control log on) | TV in office | Held; user disabled |
| Desk loud ~20 min | Android (log on) | White noise at mics | Held; user disabled |
| Store / desk wear | **iPhone** | Normal environment | Drop ~**1 min**, then ~**10–11 min**; case reset to recover |
| iPhone + BiCROS on | **iPhone** | System sounds | Heard on **left pod and phone speaker**; bad / link-noisy on pod |

**Ruled out:** mic load / PCM starvation (Android loud test passed).

**Strong correlate:** phone OS = iPhone vs Android.

## What Apple’s model implies

From Apple Bluetooth Accessory Design Guidelines (audio routing):

- Two-way / call-like audio → **HFP eSCO**
- Music / media → **A2DP**
- On HFP eSCO, accessory speaker + mic should be **dedicated** to that link
- No defined route → play on the iPhone itself

Our peer SCO looks like a reserved voice link. iOS can half-route UI sounds onto
the HFP-shaped path (hissy left-ear audio + phone speaker) and later drop the
classic ACL when scheduling / supervision loses.

Industry pattern: many controllers only afford **one** slave-role SCO. Peer SCO
already consumes that; phone HFP eSCO then collides or starves ACL.

## Parked experiment (not product)

Branch [`cursor/ios-coexist-3d85`](https://github.com/HoldYouClose1988/pinebuds-cros/tree/cursor/ios-coexist-3d85)
· release [v0.4.0](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.4.0):

- Pause BiCROS on real `HF_AUDIO_CONNECTED` / non-peer `SCO_CONNECT`
- Does **not** fix silent iPhone drops with no HF event
- **Not** maintained as a daily driver — use Android + **v0.3.65**

See [BRANCHES.md](BRANCHES.md).
