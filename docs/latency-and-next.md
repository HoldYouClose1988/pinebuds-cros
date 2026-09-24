# Latency, stability, and next ideas (review welcome)

**Baseline firmware:** **v0.3.23** (= v0.3.21) — floor 4 × 50 ms ADPCM on BESAUD extra L2CAP.  
**Measurement build:** **v0.3.24** — same media path + hop timestamps (B) + L2CAP mode log (H).  
**Measured:** clap **start→start ≈ 322 ms** (v0.3.24, Capture on, stable / no dropouts).  
**Usability:** cutouts rare; brief ones only in hectic noise. DIY / not a hearing aid.

This note is for **fresh eyes**: what we proved, what failed, where the delay lives, and ranked ideas that are *not* “thin the jitter floor again.”

Related: [cros-transport.md](cros-transport.md) · [architecture-cros.md](architecture-cros.md) · [CHANGELOG.md](../CHANGELOG.md)

---

## What works (do not regress)

| Piece | Detail |
|-------|--------|
| Transport | BESAUD **extra L2CAP** CID `0x0b0e` after PING/PONG (or PING) READY |
| Fallback | IBRT custom-cmd ADPCM until READY / if extra down |
| Peer addr on phone master | `ibrt_ctrl_t::p_tws_remote_dev` (besaud peer is NULL there) |
| Logging coexistence | TOTA SPP **quiet** while extra media runs — chatty SPP kills the link |
| RX jitter floor | **4 frames × 50 ms = 200 ms** minimum while on extra |

## Lever scorecard (ear + clap)

Measure clap as **waveform start → output start**. Earlier “243 ms” was likely misaligned.

| Build | Change | Clap | Cutouts | Verdict |
|-------|--------|------|---------|---------|
| 0.3.16–17 | Quiet SPP; floor ~4 | ~376 ms? | OK | Extra usable with Capture on |
| 0.3.18 | Floor 4→3 | ~243 ms? | Mild / storms | Floor thin — unstable under stress |
| 0.3.19 | 50→40 ms frames, floor 3 | **≈323 ms** | ~3 / 10 s | Shorter frames ≠ better; worse chop |
| 0.3.20 | Back to 50 ms, floor 3 | **≈336 ms** | **~2 / s** | Floor 3 unusable |
| **0.3.21 / 23** | **Floor 4 × 50 ms** | **≈330 ms** | Rare | **Baseline** |
| 0.3.22 | 10 ms TX poll, same frames | ≈330 ms | Back / unusable | Poll ≠ delay; side effects |
| **0.3.24** | **B+H probe only** | **≈322 ms** | **None while logging** | Measurement; media = 0.3.23 |

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

**Why still relevant:** Pool exhaustion / FC stalls under **A2DP + extra CROS**
remain the leading theory for the 0.3.25 LEFT+video storm.

**Workaround shipped in v0.3.26:** suspend A2DP while CROS is enabled (see below).

### C′. Suspend A2DP while CROS on — **v0.3.26**

**Shipped:** if `a2dp_is_music_ongoing()` at CROS enable → suspend stream + stop
local SBC. Frees mobile ACL airtime for TWS extra. User presses play after DISABLE.

**Risk:** Low–medium (UX: video pauses). Correct for DIY CROS priority.

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

**0.3.25 ear (LEFT master + video):** underrun storm (`underrun=1757` / `rx=1928`,
`jitter=8`, `rx_buf@put` avg 88 ms) while link stayed ACTIVE. **No delay increase.**
**Did not stop video cutouts** — next lever is **C** (ACL pool).

**Why:** Sniff → bursty delivery → need deep jitter.

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

- Private dynamic PSM media channel  
- 8 kHz capture (less ACL, worse quality)  
- SCO between buds (previously dismissed)  
- Phone relay (usually worse latency)  
- Vendor isochronous / ECC test paths (not app media APIs)

---

## Suggested next (agreed order)

1. **B+H** — **done**.  
2. **G** — **done** (ACTIVE; not the video cutout fix).  
3. **C** — ACL header bump **blocked** (closed `.a`).  
3′. **C′** — A2DP suspend while CROS — **v0.3.26**.  
4. **A** — PLC if quiet-room underruns remain.

## Suggested review questions

1. Is the ~200 ms floor diagnosis right, or is burstiness fixable enough to run floor 2–3 cleanly?  
2. After B dumps: is the non-jitter “other” mostly tick wait (~0–50 ms), BT queue, or RX buffer?  
3. Any BES/OpenPineBuds prior art for non-control BESAUD extra *with* real-time media?  
4. Is bumping `HCI_NUM_ACL_BUFFERS` known-safe on BES2300YP / PineBuds?

## How to help

- Comment on this doc / open a GitHub Discussion or Issue.  
- Reproduce baseline: flash [LATEST](../flash-packages/pinebuds-cros-LATEST.zip), clap start→start, note cutouts.  
- If you have HCI logs around an underrun storm (`underrun threshold` lines), that’s gold.
