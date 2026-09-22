# CROS Log (Android)

Minimal classic-Bluetooth **SPP** client that reads Bestechnic **TOTA**
`OP_TOTA_STRING` (`0x1000`) frames from PineBuds Pro and shows them live.

See [docs/bt-log-sink.md](../../docs/bt-log-sink.md) for the firmware side.

## Requirements

- Android Studio Ladybug+ / AGP 8.x
- Phone with classic Bluetooth (BLE-only tablets will not work)
- Buds firmware built with **`TOTA=1`** (otherwise SPP channel 12 is not advertised)

## Build / run

1. Open this folder in Android Studio (`android/cros-log`).
2. Sync Gradle, run on a physical device (emulator has no classic BT).
3. Pair PineBuds in system Bluetooth settings first.
4. In the app: pick the device → flip **Capture logs** on.
5. Quad-tap CROS / poke the buds — `[cros_*]` lines should scroll in.
6. Flip **Capture logs** off before a clean latency ear-test (drops SPP so TWS sniff can resume). Flip on again when you need lines.

## Wire format

Little-endian:

| bytes | field |
|------:|-------|
| 0–1 | `cmdCode` = `0x1000` |
| 2–3 | `paramLen` |
| 4… | UTF-8 / ASCII text |

No AES handshake is required to **receive** strings.

## Permissions

Android 12+: `BLUETOOTH_CONNECT` (+ `BLUETOOTH_SCAN` if discovery is used).
Older: `BLUETOOTH` / `BLUETOOTH_ADMIN` + location for discovery on some OEMs.
