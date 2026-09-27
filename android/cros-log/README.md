# CROS Control (Android)

Classic-Bluetooth **SPP** client for PineBuds Pro CROS/BiCROS:

- **Controls** tab — BiCROS knobs (poor side, mix, bass, treble)
- **Logs** tab — live TOTA capture with per-line timestamps (`HH:mm:ss.SSS`)

Firmware **v0.3.44+** with **`TOTA=1`**. See [docs/bt-log-sink.md](../../docs/bt-log-sink.md).

## Requirements

- Android Studio with AGP 9 support (Panda / Quail / recent Otter+)
- **Gradle 9.1+** (wrapper ships 9.1.0) — required to run the Gradle daemon on **JDK 25**
- Phone with classic Bluetooth (BLE-only tablets will not work)

### JDK / Gradle note

Gradle **8.9** only runs on Java ≤22. If Android Studio selected **JVM 25** for Gradle,
you need this project's Gradle **9.1** wrapper (already set). After a pull:

1. **File → Sync Project with Gradle Files**
2. If it still complains: **Settings → Build, Execution, Deployment → Build Tools → Gradle → Gradle JDK**
   - Either leave **JDK 25** (works with Gradle 9.1), or pick **JDK 17 / 21** embedded in Studio

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

In Android Studio: open `android/cros-log` → Sync → Run.

Or from a terminal (needs a local Gradle wrapper / Studio-installed SDK):

```bash
cd android/cros-log
# Studio can generate gradlew on first sync; or:
gradle wrapper --gradle-version 9.1.0
./gradlew :app:assembleDebug
# apk: app/build/outputs/apk/debug/app-debug.apk
```

1. Pair PineBuds in system Bluetooth.
2. Open app → pick device → **Connect**.
3. **Controls** tab: adjust knobs → **Apply**.
4. **Logs** tab: watch timestamped lines; **Share** / **Clear** as needed.
