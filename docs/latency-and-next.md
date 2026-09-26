# Latency, stability, and next ideas (review welcome)

## Breakthrough — SCO media ≈ **140 ms** (v0.3.39 ear)

Bud↔bud **SCO/eSCO** + stock HFP **CVSD** voice player: clap ≈ **140 ms**, link
steady. Extra L2CAP path remains ≈ **330 ms** (v0.3.27). **SCO is the latency path**;
quality (MSBC / tuning / asymmetric CROS) is next. See
[CHANGELOG 0.3.39](../CHANGELOG.md#0339--2026-09-26) and
[release v0.3.39](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.39).

| Path | Clap | Status |
|------|------|--------|
| **SCO CVSD (0.3.39)** | ≈ **140 ms** | Latency path — quality TBD |
| Extra L2CAP (0.3.27) | ≈ **322–330 ms** | Daily / quality baseline |

**Extra-pipe baseline:** **v0.3.27** — floor 4 × 50 ms ADPCM on BESAUD extra L2CAP;
quiet underrun + sniff lock; ear-validated (no cutouts, Capture on). Keep for daily
wear until SCO quality is ready.  
**Measurement build (extra):** **v0.3.24** — hop timestamps (B) + L2CAP mode log (H).  
**Usability:** DIY / not a hearing aid.

**Strategy:** ship quality on the SCO path; do not thin the extra jitter floor again
without a new delivery story.

This note is for **fresh eyes**: what we proved, what failed, where the delay lives, and ranked ideas that are *not* “thin the jitter floor again.”

Related: [cros-transport.md](cros-transport.md) · [architecture-cros.md](architecture-cros.md) · [CHANGELOG.md](../CHANGELOG.md) · [references.md](references.md)

---

## What works (do not regress)

| Piece | Detail |
|-------|--------|
| **SCO media (0.3.39)** | Peer `sco_open_link` + `hfp_ibrt_sco_audio_connected(CVSD)` → **~140 ms** clap |
| Transport (extra) | BESAUD **extra L2CAP** CID `0x0b0e` after PING/PONG (or PING) READY |
| Fallback | IBRT custom-cmd ADPCM until READY / if extra down |
| Peer addr on phone master | `ibrt_ctrl_t::p_tws_remote_dev` (besaud peer is NULL there) |
| Logging coexistence | TOTA SPP **quiet** while media runs — chatty SPP kills the link |
| RX jitter floor (extra) | **4 frames × 50 ms = 200 ms** minimum while on extra |

## Lever scorecard (ear + clap)

Measure clap as **waveform start → output start**. Earlier “243 ms” was likely misaligned.

| Build | Change | Clap | Cutouts | Verdict |
|-------|--------|------|---------|---------|
| 0.3.16–17 | Quiet SPP; floor ~4 | ~376 ms? | OK | Extra usable with Capture on |
| 0.3.18 | Floor 4→3 | ~243 ms? | Mild / storms | Floor thin — unstable under stress |
| 0.3.19 | 50→40 ms frames, floor 3 | **≈323 ms** | ~3 / 10 s | Shorter frames ≠ better; worse chop |
| 0.3.20 | Back to 50 ms, floor 3 | **≈336 ms** | **~2 / s** | Floor 3 unusable |
| **0.3.21 / 23** | **Floor 4 × 50 ms** | **≈330 ms** | Rare | **Extra baseline** |
| 0.3.22 | 10 ms TX poll, same frames | ≈330 ms | Back / unusable | Poll ≠ delay; side effects |
| **0.3.24** | **B+H probe only** | **≈322 ms** | **None while logging** | Measurement; media = 0.3.23 |
| **0.3.39** | **Peer SCO + CVSD voice** | **≈140 ms** | Steady | **Latency breakthrough** |

## B+H results (v0.3.24 ear log)

**H — L2CAP mode:** `ERTM_support=0`, `cfg_rfc=BASIC(0)`, CID `0x0b0e`,
scid=dcid=`0x0b0e`, psm=`0x0033`, state=`OPEN`(9), mtu=`679`, cfg flags `0`
→ **basic fixed-CID channel** (no ERTM / no CFG). Closed.

**B — RIGHT / TX dump** (two sessions, same bud `…:2B:B7`, `role=POOR/TX`):

| Hop | Session A (long) | Session B (shorter) | Read |
|-----|------------------|---------------------|------|
| `cap→send` | n=3217 **avg 25 ms** (0–50) | n=695 **avg 25 ms** (0–63) | Half of 50 ms tick — frame waits for send tick |
| `extra q→bt` | avg **0 ms** (max 1) | avg **0 ms** (max 1) | BT-thread dispatch is free |
| `extra bt→TX_HANDLED` | avg **0 ms** (max 69) | avg **2 ms** (max 159) | Stack accepts fast; rare ACL stalls |
| `cmd q→done` | 33× @ 1 ms then extra | same | Brief cmd until READY |
| underrun / tx_fail | 0 / 0 | 0 / 0 | Clean TX |

**LEFT / RX dump** (`role=GOOD/RX`, ~25 s / 497 frames, READY via pong):

| Hop | Result | Read |
|-----|--------|------|
| `recv→put` | n=497 **avg 0 ms** | Decode+put free |
| `rx_buf@put` | **avg 102 ms** (0–190) | **Not** sitting at 200 ms floor — oscillates |
| `play_buf@get` | avg 122 ms (0–230) | Same story from playback |
| underrun | **20** (inaudible to tester) | Brief holes masked by repeat-last-PCM |

### Revised delay model (B complete)

```
~100–190 ms  RX buffer depth in practice (avg ~102; floor target 200, often drained)
~ 50 ms      ADPCM frame period
~ 25 ms      cap→send tick wait          ← measured RIGHT
~  0–2 ms    BT queue / TX_HANDLED avg   ← measured RIGHT
~ remainder  air + DMA + clap alignment
───────
~322 ms      clap start→start (0.3.24)
```

**Conclusion:** non-jitter TX path is clean. RX depth **averages ~half the floor** with
min=0 and 20 underruns / ~25 s — **bursty delivery**, not a stuck encode/BT queue.
That is exactly what **G (sniff / link-policy lock)** targets.

---

## Brainstorm — next approaches (ranked for discussion)

### A. Make underruns less audible (latency stays; quality of glitches improves)

**Idea:** Keep floor 4 (or carefully try 3) but replace “repeat last PCM” with better PLC: comfort noise, ADPCM predictor freeze + fade, time-scale stretch of the last frame, or cross-fade.

**Why:** Cutouts are the usability killer when we thin buffers. If underruns sound like soft dips instead of holes, we can revisit floor 3.

**Risk:** Medium. Easy to make “robotic” worse; needs A/B ear tests.

### B. Instrument the “other ~80 ms” before more guesses — **in v0.3.24**

**Idea:** Timestamp (or sequence + side log) at: capture frame complete → L2CAP submit → peer `datarecv` → pcmbuff put → playback consume. Dump on DISABLE (or rare SPP event lines).

**Shipped:** `[cros_lat]` accumulators (avg/min/max) for cap→send, extra q→bt,
bt→TX_HANDLED, cmd q→done, recv→put, rx_buf@put, play_buf@get. Dump on TX/RX STOP.
Bud clocks are not synced — air time is not measured. Flash **v0.3.24**, run ~30 s,
disable, paste dump lines.

**Why:** If “other” is actually 20 ms, we’re done optimizing non-jitter path. If it’s 120 ms stuck in BT queue, that’s a different fix.

**Risk:** Low. Should be first engineering step before big design changes.

### C. Raise host ACL buffer count — **blocked (closed stack)**

**Idea:** Bump `HCI_NUM_ACL_BUFFERS` (and related) from 6 → 8 or 12 in a one-variable flash; keep baseline media settings.

**Blocked:** BT profiles are closed `.a` libraries; changing the header does not
resize `hci_rx_acl_buff` / host pools. Tried and reverted. True bump needs a
rebuildable stack or a different vendor API.

**Why still relevant:** Pool exhaustion / FC stalls under load remain a theory for
bursty `rx_buf@put` — but the 0.3.25 LEFT storm was **PC speakers** (acoustic
test), **not** phone A2DP to the buds. Do not treat A2DP as the proven cause.

### C′. Suspend A2DP while CROS on — **optional; not default**

**Mis-shipped in v0.3.26** after reading “video” as BT A2DP. **v0.3.27** defaults
`CROS_SUSPEND_A2DP=0`. Build with `=1` only if you actually stream music to the
buds and want pause-on-CROS.

### D. Finer frames with **same ms of jitter** (chop fix, not delay cut)

**Idea:** e.g. 20 ms frames × floor **10** (= 200 ms) — same delay budget, more recovery points per gap.

**Why:** 40 ms × floor 3 failed because it cut **ms of buffer**. Same ms with finer quanta might reduce hole length.

**Risk:** Higher packet rate on ACL (same class of pain as 0.3.19 if not careful). Try only with ms-equivalent floor.

### E. Adaptive jitter with asymmetric rates

**Idea:** Start at floor 4; on underrun jump toward max quickly; shrink only after long healthy periods (already partly true) — or **never shrink below 4** while on extra (already policy) but allow temporary **target** above 4 without measuring clap during storms.

**Why:** Clap during elevated jitter reads high; adaptive might keep average delay lower in quiet rooms if we ever allow start &lt; 4 — only after A/C.

**Risk:** Repeating 0.3.14 shrink cliff if mis-tuned.

### F. Redundancy / simple FEC

**Idea:** Periodically duplicate last frame or send a XOR of last N frames so a single dropped L2CAP PDU doesn’t starve PCM.

**Why:** Targets packet loss, not just late burst arrival.

**Risk:** More ACL load (danger zone given buffer=6). Small duty cycle only.

### G. Link policy during CROS — **v0.3.25; not the A2DP lever**

**Idea:** Ensure sniff is off / delayed while CROS enabled (SPP already blocks sniff; without Capture, does TWS enter sniff and burst?). Role-switch lock. Any BES “prefer throughput” knobs on the TWS ACL.

**Shipped:** sniff lock while CROS on. Logs show `tws=ACTIVE` / `mobile=ACTIVE`.

**0.3.25 ear (LEFT master, PC-speaker walk):** underrun storm (`underrun=1757` /
`rx=1928`, `jitter=8`, `rx_buf@put` avg 88 ms) while link stayed ACTIVE.
**No delay increase.** Sniff lock did not stop the cutouts.

**v0.3.27:** underrun-threshold lines are `CROS_LOG_STAT` (UART-only in quiet) so
SPP does not tee 50…1750 mid-storm.

**0.3.27 ear (2026-09-26):** LEFT master, Capture on, ~5 min CROS — **no cutouts**,
`underrun=41` / `n=6336`, no phone threshold spam. Storm not reproduced; quiet
harden holds. `rx_buf@put` still bursty (avg 165 ms, 0–270) — floor doing the work.

**Why:** Sniff → bursty delivery → need deep jitter. G removed that variable.

**Risk:** Battery. Need logs of sniff entry around cutouts.

### H. Understand L2CAP mode of extra channel — **answered in v0.3.24**

**Idea:** Confirm whether `0x0b0e` is basic or ERTM/streaming; retransmission behavior vs drop. Disasm / HCI sniff if needed.

**Finding:** This tree has `SUPPORT_L2CAP_ENHANCED_RETRANS=0` and
`L2CAP_CFG_RFC_MODE=L2CAP_MODE_BASE`. `l2cap_create_besaud_extra_channel` stamps
scid=dcid=`0x0b0e`, psm=BESAUD, **state=OPEN** and notifies immediately — **no
config exchange / no ERTM**. Runtime log on OPEN/STOP confirms (`[cros_extra] L2CAP mode`).

**Why:** Retransmit-induced reordering/delay could force buffering — not applicable
here; burstiness is elsewhere (ACL FC / sniff / scheduling).

**Risk:** Research time; may not be changeable. → Closed for mode; still useful to
keep the log line for regressions.

### I. Long shots (parked unless A–H stall)

- Private dynamic PSM media channel (surveyed; higher risk than deferred extra)  
- 8 kHz capture (less ACL, worse quality)  
- Phone relay (usually worse latency)  
- Vendor isochronous / ECC test paths (not app media APIs)  
- Other L2CAP CIDs / custom IBRT cmds — **exhausted** (only extra CID is `0x0b0e`; cmd enum has Fast Pair / VA stubs only)

### J. `RFCOMM_CHANNEL_BES_OTA` (ch 4) — **checked; not a latency lever**

**Claude lead:** second named RFCOMM channel next to TOTA, same “defined but unclaimed” shape as extra L2CAP.

**SDK check (open_source build):**
- Enum + `BTIF_APP_SPP_SERVER_BES_OTA_ID` in `app_spp.h`; IBRT UI stub case only.
- `ota_spp.h` present; **no** `ota_spp.c` in tree; `OTA_ENABLE ?= 0`.
- Nothing registers `RFCOMM_CHANNEL_BES_OTA` as a live SPP service in this config.

**Verdict:** Cheapest discovery claim is true — channel is unclaimed here. But RFCOMM still rides **classic ACL best-effort** (same contention class as TOTA / extra). Hijacking it for CROS media would not buy reserved delivery and would fight IBRT profile sync. **Do not flash for latency.** Keep as “free serial ID if we ever need a second phone SPP.”

### K. SCO/eSCO bud↔bud — **next latency experiment**

**Earlier dismissal** (“phone HFP / sniffer path”) was partly **assumed-hard**, not a measured blocker.

**What exists:**
- Controller/LMP: `lmp_esco_link_req`, `ESCO_ENABLE`, `NUM_SCO_CONNS=2`.
- Host API (closed `.a`, headers open): `sco_open_link(bdaddr, …)` / `sco_register_link` — **arbitrary BDADDR**, not HFP-only.
- HFP path: `hf_createSCO`; IBRT sniffer hooks for **phone** SCO (`btapp_sniffer_sco_start`, etc.).

**Why it matters:** eSCO is timeslot-reserved voice — the scheduling property ACL lacks. That matches the measured bottleneck (`rx_buf@put` burstiness). **Agreed next latency bet** after freezing extra at v0.3.27.

**Unknowns / risks:** No open bud↔bud caller. IBRT may refuse SCO on the TWS ACL. PCM/AF path is wired for HFP sniffer, not peer mic. CVSD/mSBC quality + phone-call conflict. Medium–high brick risk.

**Smallest probe:** **v0.3.29** — With CROS enable, master `sco_open_link(tws_peer)`
after 1.5 s; slave registers only. Log OPEN/CLOSED; **no** SCO audio. Extra CROS
unchanged. Capture requires tablet classic BT (TOTA) — cannot also be
“phone disconnected” for SPP logs; UART only for that case.

**0.3.29 ear (LEFT master, Capture on):**  
`sco_init` / `register_link` / `open_link` all **rc=0** → API accepts peer SCO.  
**No `[cros_sco] OPENED` / `CLOSED` notify** → link never completes to our
callback. TWS + extra survived. `close_link rc=1` on DISABLE (nothing open).

**§K status: open investigation, not dead.** Same pattern as early extra (create
returned but path needed READY/coexist work). Hypotheses for no-OPENED:
peer register race, IBRT refusing SCO on TWS ACL after HCI accept, missing HCI
SCO complete → `sco_conn_opened_ind`, wrong notify registration, need eSCO /
codec params, mobile ACL coexistence.

**v0.3.30 next probes (still OPEN/CLOSED only, no SCO audio):**
1. **PONG/READY trigger** — open as soon as extra peer READY (1.5 s fallback kept).
2. **BTEVENT tee** — `SCO_CONNECT_IND/CNF/DISCONNECT` → `CROS_LOG` with err + rem.
3. **Phone SCO reference** — cros-log **Phone SCO on** (`startBluetoothSco`);
   compare stack BTEVENT vs peer `open_link`.
4. Optional `CROS_SCO_SLAVE_OPEN=1` if register-only slave looks like the race.

**0.3.30 ear (LEFT master, Capture on):**  
Fallback @1.5 s raced extra @2 s — READY open never ran; `open_link rc=0`, no
`OPENED`. Phone SCO → Android `CONNECTED` ×2; **zero** bud `BTEVENT_SCO_*`.
First phone SCO also closed extra. Root cause for missing BTEVENT: tee was in
`app_bt_sniff_manager_process`, compiled out under IBRT.

**v0.3.31:** READY-only open (early register; 12 s late fallback); BTEVENT tee in
`app_bt_global_handle`; HFP `AUDIO_CONNECTED/DISCONNECTED` (+ IBRT mock) tee.

**0.3.31 ear:** LEFT-master OK — READY open, phone `BTEVENT_SCO_CONNECT_IND`
(tee live), peer still no `OPENED`. RIGHT-master+POOR/TX **crashed** right after
`registered early` (SPP died).

**v0.3.32:** no SCO on enable; settle after READY (1.5 s on poor) then register+open.
Packaging: versioned zips only (no LATEST alias).

**0.3.32 ear:** LEFT OK. RIGHT still crashed on enable **before sniff LOCK** —
not SCO. Locus = `apply_enabled` / sniff when POOR/TX is IBRT master.

**v0.3.33:** refuse CROS when poor side is IBRT master (bud stays up); SCO probe
default off; sniff skips `exit_sniff_with_tws` on poor.

**0.3.33 ear (confirmed):** RIGHT master → `REFUSE enable`, no reboot. LEFT master
→ CROS OK. Constraint is **POOR/TX × IBRT master** (not physical right). Must
carry into v1.0 configurable poor side — [architecture-cros.md](architecture-cros.md#hard-constraint-v0333--must-keep-for-v10).

**v0.3.34:** SCO probe on again (settle-after-READY); poor-master refuse kept.

**0.3.34 ear (LEFT master, Capture on):** settle → `open_link rc=0`; **no OPENED**,
**no peer BTEVENT**. Phone SCO still shows `BTEVENT_SCO_*`.

**v0.3.35:** last planned lever on this pipe — `CROS_SCO_SLAVE_OPEN=1` (both buds
call `open_link` after READY settle).

**0.3.35 ear (LEFT master):** **SUCCESS — `[cros_sco] OPENED`** + peer
`BTEVENT_SCO_CONNECT_IND rem=<tws peer>`. Extra CROS then choppy → hang until
case (peer SCO + extra ACL + mobile; Phone SCO also started). Link is real;
coexistence with extra is not.

**v0.3.36:** auto-close SCO ~300 ms after OPENED (proof without wedging).

**0.3.36 ear (LEFT master):** **PASS** — `OPENED` → `proof hold done` →
`close_link`/`unregister`/`BTEVENT_SCO_DISCONNECT`; **extra held** through SCO
exit; clean DISABLE. §K link proof done.

**v0.3.37:** `CROS_SCO_ALONE=1` — skip extra; leave SCO OPENED (cmd audio).

**0.3.37 ear:** SCO **held** (no wedge). Cmd under SCO = very choppy; Capture
overrun from underrun STAT spam.

**v0.3.38:** alone = **no ACL TX/RX** (silence + SCO hold).

**0.3.38 ear (~2 min):** **PASS** — silence, OPENED held, clean close.

**v0.3.39 ear:** **PASS** — CVSD voice on peer SCO; **clap ≈140 ms**.

**v0.3.40–41:** CROS shape + hfp_vol bump; stable ~140 ms; tablet vol N/A.

**v0.3.42:** **mSBC 16 kHz** (was CVSD 8 kHz roughness). Quality step on SCO path.

Do not run ACL CROS with peer SCO. Daily extra still v0.3.27 if needed.

**Daily audio until SCO media:** **v0.3.27 extra** baseline.

### L. Parallel BLE between buds — **no idle link; VOB sample exists**

**Claude lead:** dual-mode chip; maybe an idle bud↔bud BLE/GATT already up.

**SDK check:**
- TWS BLE NV is **address/IRK exchange** for phone-facing identity (`nv_record_tws_*ble*`), not a live peer BLE connection.
- `snoop_via_ble_enable = false` in this UI config.
- Open sample: `services/ble_app/app_vob/voice_over_ble.c` (`__VOICE_OVER_BLE_ENABLED__`) — SRC↔DST encoded voice over BLE datapath; flag **off** in open builds.

**Verdict:** No free idle BLE pipe today. Standing one up is a second radio (see cros-transport VOB). Different failure mode than ACL burstiness; still high coexist risk with IBRT. Keep as long-shot **after** A / before or beside K.

### L′. Phone↔bud BLE GATT (Erik pattern) — **logging coexist, not media**

**Source:** [erik-smit/EriksPineBuds](https://github.com/erik-smit/EriksPineBuds) runs `BLE ?= 1` with custom GATT (touch/EQ) + Android companion. Same closed profiles `.a` as us.

**Why it matters for us:** Capture today opens **classic TOTA SPP** on the same ACL as extra CROS. Quiet mode helps; a BLE notify log sink would move chatty debug off that ACL entirely (different radio). Does **not** shrink the 200 ms floor by itself.

**Smallest probe (later):** Enable `BLE=1`, add a thin GATT notify characteristic for `[cros_*]` lines (steal Erik’s profile layout / Android BLE client patterns); keep TOTA as fallback. Measure underruns with Capture-on vs SPP Capture-on. **Not** a bud↔bud media transport.

**Also from openqore/[besota](https://github.com/nnonickreal/besota):** BES OTA RFCOMM UUID `66666666-…` is real — strengthens §J (channel exists when OTA on); still not a latency pipe. Full survey: [references.md](references.md#community-bes--pinebuds-forks-surveyed-2026-09-24).

---

## Suggested next (agreed order)

**Pipe policy:** **v0.3.27 = extra L2CAP baseline** (features freeze here if SCO fails).  
Latency chase leaves extra; do not thin floor 4 again without new evidence.

1. **B+H** — **done**.  
2. **G** — **done** (ACTIVE; not the PC-speaker cutout fix).  
3. **C** — ACL header bump **blocked** (closed `.a`; Erik/openqore same libs).  
3′. **C′** — A2DP suspend — optional only; not the 0.3.25 cause.  
3″. **Quiet underrun** — **v0.3.27** — **extra baseline declared**.  
4. **K** — SCO ~140 ms (0.3.39) + **CROS shape** (0.3.40 mute). Next: prove
   direction ear; quality last (MSBC / FF mic / tuning).  
5. **A** — PLC on extra if daily wear shows audible holes (quality, not delay).  
6. **L′** — BLE GATT log sink if Capture+SPP still hurts / for UART-free logging.  
7. **L** — VOB / peer BLE media (bench first).  
— **J** — BES_OTA RFCOMM: research closed; not a media path.  
— Daily CROS audio: **v0.3.27 extra** until SCO proves out.

## Suggested review questions

1. Is the ~200 ms floor diagnosis right, or is burstiness fixable enough to run floor 2–3 cleanly?  
2. After B dumps: is the non-jitter “other” mostly tick wait (~0–50 ms), BT queue, or RX buffer?  
3. Any BES/OpenPineBuds prior art for non-control BESAUD extra *with* real-time media?  
4. Is bumping `HCI_NUM_ACL_BUFFERS` known-safe on BES2300YP / PineBuds? *(Answer for this tree: header-only; pool is in closed `.a`.)*  
5. Has anyone opened `sco_open_link` to the **TWS peer** (not the phone) on BES2300 / IBRT?

## How to help

- Comment on this doc / open a GitHub Discussion or Issue.  
- Reproduce baseline: flash [v0.3.27](../flash-packages/pinebuds-cros-v0.3.27.zip) or current probe zip, clap start→start, note cutouts.  
- If you have HCI logs around an underrun storm (`underrun threshold` lines), that’s gold.
