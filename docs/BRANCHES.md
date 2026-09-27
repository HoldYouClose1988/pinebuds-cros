# Branch / track layout

Two firmware tracks in this repo:

| Track | Branch | Flash / tag | Role |
|-------|--------|-------------|------|
| **Android** | [`main`](https://github.com/HoldYouClose1988/pinebuds-cros/tree/main) · [`android`](https://github.com/HoldYouClose1988/pinebuds-cros/tree/android) | **v0.3.65** / `v0.3.65-android` | Known-good BiCROS on Android. Freeze / bugfix only unless agreed. |
| **iOS** | [`cursor/ios-coexist-3d85`](https://github.com/HoldYouClose1988/pinebuds-cros/tree/cursor/ios-coexist-3d85) | **v0.4.0+** | iPhone coexist: pause BiCROS when the phone wants HFP/eSCO. |

## Why

BiCROS rides **peer eSCO + HFP voice player**. Android held ~20 min under quiet/loud load.
iPhone drops ~1–11 min and half-routes system sounds onto the SCO path. Apple expects HFP
eSCO to be exclusive for call-like audio — so iOS needs an explicit coexist policy.

## Rules

- Do **not** merge experimental iOS coexist into `main` / `android` until ear-validated on iPhone.
- Android users: flash **v0.3.65** (or later android-track releases).
- iOS testers: flash **v0.4.x** builds from the iOS branch / matching GitHub Release.
- App (CROS Control) is shared; Help copy can mention tracks once iOS builds ship.
