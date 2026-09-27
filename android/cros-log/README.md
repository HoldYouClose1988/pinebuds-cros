# CROS Control (Android)

Classic-Bluetooth **SPP** client for PineBuds Pro CROS/BiCROS:

- Live **TOTA** log capture (`OP_TOTA_STRING` `0x1000`)
- **BiCROS knobs** (firmware **v0.3.44+**): poor side, mix (sidetone dB), bass, treble

See [docs/bt-log-sink.md](../../docs/bt-log-sink.md) for the firmware side.

## Requirements

- Android Studio Ladybug+ / AGP 8.x
- Phone with classic Bluetooth (BLE-only tablets will not work)
- Buds firmware **v0.3.44+** with **`TOTA=1`**

## Knobs protocol

With Capture/control on, the app sends UTF-8 payloads in `OP_TOTA_STRING` frames:

```text
cros get
cros set poor=right mix=-20 bass=0 treble=0
```

Bud replies with `[cros_cfg] …` lines (also syncs peer over IBRT).

| Knob | Range | Notes |
|------|-------|-------|
| Poor side | LEFT / RIGHT | Keep good ear as IBRT master (master×TX refuse) |
| Mix | −30…0 dB (step 2) | Local good-ear sidetone vs SCO |
| Bass / treble | −6…+6 dB | Soft shelves on SCO playback |

## Build / run

No APK in this cloud environment (no Android SDK). On your machine:

1. Install [Android Studio](https://developer.android.com/studio).
2. **File → Open** → `android/cros-log`.
3. Pair PineBuds → pick device → **Capture / control** on.
4. Adjust knobs → **Apply**. Use **Get** to refresh from buds.

```bash
cd android/cros-log
./gradlew :app:assembleDebug
# apk: app/build/outputs/apk/debug/app-debug.apk
```
