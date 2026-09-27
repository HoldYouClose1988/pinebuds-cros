# CROS Control (Android)

Classic-Bluetooth **SPP** client for PineBuds Pro CROS/BiCROS:

- **Controls** tab — BiCROS knobs (poor side, mix, bass, treble)
- **Logs** tab — live TOTA capture with per-line timestamps (`HH:mm:ss.SSS`)

Firmware **v0.3.44+** with **`TOTA=1`**. See [docs/bt-log-sink.md](../../docs/bt-log-sink.md).

## Requirements

- Android Studio Ladybug+ / AGP 8.x
- Phone with classic Bluetooth (BLE-only tablets will not work)

## Knobs protocol

With **Connect** on, the app sends UTF-8 payloads in `OP_TOTA_STRING` frames:

```text
cros get
cros set poor=right mix=-20 bass=0 treble=0
```

| Knob | Range | Notes |
|------|-------|-------|
| Poor side | LEFT / RIGHT | Keep good ear as IBRT master |
| Mix | −30…0 dB (step 2) | Local good-ear sidetone vs SCO |
| Bass / treble | −6…+6 dB | Soft shelves on SCO playback |

## Build / run

```bash
cd android/cros-log
./gradlew :app:assembleDebug
# apk: app/build/outputs/apk/debug/app-debug.apk
```

1. Pair PineBuds in system Bluetooth.
2. Open app → pick device → **Connect**.
3. **Controls** tab: adjust knobs → **Apply**.
4. **Logs** tab: watch timestamped lines; **Share** / **Clear** as needed.
