# Branches

| Branch | Role |
|--------|------|
| **`main`** / **`android`** | **Product** — Android BiCROS (**fw v0.3.65**, flash zip **v0.3.66**). Use this. |
| `cursor/tws-link-explore-3d85` | **Explore** — TWS poll / closed-lib diagnostics. Not a release line. |
| `cursor/ios-coexist-3d85` | **Parked** — experimental iPhone coexist (**v0.4.0**). Not a daily driver. |

## Product = Android

Flash **[v0.3.66](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.66)**
(same bin as v0.3.65 + installer + APK). Pair buds to an **Android** phone.
App: CROS Control from the zip / `android/cros-log`.

## Explore (TWS link)

`cursor/tws-link-explore-3d85` has read-only poll/tpoll diagnostics and notes from
peeking named IBRT objects. Fun / research only — do **not** ship from this
branch or merge into `main` until something is deliberately productized.

## Why iOS is parked

Field + desk tests: Android held ~20 min under quiet and loud mic load; **iPhone
dropped in ~1–11 min** with half-routed system sounds on the SCO path. BiCROS
uses peer eSCO / HFP voice — the pipe iOS expects to own for calls. Full write-up:
[iphone.md](iphone.md).

Do **not** merge iOS coexist into `main` unless someone is actively productizing
iPhone support again.
