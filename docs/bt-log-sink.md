# Phone-side log sink (TOTA / SPP) — investigation

**Verdict:** Reuse Bestechnic **TOTA over classic Bluetooth SPP/RFCOMM**. Do **not** stand up a new BLE GATT log service from zero. Do **not** solder UART until CROS latency is acceptable.

This is the logging path for CROS bring-up — phone captures `[cros_*]` lines while buds stay closed. It is **not** a CROS audio transport (see [cros-transport.md](cros-transport.md) §7).

## Why UART soldering is the wrong next step

| Approach | Destructive? | Needs flash? | Both buds? |
|----------|--------------|--------------|------------|
| UART pads / pogo / case tear-down | Yes | No | Painful |
| New BLE GATT Nordic-UART clone | No | Yes + unproven phone stack | Phone only sees one GATT often |
| **Stock TOTA SPP (this doc)** | No | Yes (`TOTA=1`) | Primary bud easy; secondary via relay later |
| Extra L2CAP / cmd CROS flashes | No | Yes | Already burning cycles without logs |

Goal: one purposeful flash that turns on TOTA + a thin log tee, then iterate CROS with eyes open.

## What is already in the tree

Claude’s tip checks out. Mapping against `vendor/OpenPineBuds`:

### RFCOMM channel map (`services/bt_app/app_spp.h`)

| Enum | RFCOMM # | Role |
|------|----------|------|
| `RFCOMM_CHANNEL_GS_CONTROL` | 10 | GSound |
| `RFCOMM_CHANNEL_GS_AUDIO` | 11 | GSound |
| **`RFCOMM_CHANNEL_TOTA`** | **12** | **TOTA debug / OTA serial** |
| `RFCOMM_CHANNEL_BES_OTA` | 13 | BES OTA |
| `RFCOMM_CHANNEL_AI_VOICE` | 14 | AI voice |
| `RFCOMM_CHANNEL_GREEN` / `RED` | 15 / 16 | Brand “interconnection” |
| `RFCOMM_CHANNEL_FP` | 17 | Fast Pair RFCOMM |
| `RFCOMM_CHANNEL_TOTA_GENERAL` | 18 | Second TOTA SPP (“SppGen”) |

(`RFCOMM_CHANNEL_1` starts at **10** in `spp_api.h`, so TOTA = channel **12** — matches community RE of BES earbuds using ch12 for vendor SPP.)

### Phone-facing plumbing (already written)

| Piece | File | Notes |
|-------|------|-------|
| SPP **server** + SDP `SC_SERIAL_PORT` (0x1101) | `services/tota/app_spp_tota.cpp` | Bud listens; phone is client |
| Init gated by build | `besmain.cpp` → `app_tota_init()` under `TEST_OVER_THE_AIR_ENANBLED` | Off today (`TOTA ?= 0`) |
| String TX API | `tota_printf()` in `app_tota.cpp` | `OP_TOTA_STRING` = `0x1000` |
| Wire frame | `APP_TOTA_CMD_PAYLOAD_T` | `uint16 cmdCode` + `uint16 paramLen` + bytes (LE) |
| Encryption | `TOTA_ENCODE=1` | **`OP_TOTA_STRING` is never encrypted** (`tota_stream_data_transfer.cpp`) |
| Conn handshake | `OP_TOTA_CONN_*` | Needed only for encrypted cmds / flash / dumps — **not** for log RX |
| Second SPP | `app_spp_tota_general_service.cpp` | Custom 128-bit UUID; optional later |
| BLE TOTA profile | `services/ble_profiles/tota/*` | Only if `TOTA=1` **and** `BLE=1`; open_source has `BLE ?= 0` |
| Sniff interaction | `spp_tota_in_progress()` blocks TWS sniff while SPP up | Expected while logging; disconnect when done |

Default open_source build: **`TOTA ?= 0`** → SDP record never registered, `app_tota_init` never called. Sources still compile into `libtota.a`.

## TRACE vs TOTA (important)

Stock `TRACE()` → `hal_trace_output` → **UART only**. There is no stock “mirror TRACE to SPP” hook.

So “capture logs over Bluetooth” means one of:

1. **Selective tee (recommended):** `cros_bt_logf(...)` → ring buffer → timer schedules
   BT-thread flush (`app_bt_start_custom_function_in_bt_thread`) → ≤2× `tota_printf` per
   tick when `app_is_in_tota_mode()` (SPP up). Skip if phone disconnected; drop when ring
   full. Do **not** call `tota_printf` from a general OS timer (it `osSemaphoreWait`s forever).
   Do **not** call SPP from UART ISR / hardfault.
2. **Full TRACE hijack:** wrap `hal_trace_output` — high volume, risk of deadlocks/recursion (`TOTA_LOG_*` itself uses `TRACE`). Skip for v0.
3. **Audio dump / stream path:** `OP_TOTA_STREAM_DATA` / `app_tota_audio_dump_*` — for PCM dumps, not text logs.

## Android connect recipe (classic BT, not BLE)

User wording “over Bluetooth” maps to **classic SPP**, not BLE GATT. That is what TOTA already implements.

1. Pair / connect PineBuds as usual (A2DP).
2. Open a second socket: Serial Port UUID  
   `00001101-0000-1000-8000-00805F9B34FB`  
   Prefer SDP resolution; fallback RFCOMM channel **12** if the stack is sticky.
3. Read framing:
   - `cmdCode` LE u16 == `0x1000`
   - `paramLen` LE u16
   - `paramLen` UTF-8 / ASCII bytes (our `[cros_*]` lines)
4. No AES handshake required to **receive** strings.
5. Optional: send `OP_TOTA_ECHO_TEST_CMD` (`0x6101`) after implementing encode/handshake — not needed for log sink v0.

Scaffold: [`android/cros-log/`](../android/cros-log/) (Kotlin, Android Studio).

### Caveat: which bud do you see?

Phone ACL + SPP attach to the **IBRT master / mobile-connected** bud. The secondary’s TRACE stays local unless we:

- relay secondary logs over BESAUD/IBRT cmd to the master, then out TOTA, or  
- reconnect after role switch / force which bud is primary.

For Stage B, that is still enough: master carries most `[cros_tws]` / `[cros_extra]` TX or RX depending on role. Plan a tiny `CROS_LOG_RELAY` cmd later if we need both sides in one capture.

## TOTA SPP vs new BLE GATT

| | TOTA SPP | New BLE GATT NUS-alike |
|--|----------|-------------------------|
| Firmware surface | Flip `TOTA=1`; call existing `tota_printf` | New service UUID, ATT table, notify MTU, PHY, coexist with A2DP |
| Phone API | Mature `BluetoothSocket` SPP | BLE scan + GATT + CCCD; Android Classic+LE juggling |
| MTU / rate | ~600–666 B RFCOMM frames | ~20–180 B notifies unless Data Length Extension |
| In tree today | Yes, phone-oriented | BLE TOTA exists but needs `BLE=1` (off); still vendor protocol |
| Risk to TWS | Sniff blocked while connected (known) | Unknown dual-mode scheduling |

**Reuse TOTA.** BLE only if classic SPP proves unusable on the test phone.

## Minimal path (ordered — no CROS flash until step 3 works)

### 1. Android app (no bud flash)

Build/install [`android/cros-log`](../android/cros-log/). Connect to a paired PineBuds (even stock) and confirm SPP open fails cleanly while TOTA is off — proves permissions / UUID path.

### 2. Firmware flash — logging only (no L2CAP change)

One flash package:

- Build with **`TOTA=1`** (sets `TEST_OVER_THE_AIR_ENANBLED`).
- Keep current CROS behavior (v0.3.4 cmd path + deferred extra still optional/off for this flash if desired).
- Add thin helper [`firmware/stage_b/cros_bt_log.*`](../firmware/stage_b/cros_bt_log.h):
  ring + BT-thread capped `tota_printf` flush (not from the OS timer directly).
- Tee existing `[cros_tws]` / `[cros_extra]` sites through `cros_bt_logf` (still `TRACE` to UART if pads ever used).

Acceptable risk: SDP gains an SPP record; sniff blocked while the log app is connected. Disconnect the app when measuring glass-to-glass latency.

### 3. Use logs for the next CROS probe

Only after strings flow: flash deferred-extra / alternate CID / etc. with the app connected so OPEN/CLOSED/tx/fail lines are visible without opening the buds.

### Explicit non-goals for this slice

- Full UART-over-BT mirror of every `TRACE`
- TOTA flash / ANC / EQ command surface on Android
- Using SPP as CROS audio pipe (wrong ACL, wrong latency class)

## Wire format cheat sheet

```
Offset  Size  Field
0       2     cmdCode   = 0x1000 (OP_TOTA_STRING), little-endian
2       2     paramLen  = strlen(text), little-endian
4       N     text      = ASCII/UTF-8, no guaranteed NUL in payload
```

Example on the wire for `"hi"`: `00 10 02 00 68 69`.

## Build flag reminder

```makefile
# config/open_source/target.mk
TOTA ?= 0   # → set 1 for log-sink builds
```

When `TOTA=1`:

```text
-DTEST_OVER_THE_AIR_ENANBLED
-DSHOW_RSSI
export TEST_OVER_THE_AIR=1
# BLE_TOTA_ENABLED only if BLE=1 as well (leave BLE=0 for now)
```

Pass through our wrapper when ready:

```bash
TOTA=1 ./scripts/build.sh
```

`open_source` needs patch `0005` (applied by bootstrap): stock `TOTA=1` alone
left `TEST_OVER_THE_AIR=0` (no `libtota` / includes) and the ANC module pulled
missing `anc_spp_tool` symbols. Verified link of a TOTA+CROS image in this tree.