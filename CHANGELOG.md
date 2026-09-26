# Changelog

All notable flash-package / firmware iterations for PineBuds Pro CROS + SITE.
Format: version, date (UTC), then user-facing changes.

**This project is experimental DIY CROS firmware — not a hearing aid or PPE.**
Ear-validated extra-path CROS from v0.3.16+; still not a clinical product.

## [0.3.36] — 2026-09-26

### Firmware — SCO OPENED proved; auto-close so buds do not wedge
- **0.3.35 ear (LEFT master):** first **`[cros_sco] OPENED`** + peer
  `BTEVENT_SCO_CONNECT_IND rem=b7:2b:…` with `CROS_SCO_SLAVE_OPEN=1`. Extra CROS
  then went choppy → dropout; buds unresponsive until case. Phone SCO while peer
  SCO was up likely made it worse.
- **0.3.36:** same open path; **close SCO ~300 ms after OPENED** (proof only —
  do not leave peer SCO up under extra L2CAP + mobile ACL).
- Latency path forward: put CROS **on** SCO and drop extra — not both at once.

### Test (LEFT master)
1. Flash both — `init v0.3.36`.
2. Capture LEFT. Quad-tap. **Do not** tap Phone SCO during the probe.
3. Expect: `OPENED (… tearing down)` then `proof hold done — close` / `CLOSED`
   / `close_link`. Extra audio should survive.
4. Paste log.

## [0.3.35] — 2026-09-26

### Firmware — last §K lever: `CROS_SCO_SLAVE_OPEN=1`
- Both buds issue `sco_open_link` after READY settle (not master-only).
- Poor-master refuse unchanged. Extra baseline unchanged. No SCO audio yet.

### Test (LEFT = IBRT master)
1. Flash both — `init v0.3.35` / `slave_open=1` in sco probe init.
2. Capture LEFT. Quad-tap CROS.
3. Expect settle → `open_link` on master; slave log (if captured) also `open_link`.
4. Success = **`[cros_sco] OPENED`**. Else same wall — paste LEFT log.

### Result (2026-09-26 ear — LEFT master) — **OPENED**
`slave_open=1`. After settle: `open_link rc=0` → peer
`BTEVENT_SCO_CONNECT_IND rem=b7:2b:11:11:22:20` → **`[cros_sco] OPENED`**.
Extra CROS then choppy → silence; buds wedged until case (SPP died). User also
started Phone SCO while peer SCO was up. **§K link is real**; must not coexist
with extra media + mobile. Follow-up: **v0.3.36** auto-close after OPENED.

## [0.3.34] — 2026-09-26

### Firmware — SCO probe back on (keep master×TX refuse)
- **CROS_SCO_PROBE=1** again: settle-after-READY register+open (from 0.3.32).
- **Still refuse** POOR/TX when IBRT master (0.3.33 guard) — test SCO only with
  poor side as slave (default: LEFT master, RIGHT TX).
- Extra baseline unchanged. No SCO audio yet — looking for `[cros_sco] OPENED`.

### Test (LEFT must be IBRT master)
1. Flash both — `init v0.3.34`.
2. Confirm LEFT master (`role=` / reseat if RIGHT is master).
3. Capture LEFT. Quad-tap CROS.
4. Expect: extra READY → `[cros_sco] peer READY — settle …` → `open_link rc=` →
   **`[cros_sco] OPENED`** (success) or still nothing after settle.
5. Optional: Phone SCO on — expect `BTEVENT_SCO_*` / `HF_EVENT_AUDIO_*` (known-good).
6. Paste LEFT log. Do **not** force RIGHT master for this test.

### Result (2026-09-26 ear — LEFT master)
`init v0.3.34`, Capture on, `role=0` master, GOOD/RX. Settle path ran;
`sco_init` / `register_link` / `open_link` all **rc=0**. **No** `[cros_sco] OPENED`
or peer `BTEVENT_SCO_*` (peer rem `b7:2b:…`). Phone SCO produced
`BTEVENT_SCO_CONNECT_IND` / disconnect (`rem=9c:65:…`) — tee + phone path OK.
DISABLE: `close_link rc=1` (nothing open). Same §K wall as 0.3.29–0.3.32:
API accepts peer open; link never completes. Next levers (undecided):
`CROS_SCO_SLAVE_OPEN=1`, or UART / phone-disconnected retest (`mobile_conhandle=0`).

## [0.3.33] — 2026-09-26

### Firmware — refuse POOR/TX when it is IBRT master (stop the reboot)
- **0.3.32 RIGHT log:** still died on local enable — after `ENABLE`, **before**
  sniff LOCK. So not SCO register (that was already deferred). Crash is in
  `apply_enabled` / sniff path when **RIGHT = master + mic TX**.
- **Guard:** if poor side is IBRT master → log `REFUSE enable — POOR/TX is IBRT
  master` and roll back (no TX start). Bud stays up.
- Softened sniff on TX: skip `exit_sniff_with_tws` on poor side; breadcrumbs.
- **SCO probe default OFF** (`CROS_SCO_PROBE=0`) — peer never got OPENED; isolate
  crash. Rebuild with `CROS_SCO_PROBE=1` when chasing SCO again.
- Override refuse: `CROS_ALLOW_POOR_MASTER=1` (still risky).

### Test
1. Flash both — `init v0.3.33`.
2. **LEFT master:** CROS should work as before (extra audio; no SCO).
3. **RIGHT master:** quad-tap RIGHT → expect **`REFUSE enable`** (no reboot).
   Or enable from LEFT while RIGHT is master → RIGHT logs REFUSE, stays up.
4. Paste RIGHT log if it still dies (should not).

### Result (2026-09-26 ear)
- **RIGHT master + POOR/TX:** `REFUSE enable` — bud stayed up (confirmed).
- **LEFT master + GOOD/RX:** full enable / RX / extra READY OK. Phone SCO showed
  `BTEVENT_SCO_CONNECT_IND` + `HF_EVENT_AUDIO_CONNECTED` (tee path works; SCO
  probe itself still off).
- **v1.0:** document constraint as role×TX, not “right bud” — see
  [architecture-cros.md](docs/architecture-cros.md#hard-constraint-v0333--must-keep-for-v10).

## [0.3.32] — 2026-09-26

### Packaging
- **No more `pinebuds-cros-LATEST.zip`.** Ship versioned zips only
  (`pinebuds-cros-v0.3.32.zip`). `CURRENT.txt` points at the current version.

### Firmware — RIGHT-master crash mitigation (SCO)
- **0.3.31 RIGHT log:** IBRT master + POOR/TX; after `registered early` the bud
  died (SPP cut). LEFT-master (GOOD/RX) survived; phone `BTEVENT_SCO_CONNECT_IND`
  tee worked; peer `open_link` still no `OPENED`.
- **Change:** no `sco_init`/`register` on CROS enable. On peer READY wait settle
  (500 ms GOOD / 1500 ms POOR) then register+open. Avoids racing mic TX start.

### Test
1. Flash both — `init v0.3.32`.
2. **LEFT master** (usual): Capture LEFT → quad-tap → expect READY → settle →
   `open_link` (no early register). Phone SCO → `BTEVENT_SCO_*`.
3. **RIGHT master** (force role if you know how): Capture RIGHT → enable CROS →
   bud must **stay up**; expect `peer READY — settle 1500ms` then sco lines (or
   paste if it still dies — note last line).
4. Paste both if possible.

## [0.3.31] — 2026-09-26

### Firmware — §K: READY-only open + fix dead BTEVENT tee
- **0.3.30 ear:** fallback @1.5 s raced extra defer @2 s — READY open never ran.
  Phone SCO `CONNECTED` on Android but **zero** bud `BTEVENT_SCO_*`.
- **Root cause of missing BTEVENT:** tee lived in `app_bt_sniff_manager_process`,
  which is `#if !defined(IBRT)` — **dead on PineBuds**. Moved to
  `app_bt_global_handle` (always runs).
- **READY-only:** early `register_link` on enable; `open_link` only on peer READY
  (or 12 s late fallback). No 1.5 s race.
- **HFP tee:** `HF_EVENT_AUDIO_CONNECTED/DISCONNECTED` + IBRT mock → `CROS_LOG`.

### Test
1. Flash both — `init v0.3.31`.
2. Capture LEFT. Quad-tap. Expect `registered early` then after PONG
   `peer READY — open now` / `open_link rc=` (**not** fallback first).
3. **Phone SCO on.** Expect bud `[cros_sco] HF_EVENT_AUDIO_CONNECTED` and/or
   `BTEVENT_SCO_*` (proves tee). Compare to peer path `OPENED` or not.
4. Paste log.

## [0.3.30] — 2026-09-26

### Firmware — §K next probes (still no SCO audio)
- **PONG/READY open:** `cros_sco_probe_on_peer_ready()` fires `sco_open_link`
  as soon as extra peer READY (PING/PONG/audio). 1.5 s timer kept as fallback.
- **Stack tee:** `BTEVENT_SCO_CONNECT_IND/CNF/DISCONNECT` → `CROS_LOG` with
  `err=` + rem BDADDR (compare phone SCO vs peer open).
- **Optional:** `CROS_SCO_SLAVE_OPEN=1` so slave also issues `open_link`
  (default 0). Extra baseline unchanged.

### Android cros-log v0.2.0
- **Phone SCO on/off** button: `AudioManager.startBluetoothSco()` +
  `ACTION_SCO_AUDIO_STATE_UPDATED` lines in the log pane.
- Use with Capture on: phone SCO is the known-good stack reference; peer
  `open_link` is the experiment.

### Test
1. Flash both — `init v0.3.30`. Rebuild/install cros-log.
2. Capture LEFT (master). Quad-tap CROS. Expect
   `[cros_sco] peer READY — open now` then `open_link rc=` (not only the
   fallback timer).
3. Tap **Phone SCO on** (buds still paired). Expect bud
   `[cros_sco] BTEVENT_SCO_CONNECT_*` — that proves the tee + stack path.
4. Paste: did peer path get `OPENED` or only BTEVENT / nothing?

## [0.3.29] — 2026-09-26

### Firmware — SCO probe: arm on peer-mode too
- **0.3.28 miss:** remote enable (`peer mode=1`) never called
  `cros_sco_probe_on_cros_enable` — only local quad-tap did. Ear log showed
  `[cros_extra] OPEN` (L2CAP), **not** `[cros_sco] OPENED`.
- Both local and peer enable/disable now arm/disarm the SCO probe.
- Same OPEN/CLOSED-only probe otherwise.

### Test
Flash both — `init v0.3.29`. Prefer phone disconnected. Capture the **LEFT**
bud (or whichever is IBRT master — it runs `open_link`). Quad-tap CROS; wait
≥2 s. Success line is exactly **`[cros_sco] OPENED`** (not `cros_extra] OPEN`).

### Result (2026-09-26 ear — LEFT master)
`init v0.3.29`, Capture LEFT, `role=0` (MASTER). Probe armed; `sco_init` /
`register_link` / `open_link` all **rc=0**. **Never saw `[cros_sco] OPENED` or
`CLOSED` notify.** DISABLE: `close_link rc=1` (nothing to close). Extra L2CAP
still READY; TWS stayed ACTIVE — probe did not brick the link.
Phone/tablet stayed connected for Capture (`mobile_conhandle=0x0080`) — required
for TOTA SPP; a fully-disconnected retest needs UART.

### Status / next (§K not closed)
**Partial success:** host SCO API accepts bud↔bud open; **OPENED callback never
fires** under Capture conditions. Not written off — same class of “API exists,
path incomplete” as early extra L2CAP. Next: find why OPENED is missing
(peer register timing, IBRT policy, HCI event not reaching our notify, eSCO
params, HFP-shaped setup, …). Keep **v0.3.27 extra** as daily audio until SCO
audio is proven.

## [0.3.28] — 2026-09-26

### Firmware — SCO/eSCO bud↔bud OPEN/CLOSED probe (§K)
- **Extra baseline unchanged** (v0.3.27 media + quiet underrun + sniff lock).
- On CROS enable: after 1.5 s, master `sco_open_link(tws_peer)`; slave
  `sco_register_link` only. Log `[cros_sco] OPENED` / `CLOSED` / `open_link rc=`.
- **No SCO audio yet** — mic/speaker stay on extra L2CAP.
- Prefer **phone disconnected** (logs WARN if `mobile_conhandle` set).
- Disable with `CROS_SCO_PROBE=0`.

### Test
1. Flash both buds — `init v0.3.28 SCO-probe+G`.
2. Forget/disconnect phone from buds (cleanest); TWS still paired.
3. Capture on; quad-tap CROS; wait ≥2 s.
4. Look for `[cros_sco] open_link` then **`OPENED`** (success) or only CLOSED /
   TWS drop (fail). Extra CROS audio should still work either way.
5. Quad-tap off — expect `close_link` / `CLOSED`. Paste log.

## [0.3.27] — 2026-09-24 — **extra-pipe baseline**

### Status
**Freeze / keep** for BESAUD **extra L2CAP** CROS. Ear-validated daily wear with
Capture on. Product features continue on this pipe if SCO latency work does not
pan out. Latency chase next: **SCO/eSCO bud↔bud** ([latency-and-next.md](docs/latency-and-next.md) §K).

### Firmware — correct 0.3.26 misread; quiet underrun SPP tee
- **Clarification:** 0.3.25 “video” cutouts were **PC speakers** (acoustic test),
  **not** Bluetooth A2DP to the buds. A2DP-suspend was the wrong lever.
- **Default:** `CROS_SUSPEND_A2DP=0` (optional `=1` if you really stream to buds).
- **Quiet harden:** `underrun threshold` lines use `CROS_LOG_STAT` (UART only while
  quiet). 0.3.25 LEFT storm teed thresholds 50…1750 over SPP mid-media — that can
  amplify ACL contention. Same media + G sniff lock otherwise.

### Test
Confirm `init v0.3.27 quiet-underrun+G`. PC-speaker walk like 0.3.25 — Capture on
OK. Expect **no** `underrun threshold` on phone during quiet (UART only). Compare
cutouts / DISABLE `underrun=` count vs 0.3.25.

### Result (2026-09-26 ear)
LEFT master (GOOD/RX), Capture on, long wear in CROS. **No cutouts by ear.**
Phone stayed quiet (no `underrun threshold` spam). DISABLE: `underrun=41` over
`rx_buf@put n=6336` (~5 min @ 50 ms) — vs 0.3.25 storm `1757`/`1928`. Burstiness
still present (`rx_buf` avg **165 ms**, 0–270) but floor absorbed it. Sniff stayed
ACTIVE. **Quiet-underrun harden looks good; 0.3.25 storm not reproduced.**
**Declared extra-pipe baseline.**

## [0.3.26] — 2026-09-24

### Firmware — A2DP suspend while CROS on (video coexist) — **superseded**
- Built on a misread of “video” as BT A2DP. **Skip; use 0.3.27.**
- Kept in history: optional `CROS_SUSPEND_A2DP=1` remains for real A2DP coexist.

## [0.3.25] — 2026-09-24

### Firmware — G: sniff lock while CROS enabled
- **Same media as v0.3.24 / 0.3.23** (floor 4 × 50 ms). No floor/frame change.
- **Why:** B+H showed TX path clean (`cap→send`≈25 ms, BT≈0 ms) but LEFT RX
  `rx_buf@put` avg **102 ms** (0–190) with **20 underruns / ~25 s** — bursty
  delivery. Capture already blocks sniff; this locks sniff for **Capture-off** use.
- On CROS enable: `tws_sniff_block(120s)`, `exit_sniff_with_tws`, sniff checker on;
  refresh block every 60 s; `sniff_allowed` also false while `cros_tws_is_enabled()`.
- Log `[cros_tws] sniff LOCK/UNLOCK` + `link@… tws=/mobile=` (ACTIVE vs SNIFF).

### Test / result (0.3.25)
Sniff lock worked (`tws=ACTIVE`). **No delay increase.** Cutouts with LEFT master
while testing against **PC speakers** (not BT video) — underrun storm
(`1757`/`1928`, `jitter=8`); SPP still teed underrun thresholds while quiet.

## [0.3.24] — 2026-09-24

### Firmware — B+H measurement on 0.3.23 baseline (no media change)
- **Same audio path as v0.3.23** (floor 4 × 50 ms, 50 ms tick). No latency lever.
- **B — hop timestamps:** capture→send, extra queue→BT `l2cap_send`, BT→`TX_HANDLED`,
  cmd queue→done, recv→put, RX pcmbuff depth @put, play depth @get. Accumulators
  dump on TX/RX STOP (DISABLE). Look for `[cros_lat]` lines after quad-tap off.
- **H — L2CAP mode of CID `0x0b0e`:** log `SUPPORT_L2CAP_ENHANCED_RETRANS`,
  `L2CAP_CFG_RFC_MODE`, channel scid/dcid/psm/state/mtu + cfg RFC flags on OPEN
  and again on STOP. Disasm shows create stamps fixed CID → `OPEN` with **no CFG**
  → **basic mode** (ERTM compiled out in this tree).

### Test
Confirm `init v0.3.24 probe B+H`. Enable CROS ~30 s with Capture on, disable,
paste `[cros_lat]` + `[cros_extra] L2CAP mode` lines. Clap should still ≈330 ms.

## [0.3.23] — 2026-09-24

### Firmware — mark 0.3.21 as current baseline (revert 0.3.22)
- **v0.3.21:** start→start ≈ **330 ms**, cutouts almost gone (brief only in hectic noise)
- **v0.3.22:** 10 ms TX poll — clap still ≈**330 ms**, **cutouts back / not usable**
- Faster poll did not remove dispatch delay that matters; side effects hurt stability
- **This build:** same as 0.3.21 (50 ms tick + floor4 × 50 ms frames + ms stuck timeout
  macros kept for future). LATEST points here so nobody stays on 0.3.22

### Where latency stands
~200 ms intentional RX jitter floor; ~50 ms frame; ~80 ms other → clap ≈330 ms.
Floor/frame/tick latency levers failed usability. Review + brainstorm:
[docs/latency-and-next.md](docs/latency-and-next.md).

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
