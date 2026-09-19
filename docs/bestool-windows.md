# Installing and using bestool (Windows)

`bestool` is the community flasher for PineBuds Pro / BES2300 images.  
This flash package does **not** bundle `bestool.exe` (it is a Windows host binary you build once).

Source: [github.com/Ralim/bestool](https://github.com/Ralim/bestool)

## Prerequisites

1. **WCH CH342 driver** — plug the case in USB-C; Device Manager → **Ports (COM & LPT)** should show **two** COM ports (e.g. COM5 and COM6).  
   Driver: [WCH CH342/CH343](http://www.wch-ic.com/downloads/CH343SER_EXE.html)
2. **Rust (MSVC toolchain)** — install from [rustup.rs](https://rustup.rs/). In the installer, choose the default host triple for 64-bit Windows (`x86_64-pc-windows-msvc`). You may need [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with “Desktop development with C++”.
3. Git for Windows (to clone bestool).

## Build bestool.exe (one-time)

Open **PowerShell**:

```powershell
git clone https://github.com/Ralim/bestool.git
cd bestool\bestool
cargo build --release
```

Binary path:

```text
bestool\bestool\target\release\bestool.exe
```

### Put it on PATH (pick one)

**Option A — copy next to this flash package** (simple for one-off flashes):

```powershell
Copy-Item .\path\to\bestool.exe .
# Then from the unzipped package folder, flash.ps1 / backup.ps1 find it as .\bestool.exe
```

If you keep `bestool.exe` in the same folder as `flash.ps1`, call:

```powershell
.\flash.ps1 -Port0 COM5 -Port1 COM6 -Bestool .\bestool.exe
```

**Option B — user PATH:**

```powershell
# Example: permanent user PATH entry
[Environment]::SetEnvironmentVariable(
  "Path",
  $env:Path + ";C:\Tools\bestool",
  "User"
)
# Copy bestool.exe into C:\Tools\bestool, then open a new PowerShell
```

Verify:

```powershell
bestool.exe --help
```

## Backup stock firmware (do this once)

Seat both buds, plug the case in, note your two COM ports.

```powershell
.\backup.ps1 -Port0 COM5 -Port1 COM6 -Bestool .\bestool.exe
```

Or manually:

```powershell
.\bestool.exe read-image backup-left.bin --port COM5
.\bestool.exe read-image backup-right.bin --port COM6
```

Keep those files safe. Factory images also live on the [PINE64 wiki](https://wiki.pine64.org/wiki/PineBuds_Pro#Firmware_images).

## Flash this package

1. Seat both buds; case on USB.
2. Wake the programmer window: remove buds ~3 seconds and reseat, **or** long-hold the rear button in-case (~5 seconds).
3. From the unzipped package folder:

```powershell
.\flash.ps1 -Port0 COM5 -Port1 COM6 -Bestool .\bestool.exe
```

Equivalent manual commands:

```powershell
.\bestool.exe write-image open_source.bin --port COM5
.\bestool.exe write-image open_source.bin --port COM6
```

4. Leave buds in the case ~30 seconds for TWS re-pair.

## Troubleshooting

| Symptom | What to try |
|---------|-------------|
| `bestool not found` | Pass `-Bestool .\bestool.exe` or add it to PATH; new PowerShell window after PATH change |
| Cargo / link errors | Install VS Build Tools (C++), then `rustup default stable` and rebuild |
| Timeout on write/read | Wake buds again; confirm COM numbers; close other serial apps; try the other USB port |
| Only one bud updates | Flash the quiet COM port again explicitly |
| Soft-brick | Restore with PINE64 `dld_main` + factory APP (+ OTA if required) |

## Optional: official programmer

If bestool misbehaves, use PINE64 `dld_main` (APP only for community bins). See `FLASH.md` and the [programmer user manual](https://files.pine64.org/os/PineBudsPro/PineBuds%20Pro%20programmer%20user%20manual.pdf).

## Flash budget

On-chip flash has limited erase cycles (~500). Prefer validating builds in software; flash only when you intend to.
