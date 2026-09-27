# CROS Control (Android) v0.4.0

Classic-Bluetooth **SPP** client for PineBuds Pro BiCROS (firmware **v0.3.64+**,
`TOTA=1`).

Meant for people who already flash the buds — technical, but not a raw log console.
**Day to day you do not need the app** after knobs are Applied (quad-tap toggles BiCROS).

## What you see

- **Help** — FAQ + status beep/tone chart
- **Connect** — knobs + status (support log stays **off**)
- **Status banner** — connected / BiCROS on·off / knobs / last save (via `cros status`)
- **Knobs** — poor ear, local mix, EQ, CROS path level, hiss filter → **Apply** (NV on both buds)
- **Support log** — hidden toggle; turn on only for bug reports
- **Phone SCO** — only under support log (dev probe)

First launch shows a one-time DIY disclaimer (includes “app not required day to day”).

## Requirements

- Android Studio with AGP 9 / Gradle 9.1+ (JDK 17–25)
- Phone with classic Bluetooth

## Build

```bash
cd android/cros-log
./gradlew :app:assembleDebug
# apk: app/build/outputs/apk/debug/app-debug.apk
```

1. Pair PineBuds in system Bluetooth.
2. Open app → accept DIY disclaimer → pick device → **Connect**.
3. Adjust knobs → **Apply**. Case/reboot keeps them (fw NV).
4. Leave **Show support log** off unless sharing a bug report.
