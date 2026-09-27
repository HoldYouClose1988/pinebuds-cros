# Changelog

All notable flash-package / firmware iterations for PineBuds Pro CROS + SITE.
Format: version, date (UTC), then user-facing changes.

**This project is experimental DIY CROS firmware — not a hearing aid or PPE.**
Ear-validated extra-path CROS from v0.3.16+; still not a clinical product.

**Where to get bins:** current + audio baseline in
[`flash-packages/`](flash-packages/); **all** historical zips on
[GitHub Releases](https://github.com/HoldYouClose1988/pinebuds-cros/releases).
Bring-up journals live under [`docs/archive/`](docs/archive/).

**Product = Android + v0.3.65.** iPhone is not supported for daily wear —
[docs/iphone.md](docs/iphone.md). Parked iOS experiment: v0.4.0 branch.

## [0.4.4] — app — Android-only messaging

### CROS Control **0.4.4**
- Help FAQ: iPhone not for day-to-day; use Android + firmware v0.3.65.
- Repo: Android-first README, [docs/iphone.md](docs/iphone.md), iOS branch parked.

## [0.3.65] — 2026-09-27

### App copy + poor-ear warn (CROS Control **0.4.2**) + `phone=` on status
- Help / FAQ, disclaimer, and knob hints rewritten for a general reader (warmer,
  same safety facts). Extra FAQ: crash reseat order, enable cool-down wait, mix
  ceiling / howl history. Phone-call FAQ left out until ear-tested.
- **Poor-ear Apply warning** no longer hardcodes Left as master. It warns when
  poor equals the **phone-connected** side (from `cros status` `phone=LEFT|RIGHT`,
  or Left by default on older fw).
- Firmware: `cros status` ACK adds `phone=` (physical side of the bud answering
  SPP) and reports `fw=0.3.65`.

### Test
1. Install app 0.4.2. On fw ≥0.3.65, Connect → Refresh status → banner/knobs;
   log line includes `phone=LEFT` (typical).
2. Select poor = that same side → Apply → warning names that side (not always Left).
3. Help FAQ matches rewritten copy; pending call FAQ absent.

Flash zip for 0.3.65: [`flash-packages/pinebuds-cros-v0.3.65.zip`](flash-packages/pinebuds-cros-v0.3.65.zip).
App 0.4.2 also works on **0.3.64** (assumes Left phone-side until `phone=` is present).

## [0.3.64] — 2026-09-27

### Firmware — `cros status` one-shot + app polish (v0.4.0)
- **`cros status`** → one ACK with `enabled=` + knobs + `fw=` so the phone can
  refresh BiCROS state **without** leaving the support log on.
- Android **CROS Control 0.4.0:** DIY disclaimer; poor-ear Apply warning; human
  knob labels; status banner; Connect separate from support log (log + Phone
  SCO hidden, off by default — log traffic fights BiCROS).

### Test
1. Flash both — `init v0.3.64`. Install app 0.4.0.
2. Connect (log off) → banner shows fw + knobs; Enable BiCROS → Refresh status
   → `enabled=1`.
3. Apply → toast “Saved on buds”; case → reconnect → NV load in banner/knobs.
4. Support log off during ear use; on only for Share bug reports.

## [0.3.63] — 2026-09-27

### Firmware + app — BiCROS knobs persist on the buds (incl. poor side)
- **Before:** mix / EQ / sco / noise / poor lived in RAM only — case/reboot
  reset to defaults; app had to Apply again.
- **Now:** knobs (including **poor ear**) saved in BES NV
  (`system_info.flag_value[8]`). Both buds write on phone **Apply** and on
  peer sync RX. Boot restores and logs:
  - `[cros_cfg] NV load poor=… mix=… sco=…` when flash had a saved blob
  - `[cros_cfg] init defaults …` on first boot / empty NV
  - `[cros_cfg] NV save (set|peer|…) …` after Apply / peer / AbsVol
- App **v0.3.9:** on those lines, UI syncs sliders and prints
  `[phone] knobs from buds (…): poor=… mix=… sco=…`.

Audio baseline remains v0.3.61 behavior; this is config persistence on top.

### Test
1. Flash both — expect `NV load` or `init defaults`, then `init v0.3.63`.
2. Connect app → Get — knobs match bud; phone line confirms.
3. Change poor + mix, Apply — see `NV save (set)` on both (peer save too).
4. Case both buds, power up, reconnect — expect `NV load` with your poor/mix;
   BiCROS uses that poor side without re-Apply.

## [0.3.62] — 2026-09-27

### Optional — 75 s hold-escape (not baseline)
**Baseline remains [v0.3.61](https://github.com/HoldYouClose1988/pinebuds-cros/releases/tag/v0.3.61).**
v0.3.62 only adds an absolute hold-escape if teardown/cool-down stalls
(infinite NOT_YET). Likely a closed-stack hang under the flags — another
band-aid, not a claim of reliability. Flash only if you want that insurance.

## [0.3.61] — 2026-09-27 — **SCO BiCROS baseline**

### Firmware — DISABLED / NOT_YET cues audible again
**Product baseline.** Ear PASS (093040): all status cues; BiCROS ~140 ms;
phone A2DP before enable / after disable. Residual SCO/SPP flake accepted.
- **Ear PASS (091029 / v0.3.60):** ENABLED SCO-PCM beep good; READY / OPEN_FAIL
  stock media good; SCO recoverable after OPEN_FAIL — **keep those**.
- **Ear miss:** `[cros_cue] DISABLED (sco-pcm)` and `NOT_YET (sco-pcm)` fired but
  **no sound**. DISABLED arms PCM then teardown stops voice ~200 ms later (or
  voice already down after OPEN_FAIL). NOT_YET fires mid soft-close / cool-down
  with **no SCO PCM path**. Gate was also wrong: `sco_live()` used `log_hold`,
  so cool-down READY took silent SCO-PCM fallback instead of stock media.
- **Fix:**
  - Gate PCM vs media on `cros_sco_voice_is_up()` only
  - Voice down → stock media for DISABLED (`DIS_CONNECT`) / NOT_YET (`WARNING`)
  - Voice up → SCO-PCM; teardown **holds `voice_stop`** until DISABLED cue
    finishes (cue-hold ≤600 ms)
  - READY / OPEN_FAIL unchanged when voice down (stock PAIRING_SUC / FAIL)

### Test
1. Flash both — `init v0.3.61`.
2. On → one ENABLED beep; off → **two** DISABLED beeps (or disconnect tone if
   voice already down); logs: `DISABLED (sco-pcm)` or `DISABLED media`.
3. Mid-teardown on → NOT_YET **WARNING** tone; log `NOT_YET media`.
4. READY / OPEN_FAIL still stock pairing success/fail.
5. ≥3 on/off cycles; confirm cues never kill BiCROS / SPP.

## [0.3.60] — 2026-09-27

### Firmware — status cues via SCO-PCM (fix media wedge)
- **Ear FAIL (084801 / v0.3.59):** ENABLED fired (`[cros_cue] ENABLED id=27`
  = `AUD_ID_BT_CONNECTED`); two stock beeps played, then **SPP/log/taps died**.
  Quad-tap disable sent no event; could not re-enable — **case reset required**.
  Same class as v0.3.1: `media_PlayAudio` during peer SCO races AF / kills the
  CROS voice path.
- **Fix:** while peer SCO / voice is up, mix short **square beeps into good-ear
  SCO PCM** only — **never** start `APP_PLAY_BACK_AUDIO` / `media_PlayAudio` for
  ENABLED / DISABLED / NOT_YET. Media prompts allowed **only when SCO is fully
  down** (READY / OPEN_FAIL).

| Cue | Path | Pattern |
|-----|------|---------|
| **ENABLED** | SCO-PCM | 1× medium ~880 Hz |
| **DISABLED** | SCO-PCM | 2× short ~660 Hz |
| **NOT_YET** | SCO-PCM | 3× staccato ~990 Hz |
| **READY** | media `PAIRING_SUC` if SCO down; else 1× high PCM | |
| **OPEN_FAIL** | media `PAIRING_FAIL` if SCO down; else 1× long low PCM | |

Long SCO teardown buffer from v0.3.58 unchanged (~15–40 s).

### Test
1. Flash both — `init v0.3.60 SCO-PCM-status-cues`.
2. Quad-tap on → after BiCROS up, **one** short beep (not stock CONNECTED); log
   `[cros_cue] ENABLED (sco-pcm)`. Logs/taps must keep working.
3. Quad-tap off → **two** short beeps; teardown continues; later READY
   (stock success tone once SCO is down, or PCM fallback).
4. Tap on mid-teardown → three staccato NOT_YET; after READY, enable again.
5. Repeat on/off **≥3 cycles** without case reset. Confirm cues never kill
   BiCROS / SPP.

## [0.3.59] — 2026-09-27

### Firmware — audible BiCROS status cues (stock tones) — **revoked for ear use**
Distinct stock `AUD_ID` prompts (no new PCM). **Reliability / AF-race note:**
v0.3.1 WARNING-on-tap killed CROS audio; ENABLED fires only after confirmed
OPENED + voice/shape + **500 ms** settle — never on the tap itself.

| Cue | When | Stock id |
|-----|------|----------|
| **ENABLED** | OPENED + BiCROS shape (good ear) | `AUD_ID_BT_CONNECTED` |
| **DISABLED** | Quad-tap / peer off received | `AUD_ID_BT_DIS_CONNECT` |
| **READY** | Teardown safe (BTEVENT, or force cool-down done) | `AUD_ID_BT_PAIRING_SUC` |
| **NOT_YET** | Enable while mid-teardown / cool-down | `AUD_ID_BT_WARNING` |
| **OPEN_FAIL** | `open_link` never OPENED after retry | `AUD_ID_BT_PAIRING_FAIL` |

**OPEN_FAIL included** — silence-until-ready is ambiguous with “still waiting.”

Long SCO teardown buffer from v0.3.58 unchanged (~15–40 s).

### Test (cycle several times — not once)
1. Flash both — `init v0.3.59`.
2. Quad-tap on → wait → **CONNECTED**-like cue only after BiCROS is actually up
   (not at the tap). Log: `[cros_cue] ENABLED` after shape.
3. Quad-tap off → **DISCONNECT**-like cue immediately; later **PAIRING_SUC**-like
   READY when teardown finishes (or after force cool-down).
4. Tap on during teardown → **WARNING** NOT_YET; after READY, tap on → ENABLE.
5. Optional stress: if OPENED missing path appears → **PAIRING_FAIL** cue.
6. Repeat on/off **≥3 cycles**; confirm cues never kill BiCROS audio.

## [0.3.58] — 2026-09-27

### Firmware — reliable BiCROS re-enable (BTEVENT + cool-down)
- **Ear PASS (081742):** SCO down/up cleanly **3×** without case reset.
- **Ear (074125 / 074942FAIL):** teardown almost never got
  `sco_notify(CLOSED)`; always **hard-timeout**. Real drop was
  `BTEVENT_SCO_DISCONNECT` ~20 s later (`err=0x22`). Immediate deferred
  ENABLE after force teardown → `open_link rc=0` but **OPENED missing**.
  Waiting ~1 min (or case reset) worked ~30% of the time.
- **Fix (intentional long buffer — reliability over speed):**
  - Soft close ×3, then **await BTEVENT** up to 20 s before unregister
  - `BTEVENT_SCO_DISCONNECT` (peer-filtered) finishes teardown as clean
    `DISABLE (BTEVENT)` and may run deferred ENABLE
  - Forced hard-timeout → **10 s cool-down** before deferred ENABLE
  - Rearm settle 3 s
- **UX note:** disable/re-enable can take **~15–40 s**. That is expected on
  this hacked peer-SCO path (closed-source stack; CLOSED notify unreliable).
  Do **not** shorten the buffer without a proven faster close — case resets
  were the alternative. Call out in release notes / future installer copy.
- AbsVol DAC-vs-loudness remains backlog.

### Test
1. Flash both — `init v0.3.58`.
2. Quad-tap on → BiCROS. Quad-tap off — expect `await BTEVENT` then
   `DISABLE (hard-timeout|BTEVENT)`; BTEVENT often arrives just after force.
3. Wait through teardown, then quad-tap on again **without** the case —
   expect `OPENED` / voice / BiCROS. Repeat on/off several times.
4. If force path: `cool … then deferred ENABLE` before rearm.

## [0.3.57] — 2026-09-27

### Firmware — AbsVol rocker actually moves BiCROS DAC (hfp_vol stick)
- **Ear (log 004700):** AbsVol was intercepted (`absvol sco=8→0→…`) but no
  audible change. Hook updated `g_vol` only; `cros_sco_set_hfp_volume` called
  `volumeset` without writing **`hfp_vol` NV**. Stock paths re-apply from
  `hfp_vol` and snapped the DAC back — rocker looked dead.
- **Fix:** persist `hfp_vol` (+ `current_btdevice_volume`) on every SCO DAC
  set (AbsVol, TOTA Apply, BiCROS enable). Log `absvol apply sco=N dac=N
  hfp_vol=N` for confirmation.
- Defaults unchanged (sco=8, noise=3).

### Test
1. Flash both — `init v0.3.57`.
2. BiCROS on + Capture. Rocker down to near 0 — contralateral should get
   quiet / mute; rocker up — louder. Expect `absvol apply` + matching
   `hfp_vol=`.
3. Stimulate the **poor** bud (RIGHT mic) while listening on good ear —
   sidetone is independent of this knob.

## [0.3.56] — 2026-09-27

### Firmware + app — phone volume rocker drives BiCROS SCO DAC
- **While BiCROS is on**, AVRCP Absolute Volume (phone rocker / system
  volume) maps to **SCO DAC** on the good ear (`sco=` / `hfp_vol`), not
  A2DP music. Same 0..15 table as before.
- Still stores A2DP NV so music after CROS-off matches the rocker.
- Logs: `[cros_cfg] absvol … sco=N a2dp=N` — app slider tracks via the
  existing `sco=` parse.
- Defaults unchanged: sco=8, noise=3.

### Test
1. Flash both — `init v0.3.56`. Enable BiCROS (quad-tap).
2. With Capture on, press phone volume up/down — expect `absvol` lines and
   audible BiCROS level change (not just music).
3. SCO DAC seekbar in the app should follow. noise= still independent.
4. Disable CROS, play music — rocker should control music again.
5. Optional: install android/cros-log **0.3.8** for updated hint copy.

## [0.3.55] — 2026-09-27

### Firmware + app — SCO DAC default 8; Music (A2DP) knob
- **Ear:** correct BiCROS level is **sco=8** (not 11). Hold-still restart OK
  (slow bring-up/tear-down expected on this SCO hack).
- **Defaults:** SCO DAC **8**, noise **3**. App label is now **SCO DAC gain**
  (was “Volume”) — this is `hfp_vol` / call DAC, not music.
- **New:** **Music (A2DP)** slider `a2dp=0..15` — separate NV + DAC when A2DP
  plays; never applied while BiCROS SCO owns the DAC. Phone AVRCP speak-gain
  report on change.
- Logs: `sco=` / `a2dp=` (still accept `vol=` as SCO alias).

### Test
1. Flash both — `init v0.3.55`. Expect `sco=8 a2dp=… noise=3` on get.
2. BiCROS at 8 should match the tuned ear level; raise sco briefly to hear
   the old floor return.
3. With CROS **off**, play music — move Music (A2DP) slider / Apply; bud
   music level should follow. With CROS **on**, A2DP change stores for later
   (does not steal SCO DAC).
4. Install android/cros-log **0.3.6** for the new labels + slider.

## [0.3.54] — 2026-09-27

### Firmware + app — default vol 11 + noise 3 (ear: 13 was the “link noise”)
- **Ear:** hold/ANC did nothing; LEFT triple (vol down) cleaned it.
  **vol=11 + noise=3** holds steady. Not teardown; not ANC.
- **Why vol sounded like “link noise”:** our knob is **HFP/SCO call volume**
  (`hfp_vol` → codec DAC gain on the SCO player), **not** A2DP music volume
  (`a2dp_vol`). Separate NV fields. BiCROS rides the call path, so turning
  it up amplifies the mSBC/SCO noise floor the same way a loud call does —
  it does not invent RF hash; it just makes the existing floor audible.
  Codec table: level **13 = −6 dB**, **11 = −12 dB** (6 dB quieter).
  HW sidetone mix is a **separate** gain (still −20 dB default).
- **Defaults:** `vol=11`, `noise=3` (was 13 / 0). App seekbars match.
- Teardown CLOSED-wait from 0.3.53 kept for re-arm.

### Test
1. Flash both — `init v0.3.54`. Fresh enable should start at vol 11 / noise 3.
2. Quad-tap BiCROS — expect usable contralateral without the old “hash”
   at 13. Nudge vol up to 13 briefly to confirm hash returns, then back.
3. Optional: rebuild/install android/cros-log 0.3.5 for matching slider defaults.

## [0.3.53] — 2026-09-27

### Firmware — wait for real SCO CLOSED so every re-open is clean
- **Ear (log 233307):** re-arm worked, but the **first** teardown hit the
  800 ms force-timeout before `CLOSED` arrived; the **second** link (after a
  true `CLOSED`) was clean. Goal: make every enable that clean.
- **Fix:**
  - Voice drain **300 ms** before `close_link` (helps HFP drop)
  - Soft wait for `CLOSED` **4 s × 3** (re-issue `close_link` each try)
  - Hard unregister only after ~12 s if `CLOSED` never arrives
  - **Defer ENABLE** while closing — do not open on a half-dead SCO; run
    deferred enable after teardown completes
  - Settle timer re-checks if still closing
- Link noise filter stays in the app but should be unused if every link is
  this clean.

### Test
1. Flash both — `init v0.3.53`.
2. Quad-tap on → BiCROS. Quad-tap off → expect `voice drained` /
   `close_link … wait CLOSED` / `CLOSED` / `DISABLE (CLOSED)` (prefer
   `CLOSED`, not `hard-timeout`).
3. Quad-tap on again **without** the case — clean `OPENED` / voice / BiCROS.
4. Optional: tap on again *while* still closing → `ENABLE deferred`; after
   `CLOSED`, deferred enable runs. Repeat on/off several times.

## [0.3.52] — 2026-09-27

### Firmware — re-enable BiCROS after DISABLE without case reset
- **Ear (log 232259):** first session OK; DISABLE; second ENABLE got
  `open_link rc=0` but **never OPENED** — needed case reseat to start again.
- **Cause:** teardown called `unregister` immediately after `close_link`
  without waiting for `SCO_CLOSED`, leaving the controller half-down so the
  next `open_link` could not complete.
- **Fix:**
  - `close_link` → wait for `CLOSED` (800 ms timeout fallback) → then
    unregister / clear / `DISABLE`
  - Reset `sco_inited` so the next enable does a fresh `sco_init`
  - Rearm settle **2.5 s** after a prior session
  - If `OPENED` missing after open_link, one register+open retry at 2 s

### Test
1. Flash both — `init v0.3.52`.
2. Quad-tap on → BiCROS. Quad-tap off → expect `close_link` / `CLOSED` /
   `DISABLE (CLOSED|timeout)`.
3. Quad-tap on again **without** the case — expect `ENABLE (…, rearm)` then
   `OPENED` / voice / BiCROS. Repeat on/off a few times.

## [0.3.51] — 2026-09-27

### Firmware — curated diagnostics on phone (without SCO flush wedge)
- **Ear (log 230956):** controls + dropouts fixed on 0.3.50; phone log was
  bare (only `[cros_cfg] set` + final `DISABLE`) — milestones were UART-only.
- **Design:** keep **no bulk ring flush** under SCO (taps stay alive). Expand
  an **ack queue** (8 deep, 1 line/tick) for curated milestones:
  - `ENABLE` / `OPENED` / `voice UP` / `BiCROS GOOD/RX` (or POOR shape)
  - `already OPENED — skip open_link` / `open_link rc=`
  - `CLOSED` / `DISABLE`
  - existing Apply/Get `[cros_cfg] …`
- Sniff/register chatter stays UART-only during SCO.

### Test
1. Flash both — `init v0.3.51`.
2. Capture on → quad-tap — expect phone lines for ENABLE → OPENED → voice UP →
   BiCROS shape (spaced ~80 ms apart, not a dump).
3. Apply still acks. Quad-tap off → CLOSED / DISABLE. Taps stay responsive.

## [0.3.50] — 2026-09-27

### Firmware — fix tap-dead + bring-up dropouts (0.3.49 regression)
- **Ear (log 225910):** BiCROS up; Apply still worked; then **quad-tap dead**, no
  more logs. Bring-up had **dropouts**.
- **Tap-dead cause:** 0.3.49 kept flushing SPP (1/tick) during SCO — same BT
  wedge class as 221736. Fix: while armed/opening/up, **ack-only** (Apply/Get);
  no ring flush. Tee resumes when SCO is fully down; `DISABLE` logged from
  `close_bt` after flags clear.
- **Dropout cause:** peer already `OPENED` this bud, then we still called
  `open_link` after the 100 ms gap. Fix: **skip `open_link` if already
  `sco_up`**; cancel open timer on `OPENED`.

### Test
1. Flash both — `init v0.3.50`.
2. Quad-tap on — expect cleaner bring-up (log may show `already OPENED — skip
   open_link` on one bud). Fewer/no start dropouts.
3. Apply volume/noise — still get `[cros_cfg] set` ack.
4. Quad-tap off — taps respond; phone shows `[cros_tws] DISABLE` / `CLOSED`.

## [0.3.49] — 2026-09-27

### Firmware — stop muting the phone log after SCO is down
- **Why it was wrong:** hold was `sco_up || open_issued`. After the SCO pipe
  closed, `open_issued` stayed set until `close_bt`, so the TOTA tee kept
  draining/muting — DISABLE/`CLOSED` never reached the phone.
- **Fix:** hold = **`sco_up` only**; clear `open_issued` on `CLOSED`.
- While SCO is up: **throttle** flush to 1 line/tick — do **not** discard the
  ring. When SCO is down: normal multi-line tee resumes immediately.

### Test
1. Flash both — `init v0.3.49`.
2. Quad-tap on/off — expect `[cros_tws] DISABLE` and `[cros_sco] CLOSED` on
   the phone without needing the case.

## [0.3.48] — 2026-09-27

### Firmware + app — volume, link-noise filter, DISABLE ack
- **Ear (log 223823):** mixing OK; Apply acks OK; quad-tap **did** turn BiCROS
  off but phone saw no DISABLE (hold still muted that `CROS_LOG`).
- **DISABLE / OPENED / CLOSED** now use `CROS_LOG_ACK` so the phone sees them
  under SCO hold.
- **Volume** (`vol=0..15`): drives good-ear HFP/SCO playback (same path as bud
  keys). Default 13. App SeekBar + `cros set … vol=N`.
- **Link noise filter** (`noise=0..5`): soft gate + mild HF rolloff on good-ear
  SCO PCM (hiss between speech). `0` = off (default). App SeekBar.
- Get/set status now includes `vol=` and `noise=`.

### Test
1. Flash both — `init v0.3.48`. Rebuild/install app **0.3.4**.
2. Quad-tap on → expect `[cros_tws] ENABLE` (+ later OPENED if teed).
3. Volume slider → Apply → hear level change; log `vol=…`.
4. Noise 2–3 → Apply — hiss should drop; speech still clear. Try 0 vs 5.
5. Quad-tap off → phone log `[cros_tws] DISABLE` and/or `[cros_sco] CLOSED`.

## [0.3.47] — 2026-09-27

### Firmware — Apply/Get confirmation on phone during CROS
- **0.3.46** paused the TOTA log tee while SCO hold — general spam stays off,
  but that also hid `[cros_cfg] set` / `get` after Apply.
- **Fix:** one-line **ack** slot (`CROS_LOG_ACK`) still flushes under hold so
  the phone shows e.g. `[cros_cfg] set poor=RIGHT mix=-18dB …` after Apply.
  Other CROS_LOG lines remain UART-only while SCO is up.

### Test
1. Flash both — `init v0.3.47`.
2. Quad-tap CROS; Apply mix — expect phone log `[cros_cfg] set … mix=…`.
3. Get — expect `[cros_cfg] get …`. Taps still work.

## [0.3.46] — 2026-09-27

### Firmware — fix Apply / quad-tap dead after CROS (BT wedge)
- **Ear report (log 221736):** after quad-tap, BiCROS audio worked but
  **Apply did nothing**, phone `cros set`/`get` got no firmware reply, and
  **four taps could not disable** — had to pocket the case. Log stopped at
  `register_link rc=0` (no `open_link` / `OPENED`).
- **Cause:** `sco_register_link` + `sco_open_link` ran in one BT-thread call;
  `open_link` needs the HCI event loop between them → BT thread stuck → SPP
  RX / key path dead. Secondary: `tota_printf` flush on BT during peer SCO.
- **Fix:**
  - Split: settle → **register only** → 100 ms gap → **open_link**
  - `cros_sco_cfg_hold()` — pause TOTA SPP log flush while CROS owns BT
  - Defer TOTA `cros set`/`get` off SPP RX (10 ms timer); skip IBRT peer sync
    while hold (local mix/EQ still apply)
- Expect phone logs to go quiet while CROS is on (UART still traces); Apply
  and taps must stay responsive.

### Test
1. Flash both — `init v0.3.46` / `hold+split-open`.
2. Capture LEFT. Quad-tap. Expect `register_link` then `open gap done` then
   `open_link` / `OPENED` (not silence after register).
3. App: change mix → **Apply** — hear change; tap still works.
4. Quad-tap off without using the case.

## [0.3.45] — 2026-09-27

### Firmware — fix BiCROS knob howling (CRITICAL)
- **Ear report:** any `cros set` caused loud mic feedback / squelch on master.
- **Cause:** phone sent `mix=-20` but firmware stored **`mix=0`** (and
  `treble=-1`→`0`) — broken negative parsing via `atoi` — then 0 dB HW
  sidetone howled in-ear. Apply also tore sidetone down/up on every tweak.
- **Fix:**
  - Hand-rolled signed int parser (no `atoi`)
  - Mix hard ceiling **−12 dB** (never 0)
  - Live mix/EQ update gain/coeffs only — no sidetone disable→enable
  - Full reshape only when poor side changes
- App: mix SeekBar capped at −12 dB (0.3.3)

### Test
1. Flash both — `init v0.3.45`.
2. Quad-tap CROS; confirm normal BiCROS at mix=-20.
3. Connect app → change bass/treble only → **Apply** — must stay quiet;
   log `mix=-20` (not 0).
4. Sweep mix −30…−12 — no howling. Expect refuse of 0 dB.

## [0.3.44] — 2026-09-27

### Firmware + app — BiCROS knobs (mix / EQ / poor side)
- Phone **TOTA string** cmds (unencrypted `OP_TOTA_STRING` RX fix):
  - `cros get`
  - `cros set poor=right|left mix=-20 bass=0 treble=0`
- **Mix:** runtime HW sidetone gain (−30…0 dB, 2 dB steps); default −20.
- **EQ:** soft bass/treble shelves on good-ear SCO PCM (−6…+6 dB).
- **Poor side:** runtime select (keeps IBRT-master×TX refuse); syncs to peer via
  `APP_IBRT_CUSTOM_CMD_CROS_CFG`.
- Android **CROS Control** app: SeekBars + poor-side radios + Apply/Get.

### Test
1. Flash both — `init v0.3.44` / `SCO-BiCROS-knobs`.
2. App: Capture on → expect `[cros_cfg] …` from auto `cros get`.
3. Change mix/bass/treble/poor → **Apply** → hear change; log `[cros_cfg] set`.
4. Quad-tap CROS; confirm BiCROS still mixes. If poor=LEFT and LEFT is master,
   enable must **REFUSE** (guard).

## [0.3.43] — 2026-09-26

### Firmware — BiCROS: mix LEFT (good) mic into local playback
- **0.3.42 ear MILESTONE:** mSBC/16k — “good quality phone call,” great latency,
  very usable. EQ later.
- **0.3.43:** on GOOD/RX enable **HW codec sidetone** (tgt −20 dB, CH4) so local
  left mic mixes into left speaker with contralateral SCO. Digital mic TX stays
  muted (left not sent over SCO). POOR keeps sidetone OFF. Build: `SPEECH_SIDETONE=1`.

### Test (LEFT master)
1. Flash both — `init v0.3.43` / `BiCROS-sidetone`.
2. Capture LEFT. Quad-tap. Expect `BiCROS GOOD/RX` + `sidetone ON`.
3. Cover/speak at RIGHT — hear on LEFT (CROS). Speak near LEFT — should also
   hear own voice on LEFT (local mix). RIGHT should stay quiet.
4. Paste LEFT log + whether local left mic is audible in the mix.

### Result (2026-09-26 ear — LEFT master) — **BiCROS PASS**
`BiCROS GOOD/RX — SCO+local mic mix, no TX; hfp_vol 4→13 sidetone ON`.
**Mixing confirmed.** Usable BiCROS with ~140 ms SCO latency. Product-path
milestone: stability + latency + CROS shape + local mix.

## [0.3.42] — 2026-09-26

### Firmware — mSBC **16 kHz** on peer SCO CROS (was CVSD 8 kHz)
- **0.3.41 ear:** louder (`hfp_vol 6→13`); **no dropouts**; latency/stability good.
  Tablet volume does **not** work in CROS (no HFP AG on peer SCO) — bud keys do.
  “Rough / dropout-like” was **8 kHz CVSD**, not link loss.
- **0.3.42:** `hfp_ibrt_sco_audio_connected(MSBC)` + `ibrt_sco_codec=MSBC`.
  Expect log `codec=mSBC/16k` / `HF_IBRT_AUDIO_CONNECTED codec=2`.

### Test (LEFT master)
1. Flash both — `init v0.3.42` / probe init shows `mSBC/16k`.
2. Capture LEFT. Quad-tap. Expect voice START `mSBC 16k`, `codec=2`, CROS shape,
   `hfp_vol →13`.
3. Speech should sound clearer than 8 kHz. Note stability + rough clap delay.
4. Paste LEFT log. If broken/silent, we can fall back (`CROS_SCO_MSBC=0`).

### Result (2026-09-26 ear — LEFT master) — **MILESTONE**
`codec=2` mSBC/16k. “Sounds like a good quality phone call” with great latency;
very usable. EQ/smooth later. **SCO CROS latency+quality milestone met.**
Next: mix LEFT (good-ear) mic into playback (BiCROS).

## [0.3.41] — 2026-09-26

### Firmware — raise SCO/HFP playback volume on CROS (good side)
- **0.3.40 ear:** CROS shape worked (`GOOD/RX`); little dropout; **volume very low**.
- No CROS-specific volume before — stock **HFP call volume** (often low; phone
  HFP vol events never arrive on peer SCO). Bud volume keys *do* hit `hfp_vol`
  while `APP_BT_STREAM_HFP_PCM` runs.
- **0.3.41:** on CROS mute apply (good side), set HFP vol to **13/15** and log
  `hfp_vol before→after`. Keys still work to trim.

### Test (LEFT master)
1. Flash both — `init v0.3.41`.
2. Capture LEFT. Quad-tap. Expect `CROS shape GOOD/RX` + `hfp_vol N→13`.
3. Confirm louder than 0.3.40; bud vol up/down still adjusts. Note dropouts.
4. Paste LEFT log.

### Result (2026-09-26 ear — LEFT master) — **louder, stable**
`hfp_vol 6→13`. No dropouts; latency/stability good. Tablet vol N/A on peer SCO.
Roughness = CVSD 8 kHz. Follow-up: **v0.3.42** mSBC 16 kHz.

## [0.3.40] — 2026-09-26

### Firmware — asymmetric CROS on peer SCO (poor TX / good RX)
- Priority: **Stability → Latency → Quality** (quality still deferred).
- **0.3.39:** SCO media ~140 ms full-duplex (call-shaped).
- **0.3.40:** after voice START, `bt_sco_player_forcemute`:
  - **POOR/TX (RIGHT):** mic ON, spk OFF
  - **GOOD/RX (LEFT):** mic OFF, spk ON
- Same alone+CVSD path. Expect log lines `CROS shape POOR/TX` / `GOOD/RX`.

### Test (LEFT master, poor=RIGHT)
1. Flash both — `init v0.3.40` / `ALONE+MEDIA+CROS`.
2. Capture LEFT. Quad-tap. Expect OPENED → voice START → `CROS shape GOOD/RX`.
3. Speak at **RIGHT** outer face — hear on **LEFT** only. LEFT mic should not
   feed RIGHT (no reverse CROS). Clap still ~140 ms-class if stable.
4. Paste LEFT log + whether direction feels like CROS (not a phone call).

### Result (2026-09-26 ear — LEFT master) — **CROS shape OK, quiet**
`CROS shape GOOD/RX — mic OFF, spk ON`. Direction worked; some dropout; volume
very low (HFP default). Follow-up: **v0.3.41** bump good-side hfp_vol.

## [0.3.39] — 2026-09-26

### Breakthrough — bud↔bud SCO media ≈ **140 ms** (ear)
Peer SCO carries live CVSD voice. Clap ≈ **140 ms** vs ≈ **330 ms** on extra L2CAP
(v0.3.27) — roughly **half the delay**. Link held steady. Quality still call-path
rough; latency path is proven.

### Firmware — first SCO media: CVSD voice player on peer OPENED
- **0.3.38 ear PASS (~2 min):** silence + SCO alone held; clean close.
- **0.3.39:** still alone (no extra). On OPENED call
  `hfp_ibrt_sco_audio_connected(CVSD, peer_sco_hdl)` — stock call PCM path
  against bud↔bud SCO. Full-duplex trial (not asymmetric CROS yet).

### Test (LEFT master)
1. Flash both — `init v0.3.39` / `ALONE+MEDIA`.
2. Capture LEFT. Quad-tap. Expect silence ACL, then OPENED → `voice START`.
3. Talk into RIGHT (poor), listen LEFT; clap if anything is audible.
4. Quad-tap off. Paste LEFT log + what you heard.

### Result (2026-09-26 ear — LEFT master) — **PASS ~140 ms**
`sco_hdl=0x0181` → `HF_IBRT_AUDIO_CONNECTED codec=1` → `voice START done rc=0`.
Link held steady. **Clap ~140 ms** (vs ~330 ms extra v0.3.27). Audio quality
rough (CVSD / call path) — defer; **SCO is the latency path**. Clean
DISCONNECT/STOP/CLOSED.

**Next:** quality (MSBC / tuning) + asymmetric CROS (poor mic → good speaker only).
Daily wear until then: **v0.3.27** extra.

## [0.3.38] — 2026-09-26

### Firmware — SCO alone = silence (no ACL CROS under SCO)
- **0.3.37 ear:** SCO **held** alone (no wedge). Cmd-path CROS under SCO was
  **very choppy** (with and without Capture). Capture overrun = underrun STAT
  spam on SPP (alone never quiets; armed reset re-fired every packet).
- **0.3.38:** alone mode **does not start ACL TX/RX** — sniff lock + SCO hold
  only (silence). Fix underrun-threshold armed reset. Next: mic→SCO→speaker.

### Test (LEFT master)
1. Flash both — `init v0.3.38` / `SCO-alone-silence`.
2. Capture LEFT. Quad-tap. Expect silence (no chop), `OPENED (alone hold…)`,
   buds responsive for 10–20 s; off → `close_link`.
3. Paste LEFT log (should stay short — no underrun flood).

### Result (2026-09-26 ear — LEFT master) — **PASS (~2 min)**
Silence + `OPENED (alone hold…)`; stayed up ~2 min with Capture on; clean
DISABLE → `close_link`/`unregister`. No underrun flood. Peer SCO without ACL
CROS is stable. Next: mic→SCO→speaker.

## [0.3.37] — 2026-09-26

### Firmware — SCO alone hold (no extra)
- **0.3.36 ear PASS:** OPENED → auto-close; extra held through SCO exit.
- **0.3.37:** `CROS_SCO_ALONE=1` — **do not open extra L2CAP**; settle ~1.5 s from
  CROS enable; open peer SCO; **leave OPENED up** until disable.
- Audio stays on **cmd** path (no extra). Goal: prove SCO without extra media
  does not wedge. Next after PASS: mic→SCO→speaker.

### Test (LEFT master)
1. Flash both — `init v0.3.37` / probe init shows `ALONE hold`.
2. Capture LEFT. Quad-tap. Leave CROS on ~10–20 s after OPENED.
3. Expect: `armed ALONE` → `OPENED (alone hold…)` — **no** `proof hold done`.
   Buds stay responsive; quad-tap off → `close_link` / `CLOSED`.
4. Paste LEFT log (PASS or wedge).

### Result (2026-09-26 ear — LEFT master) — **held, cmd choppy**
`OPENED (alone hold…)` stayed up (also held with Capture off). Cmd ACL CROS
under SCO: **very choppy**; Capture overrun from underrun STAT flood (quiet
never on). Follow-up: **v0.3.38** silence ACL under alone SCO.

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

### Result (2026-09-26 ear — LEFT master) — **PASS**
`init v0.3.36` / `auto-close after OPENED`. Sequence: `OPENED (… tearing down)` →
`proof hold done — close` → `close_link rc=0` / `unregister rc=0` →
`BTEVENT_SCO_DISCONNECT err=0x2a`. **Extra pipe held** through SCO exit (user).
CROS then disabled cleanly (`DISABLE` / `RX STOP`). §K proof complete; next is
SCO media **without** extra coexistence.

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
pan out. Latency chase next: **SCO/eSCO bud↔bud** ([latency-and-next.md](docs/archive/latency-and-next.md) §K).

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
[docs/latency-and-next.md](docs/archive/latency-and-next.md).

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
  `OP_TOTA_STRING` instead of soldering UART or inventing BLE GATT — [docs/bt-log-sink.md](docs/archive/bt-log-sink.md)
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
