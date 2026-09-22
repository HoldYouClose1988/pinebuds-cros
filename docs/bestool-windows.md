# Installing and using bestool (Windows)

`bestool` is the community flasher for PineBuds Pro / BES2300 images.

Source: [github.com/Ralim/bestool](https://github.com/Ralim/bestool) (MIT tool + BES programmer blob — see `NOTICE`).

**Flash packages ship `bestool.exe` in the zip.** You do **not** need Rust unless you want to rebuild it yourself.

## Prerequisites

1. **WCH CH342 driver** — plug the case in USB-C; Device Manager → **Ports (COM & LPT)** should show **two** COM ports (e.g. COM5 and COM6).  
   Driver: [WCH CH342/CH343](http://www.wch-ic.com/downloads/CH343SER_EXE.html)
2. Unzip the flash package so `bestool.exe`, `flash.ps1`, and `open_source.bin` are in the same folder.

If Windows Defender quarantines `bestool.exe`, restore/allow it (unsigned Rust binary).

## Critical: bootloader Sync order

The BES2300 only stays in the programmer bootloader if bestool’s **Sync** is ACKed **during reset**.  
If the bud is already seated when Sync starts, bestool hangs on:

```text
Sent message type Sync
```

Correct order **per bud**:

1. Bud **out** of the case (LED shows awake).
2. Start `bestool` / press Enter in `backup.ps1` / `flash.ps1` (Sync begins).
3. **Immediately reseat** that bud (case pogopins reset the chip so Sync catches boot).

Do **one COM port at a time**. `backup.ps1` and `flash.ps1` prompt for this.

## Backup once

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6
```

Scripts auto-find `.\bestool.exe`. Keep `backups\*.bin` safe.

## Flash

```powershell
.\flash.ps1 -Port0 COM5 -Port1 COM6
```

## Optional: rebuild bestool yourself

Only needed if you want a newer upstream binary:

```powershell
git clone https://github.com/Ralim/bestool.git
cd bestool\bestool
cargo build --release
# → target\release\bestool.exe
```

Copy over the shipped `bestool.exe`, or put it on PATH / pass `-Bestool`.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Hangs on `Sent message type Sync` | Bud was seated too early. Ctrl+C; bud out → start bestool → reseat |
| `bestool not found` | Unzip so `bestool.exe` sits next to `flash.ps1`, or pass `-Bestool .\bestool.exe` |
| Defender deleted the exe | Allow / restore; re-download the zip if needed |
| Timeout mid-read | Watchdog on large dumps; rerun `read-image` |
| Wrong COM | Confirm the CH342 pair in Device Manager |

## Factory programmer fallback

If bestool misbehaves, use PINE64 `dld_main` (APP only for community bins). See `FLASH.md` and the [programmer user manual](https://files.pine64.org/os/PineBudsPro/PineBuds%20Pro%20programmer%20user%20manual.pdf). Official tool also wants buds **out**, then Start, then reseat.
