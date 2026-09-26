# CROS Log (Android)

Minimal classic-Bluetooth **SPP** client that reads Bestechnic **TOTA**
`OP_TOTA_STRING` (`0x1000`) frames from PineBuds Pro and shows them live.

See [docs/bt-log-sink.md](../../docs/bt-log-sink.md) for the firmware side.

## Requirements

- Android Studio Ladybug+ / AGP 8.x
- Phone with classic Bluetooth (BLE-only tablets will not work)
- Buds firmware built with **`TOTA=1`** (otherwise SPP channel 12 is not advertised)

## Build / run

I can’t produce an APK in this cloud environment (no Android SDK). Build on your machine:

1. Install [Android Studio](https://developer.android.com/studio) (Ladybug+ / AGP 8.x is fine).
2. **File → Open** → this folder: `android/cros-log`.
3. Let Gradle sync; if prompted, install the suggested SDK / Build-Tools.
4. Plug in a phone (emulator has no classic Bluetooth), enable USB debugging.
5. Run ▶ on the device.
6. Pair PineBuds in system Bluetooth first, then in the app: pick the device → **Capture logs** on.
7. After `peer READY` you should see `[cros_log] quiet=1` — periodic stats stop on
   purpose so SPP does not kill extra-path audio. Transition lines (OPEN/READY/DISABLE)
   still show. Use **Share log** to export a `.txt` via the system share sheet.
8. **Phone SCO on** (v0.2.0): reference `AudioManager.startBluetoothSco()` to the
   paired buds. Compare bud log `BTEVENT_SCO_*` / `[cros_sco]` lines vs peer
   `open_link` (Capture can stay on — same ACL as phone HFP SCO).

Debug APK output (after a local build):

```text
android/cros-log/app/build/outputs/apk/debug/app-debug.apk
```

Or from a terminal with SDK installed:

```bash
cd android/cros-log
./gradlew :app:assembleDebug   # needs gradle wrapper jar from Studio’s first sync
```

Buds need firmware **v0.3.16+** for quiet-during-extra (v0.3.6+ minimum for SPP at all).

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
