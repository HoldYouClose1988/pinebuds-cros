# Branches

| Branch | Role |
|--------|------|
| **`main`** / **`android`** | **Product** — Android BiCROS **v0.3.65**. Use this. |
| `cursor/ios-coexist-3d85` | **Parked** — experimental iPhone coexist (**v0.4.0**). Not a daily driver. |

## Product = Android

Flash **[v0.3.65](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.65)**.
Pair buds to an **Android** phone. App: CROS Control from Releases / `android/cros-log`.

## Why iOS is parked

Field + desk tests: Android held ~20 min under quiet and loud mic load; **iPhone
dropped in ~1–11 min** with half-routed system sounds on the SCO path. BiCROS
uses peer eSCO / HFP voice — the pipe iOS expects to own for calls. Full write-up:
[iphone.md](iphone.md).

Do **not** merge iOS coexist into `main` unless someone is actively productizing
iPhone support again.
