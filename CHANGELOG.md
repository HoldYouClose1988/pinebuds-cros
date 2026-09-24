# Changelog

All notable flash-package / firmware iterations for PineBuds Pro CROS + SITE.
Format: version, date (UTC), then user-facing changes.

**This project is experimental DIY CROS firmware — not a hearing aid or PPE.**
Ear-validated extra-path CROS from v0.3.16+; still not a clinical product.

## [0.3.22] — 2026-09-24

### Firmware — faster TX poll on 0.3.21 baseline
- **Baseline:** v0.3.21 = floor **4** × **50 ms** frames (stability). Not thinning floor.
- **This build:** send/jitter tick **50 ms → 10 ms**; frame contents still **50 ms**
  ADPCM (same packet rate). Drains `latest_ready` sooner after capture fills.
- **Stuck watchdog is wall-clock:** `CROS_TX_STUCK_MS=200` → tick count scales with
  `CROS_TICK_MS` (was raw `4` ticks = 200 ms @50 ms; would have become 40 ms @10 ms
  and spuriously force-clear busy sends). Same for jitter healthy shrink (~7.5 s).

### Test
Confirm `init v0.3.22 tick10ms floor4`. Compare cutouts to 0.3.21; clap start→start
(expect modest improvement if dispatch wait mattered, not a 0.3.19-style regression).

## [0.3.21] — 2026-09-24

### Firmware — stabilize (restore extra jitter floor 4)
- **v0.3.20** (floor3 × 50 ms): start→start clap ≈ **336 ms**, dropouts **~2 / s**
- Floor 3 is too thin for this RF/path — underruns grow jitter and chop dominates
- **This build:** `CROS_EXTRA_JITTER_MIN_FRAMES` **3 → 4** (200 ms); 50 ms frames
  unchanged. Same media settings as the last known “mostly smooth” era (0.3.16–17)
- Next latency work should **not** thin the floor; prefer TX scheduling / other levers

### Test
Confirm `init v0.3.21 floor4 stable`. Walk 30 s — cutouts should drop sharply.
Clap start→start for the stable-path delay number.

## [0.3.20] — 2026-09-24

### Firmware — remeasure baseline (revert 40 ms frames)
- **v0.3.19:** start-to-start clap ≈ **323 ms**; ~3 cutouts / 10 s (0.2–0.3 s each).
  Prior 243 ms figure likely wrong alignment; use **clap-start → output-start** only.
- 40 ms frames did not help delay and chop felt worse — **revert to 50 ms** ADPCM /
  tick; keep extra jitter floor **3** (same as 0.3.18 media path)
- Goal: start-to-start clap on this build as the true floor3×50 ms baseline before
  the next lever (e.g. faster TX tick, not thinner floor)

### Test
Confirm `init v0.3.20 50ms+floor3 start-to-start`. Clap **start→start**; count
cutouts over ~30 s walk.

## [0.3.19] — 2026-09-24

### Firmware — latency lever (frame period)
- **v0.3.18 clap ≈243 ms** (was 376 ms @ floor4) — floor 4→3 worked
- Walk test mostly smooth; occasional &lt;0.2 s cutouts. One underrun storm
  (`threshold 50…650`, `jitter=8`) then SPP closed — possibly fan/RF; not
  reproduced. **Do not thin floor further** while maxed jitter still cliffs.
- **This build:** ADPCM / tick **50 ms → 40 ms** (640 samples @ 16 kHz); keep
  extra jitter floor **3 frames** (=120 ms). Expect ~30–40 ms less delay if
  frame period + floor-ms both shrink; watch chop / underrun thresholds

### Test
Confirm `init v0.3.19 latency 40ms-frame`. Clap ms vs 243; note cutouts.

## [0.3.18] — 2026-09-24

### Firmware — latency lever (extra jitter floor)
- **Baseline (v0.3.17):** clap test ≈ **376 ms** glass-to-glass (RIGHT mic → LEFT
  speaker) with extra jitter floor **4** frames (200 ms)
- **This build:** `CROS_EXTRA_JITTER_MIN_FRAMES` **4 → 3** (150 ms floor); max
  still 8. One variable only — expect ~50 ms less delay if the floor dominates;
  watch for more chop / underrun-threshold lines

### Test
Confirm `init v0.3.18 latency floor3`. Clap again; note ms and any chop vs 0.3.17.

## [0.3.17] — 2026-09-23

### Firmware — quiet-mode leak on remote stop
- **`cros_tws_on_peer_mode(0)`** now calls `cros_bt_log_set_quiet(0)` (local
  `cros_tws_stop` already did; remote-initiated stop did not)
- No behavior change to the extra media path — 0.3.16 coexistence intact

### Status (ear validation)
4‑minute real-content runs on RX and TX with Capture on; handshake→quiet→extra
stable. One unresolved one-off chop on a TX-logged run (no RX telemetry that
session) — watch for underrun-threshold on a future RX-logged recurrence.

## [0.3.16] — 2026-09-23

### Firmware — quiet SPP while extra media runs
Ear result: **0.3.15 + Capture off = extra works** (choppy/delayed, stable).
Capture on → dies at READY (`SPP closed by peer`). Logging traffic contends with
extra media — not the handshake or jitter floor.

- **`CROS_LOG`** = state transitions (always tee to TOTA)
- **`CROS_LOG_STAT`** = periodic counters (UART only while quiet)
- On peer READY → `quiet=1`; CLOSED / DISABLE / BESAUD-down → `quiet=0`
- Still emit rare underrun-threshold events over SPP while quiet

### SDK note (pool theory)
`HCI_NUM_ACL_BUFFERS` is **6** in this tree (`overide.h` / `bt_sys_cfg.h`). Extra
ADPCM + TOTA SPP both ride classic ACL — a tiny host ACL buffer count makes
Claude’s pp_buff/ACL-pool contention theory plausible. Not proven; quiet-SPP is
the first load test.

### Test
Capture **on**. Expect: handshake lines → `[cros_log] quiet=1` → silence in the
app log while audio continues. If link still dies, even minimal SPP is fatal
(try case-insert dump next). If it holds, load/throughput contention confirmed.

## [0.3.15] — 2026-09-23

### Firmware — extra path works; fix underrun cliff
0.3.14 **did switch** to extra (`audio_rx=` locked to `rx=`). Early underruns were
fine (~15), then after ~20 s underruns exploded (`24→152→1500+`) while `resync=1`
(packets in order, just late). Jitter had been **auto-shrinking to 2–3**, too thin
for bursty L2CAP under phone SPP.

- While on extra media: jitter **floor 4 / ceiling 8** frames (200–400 ms)
- **Do not shrink** below the extra floor
- Larger PCM ring; slightly quieter `audio_rx` logs

### Expect
`extra=1` in rx lines, `jitter` stays ≥4, underrun climb much slower / flat.
May add latency vs cmd — trade for smoothness on the real pipe.

## [0.3.14] — 2026-09-23

### Firmware — ride extra after PING (not only PONG)
0.3.13 breakthrough: master found peer, `OPEN`, `PING tx`, **`peer READY (pong=1)`**.
Extra is bidirectional. But READY was only set on **PONG rx** — the RX/master got
READY while the TX/poor bud typically only sees **PING**, kept `peer_ready=0`, and
stayed on cmd (`rx=` climbed with no `audio_rx=` from extra).

- Set **peer READY on PING rx** (as well as PONG / audio)
- Louder TX/RX pipe logs: `on EXTRA` vs `on CMD`, `audio_tx` / `audio_rx`

### Expect on LEFT (master/RX)
`PING` / `READY` then ideally **`audio_rx=`** climbing (media on extra).

### Expect if you catch RIGHT (TX) logs
`peer READY (ping=…)` then **`tx=… (on EXTRA)`**.

## [0.3.13] — 2026-09-23

### Firmware — peer address on phone master
0.3.12 LEFT/master log: `create skipped — no peer` ×10 then cmd-only.
`btif_besaud_get_peer_device()` is **NULL on IBRT master**; cmd audio + jitter 2–4 were fine
(`underrun` 6→16, jitter recovered 4→3).

- Resolve peer BDADDR: besaud peer → **`p_tws_remote_dev`** → `tws_conhandle`
- Same coexist: 2 s defer, single PING, no-peer retry, cmd until PONG

### Expect
`peer via p_tws_remote_dev` (or handle) then `CREATE` / `OPEN` / `PING` on the master.
Ideal: `PING rx` / `peer READY`. Watch for cutout after OPEN.

## [0.3.12] — 2026-09-23

### Firmware — coexist probe (extra back on, carefully)
A/B 0.3.11 showed **extra-off fixed cutout**; small jitter still underruns but holds.
This build restores jitter **2–4** and re-enables extra with coexistence knobs:

- Create deferred **2 s** after activate (cmd audio settles first)
- **Single PING** on OPEN — no 2 s ping retry storm (that was on in 0.3.9)
- **Retry create** on `no peer` up to 10× / 1 s (0.3.9 LEFT gave up once)
- Audio still **cmd-only until peer PONG** / audio RX

### What to look for
| Log / ear | Meaning |
|-----------|---------|
| Holds like 0.3.10/11 after `OPEN` / `PING tx` | Coexistence OK; OPEN+single ping tolerable |
| Cutout ~15 s again after OPEN | Even quiet extra setup contends — need harder isolation |
| `PING rx` / `peer READY` | Bidirectional extra works — next: ride audio on it |
| `no peer` then later `CREATE` | Retry fix worked on master/RX |

## [0.3.11] — 2026-09-23

### Firmware — A/B isolate (extra vs jitter)
- **`CROS_EXTRA_L2CAP=0` unchanged** (same as 0.3.10)
- **Jitter reverted to 0.3.9:** `MIN=1` / `MAX=2` frames (50–100 ms), ring back to 6 frames
- RX stats every 64 frames again so underrun climb is visible like 0.3.9

### How to read the result (Capture on, LEFT/RX master OK)
Compare to 0.3.9 (`extra=1`, jitter 1–2, underruns climbed) and 0.3.10 (`extra=0`, jitter 2–4, held):

| This build (extra=0, jitter 1–2) | Inference |
|----------------------------------|-----------|
| **Holds** — no underrun climb | Removing **extra** fixed 0.3.9; bigger jitter was not required |
| **Climbs** like 0.3.9 | Bigger **jitter** fixed it; extra may have been innocent |

Do **not** re-enable extra in the same flash as the next step — one variable at a time.

## [0.3.10] — 2026-09-23

### Firmware
- **Cmd-path stability after 0.3.9 breakthrough:** LEFT/RX heard ~15 s then cut out;
  log showed `rx` climbing with **underruns 24→62** and `create skipped — no peer`
  on the master. Likely ACL contention from TX-side extra OPEN+PING while cmd audio ran.
- **`CROS_EXTRA_L2CAP=0` by default** — stay on proven cmd ADPCM; re-enable later for PONG probe
- **Larger RX jitter:** prefill 100 ms (2 frames), ceiling 200 ms (4); bigger PCM ring
- Sparser RX stats logs (every 256 frames) to reduce TOTA SPP load during capture

### Test tip
Flip **Capture logs off** after you see `RX START` / `TX START` for a clean ear test —
SPP on the master shares the radio with TWS.

## [0.3.9] — 2026-09-23

### Firmware
- **Side probe in logs:** print raw `app_tws_is_left_side()` /
  `app_tws_is_right_side()` at init, enable, and peer-mode — not just the
  compiled `poor_cfg=RIGHT` label — so phone logs show which physical bud is
  master (`left=1 right=0` vs `left=0 right=1`)
- Includes 0.3.8 peer-PONG gate (cmd audio until READY)

## [0.3.8] — 2026-09-23

### Firmware
- **Fix silence after ~1 s:** v0.3.7 ear log showed `cmd=7` then all TX on extra
  (`fail=0`) with no peer RX — audio black-holed. Prefer BESAUD extra **only after
  peer PONG** (or audio RX); until then keep proven **cmd-path** ADPCM
- **PING/PONG gate:** OPEN sends `0xC0` ping; peer replies `0xC1`; re-ping every 2 s
  while waiting; log `peer READY` before switching pipes
- Role logs on peer mode (`POOR/TX` vs `GOOD/RX`, LEFT/RIGHT)

### Expect after flash
- Continuous delayed audio on cmd path even if extra never becomes bidirectional
- Ideal: `[cros_extra] peer READY` then `extra=` climbing — lower-latency pipe

## [0.3.7] — 2026-09-22

### Flash package
- **Bundle `bestool.exe`** in every zip (`tools/windows/bestool.exe` → package root)
- `flash.ps1` / `backup.ps1` find it automatically — no separate Rust build for Windows flash
- NOTICE updated for Ralim/bestool redistribution

## [0.3.6] — 2026-09-22

### Firmware
- **cros_bt_log flush safety:** timer only schedules; send runs on BT thread via
  `app_bt_start_custom_function_in_bt_thread`, skips when SPP down, **max 2 lines/tick**
  (avoids `tota_printf`'s forever semaphore wait on a general OS timer)
- **Flash package:** Stage B CROS + **`TOTA=1`** log sink (first logging-enabled zip)

### Android
- **Capture logs** toggle: on opens TOTA SPP; off closes it so TWS sniff is free for ear tests
- **Share log** exports `.txt` for chat review

## [0.3.5] — 2026-09-22

### Docs / tooling (no flash required yet)
- **TOTA/SPP log-sink investigation:** reuse stock RFCOMM channel **12** + `tota_printf` /
  `OP_TOTA_STRING` instead of soldering UART or inventing BLE GATT — [docs/bt-log-sink.md](docs/bt-log-sink.md)
- Android scaffold: [`android/cros-log/`](android/cros-log/) (classic SPP reader)
- Firmware helper: `cros_bt_log` tees `[cros_*]` to TOTA when built with **`TOTA=1`**
- Patch `0005`: make `TOTA=1` link on open_source (force `TEST_OVER_THE_AIR`, stub ANC tool)
- `TOTA=1 ./scripts/build.sh` verified green in CI tree; **hold CROS L2CAP flashes** until phone logs work

### Next flash (when ready)
1. Build `TOTA=1` (same CROS 0.3.4 behavior + SPP log server).
2. Install CROS Log app → Connect → confirm `[cros_tws]` / `[cros_extra]` lines.
3. Only then resume deferred-extra ear tests.

## [0.3.4] — 2026-09-22

### Firmware
- **Deferred BESAUD extra L2CAP probe:** create CID `0x0b0e` only on CROS activate (+500 ms), never on BESAUD-up (that killed TWS in v0.3.0)
- Prefer extra for audio when OPEN; **cmd-path fallback** if not
- On OPEN: exchange a small PING; continuous 50 ms ADPCM otherwise
- Prior-art survey: no open CROS recipe; OPPO RE confirms extra CIDs are for non-control traffic

### Test
1. Flash both → case RESET if needed → wait for TWS.
2. Quad-tap CROS. Link should stay up.
3. If delay/chop improves vs v0.3.3 → extra is carrying audio. If identical → still on cmd fallback.

## [0.3.3] — 2026-09-22

### Firmware
- **Remove activate alert completely** (cue was killing CROS AF after ~1s)
- Back to **v0.2.7-style** immediate start/stop on cmd path
- Packet cadence **50 ms** continuous ADPCM (only change vs the working 40 ms build)
- Extra L2CAP still gated off

## [0.3.2] — 2026-09-22

### Firmware
- **Fix cue killing CROS audio:** triple warning beep was racing the AF mic/speaker streams; play cue first, start streams after **1.2 s** settle
- Same cmd-path 50 ms ADPCM as v0.3.1

### Docs
- Prefer **case RESET ~5s** for lost TWS (purple LED not always present)

## [0.3.1] — 2026-09-22

### Firmware
- **Fix TWS break from v0.3.0:** creating BESAUD extra L2CAP on connect left buds unpaired (right pairing flash, left blue flash, quad-tap dead)
- Extra L2CAP **gated off** (`CROS_EXTRA_L2CAP=0`); audio back on **cmd-path 50 ms ADPCM** (known-good)
- **Triple warning-beep cue** when CROS activates (local + peer)

### Recovering from v0.3.0
- Flash **both** buds with v0.3.1, leave in case ~30–60s until TWS re-pairs (LEDs settle), then quad-tap

## [0.3.0] — 2026-09-22

### Firmware
- **BESAUD extra L2CAP** (CID `0x0b0e`) for CROS audio — dedicated pipe off the cmd queue
- Own notify/recv (stock wrappers TRACE-and-discard RX); create on BESAUD up, both buds
- Audio send via **BT-thread mail** + `l2cap_send_data` (not from osTimer); TX paced by `TX_HANDLED`
- **MODE sync stays on IBRT custom cmd**; audio falls back to cmd path if extra channel not open
- Packet cadence **50 ms** continuous ADPCM (ear-tuned; 405 B < ~679 MTU)

## [0.2.7] — 2026-09-22

### Firmware
- **Fix right-bud hang from v0.2.6:** `app_ibrt_cros_audio_send_now()` from the osTimer was unsafe (wrong BT/ctrl context) → solid blue LED, quad-tap dead on the TX/poor bud
- Restore **`tws_ctrl_send_cmd` + `tx_pending` / `tx_done` gate** for audio packets (same path as mode sync)
- Keep **40 ms continuous ADPCM** + seq resync; static encode scratch (no big stack alloc in ticker)
- Toggle ignored if TWS not linked (avoids half-armed start after a hung peer)

### Recovering from v0.2.6
- Power-cycle / reseat the stuck right bud (case USB, remove ~3s, reseat), then flash **both** buds with v0.2.7

## [0.2.6] — 2026-09-22

### Firmware
- Stage B sweet-spot after rate-ceiling confirm: **40 ms** packets (vs 60 ms robotic/laggy)
- **Continuous ADPCM** across sequential packets (seq byte; resync only on gaps) — less “robotic”
- Tighter RX jitter (40–80 ms); direct BESAUD send
- **Known bad:** direct `send_now` from osTimer hung the right (TX) bud — superseded by v0.2.7

## [0.2.5] — 2026-09-22

### Firmware
- Stage B **rate-ceiling experiment**: 60 ms ADPCM packets (~484 B), 60 ms ticker (fewer cmds/sec)
- Hypothesis: custom IBRT cmd channel caps send *rate*, not bandwidth — larger/rarer packets should smooth chop
- If still choppy → stop tuning cmds; need real TWS audio relay path

## [0.2.4] — 2026-09-22

### Firmware
- Stage B chop rethink: **no BT work in AF DMA callback** — capture only copies PCM; 20 ms ticker encodes/sends
- Single latest-frame slot via `tws_ctrl` (depth 1, freshest audio, tx_done paced)
- Adaptive RX jitter buffer (40–120 ms) grows on underrun, shrinks when stable

## [0.2.3] — 2026-09-22

### Firmware
- Stage B: fix remaining chop — direct send never fired `tx_done`, so most frames were dropped; send every capture frame again
- 20 ms ADPCM frames (lower packet rate); RX underrun PLC (repeat last frame)

## [0.2.2] — 2026-09-22

### Firmware
- Stage B: fix ~1s delay / chop from TWS cmd queue backlog — **direct BESAUD send**, 1-packet inflight gate
- Soft limiter for loud mic spikes (nail-on-grille was dropping CROS)
- RX latency clamp (~60 ms max); shorter 10 ms ADPCM frames; quieter default gain

## [0.2.1] — 2026-09-22

### Firmware
- Stage B CROS: switch mic relay from raw PCM to **IMA-ADPCM** (~4:1) over TWS
- 20 ms frames, 2-packet TX pipeline, ~60 ms RX prebuffer — targets choppiness / early drop

## [0.2.0] — 2026-09-22

### Firmware
- **Stage B CROS (experimental):** poor-side FF mic → good-side speaker over TWS (IBRT custom cmd, raw 16 kHz PCM)
- Default poor side = **RIGHT** (override with `CROS_POOR_IS_RIGHT=0`)
- Quad-tap toggles CROS on/off and syncs mode to the peer (requires TWS link)
- Stage A local loopback remains in tree; Stage B build does not auto-start it

### Package
- New flash zip with Stage B firmware
- Docs: CROS feature README + flash notes for quad-tap CROS test

## [0.1.6] — 2026-09-21

### Package
- Fix backup/flash Sync hang: scripts prompt **out → start bestool → reseat** per bud (BES2300 bootloader must ACK Sync during reset)
- Same experimental firmware bin as v0.1.3–v0.1.5
- Docs (`BESTOOL.md` / windows-flash) match the correct Sync order

## [0.1.5] — 2026-09-21

### Package
- New flash zip with ASCII-safe `backup.ps1` (fixes Windows PowerShell parse error)
- Same experimental firmware bin as v0.1.3/v0.1.4

## [0.1.4] — 2026-09-21

### Package
- Fix `backup.ps1` parse error on Windows PowerShell (non-ASCII em dash in throw string)

## [0.1.3] — 2026-09-19

### Docs
- Public-facing README with a prominent **work in progress / not functional** banner
- Removed personal bring-up checklist and private progress log
- Feature READMEs state CROS and SITE modes are not implemented

### Package
- Regenerated flash zip with updated `FLASH.md` (experimental loopback notes only; not CROS)

## [0.1.2] — 2026-09-19

### Privacy
- Removed hardcoded account URLs from docs, packaging scripts, and zip manifests (relative paths only)
- Dropped older flash zips that embedded account URLs; republished experimental loopback bin
- Rewrote git history so early commits no longer carry a personal author email

### Firmware
- Same experimental loopback binary lineage as v0.1.1 (ASRMIC → FF mic table fix)

## [0.1.1] — 2026-09-19

### Firmware
- **Fix:** `cfg_audio_input_path_cfg` for `AUD_INPUT_PATH_ASRMIC` now uses `CFG_HW_AUD_INPUT_PATH_ASRMIC_DEV` (was wrongly wired to `MAINMIC_DEV` / talk mic)
- Confirmed `VOICE_DETECTOR_EN=0` on `open_source` so the ASRMIC `#else` branch is active

### Package
- Rebuild with `CROS_STAGE_A=1`

## [0.1.0] — 2026-09-19

First numbered flash package.

### Firmware
- Experimental same-bud FF (MIC1) → speaker loopback (16 kHz mono, soft gain, peak clip)
- Auto-starts after boot; **quad-tap** toggles loopback off/on
- Note: 0.1.0 bin still had the ASRMIC→MAINMIC table bug; use **0.1.1+**

### Package
- Includes `open_source.bin`, `flash.ps1`, `backup.ps1`, `FLASH.md`, `BESTOOL.md`, `CHANGELOG.md`, `VERSION`, `MANIFEST.txt`, `SHA256SUMS`
