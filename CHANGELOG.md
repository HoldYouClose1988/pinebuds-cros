# Changelog

All notable flash-package / firmware iterations for PineBuds Pro CROS + SITE.
Format: version, date (UTC), then user-facing changes.

**This project is a work in progress and is not a functional CROS product.** DIY / own-risk - not a hearing aid or PPE.

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
