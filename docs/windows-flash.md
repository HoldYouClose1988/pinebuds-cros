# Windows flashing (primary workstation)

Cloud Agents **build** firmware; you **flash** on Windows over USB. Aim for this path first.

## One-time setup

### 1. USB serial driver (CH342)

The case uses a WCH **CH342** dual-UART chip. Plug the case in (USB-C) and check **Device Manager → Ports (COM & LPT)**.

- You want **two** new COM ports (e.g. `COM5` and `COM6`).
- If you only see “Unknown device”, install the [WCH CH342/CH343 Windows driver](http://www.wch-ic.com/downloads/CH343SER_EXE.html) (or the CH342 package from WCH).
- Note both COM numbers — you’ll use them for every flash.

### 2. Flash tool (pick one)

| Tool | When to use |
|------|-------------|
| **`bestool`** (Rust) | Day-to-day community firmware (APP bin only) |
| **PINE64 Windows programmer** (`dld_main`) | Factory restore, or if bestool misbehaves |

#### Option A — `bestool` on Windows

Full walkthrough: [bestool-windows.md](bestool-windows.md) (also shipped as `BESTOOL.md` inside every flash zip).

1. Install [Rust](https://rustup.rs/) (MSVC toolchain is fine).
2. Build:

```powershell
git clone https://github.com/Ralim/bestool.git
cd bestool\bestool
cargo build --release
# binary: target\release\bestool.exe
```

3. Put `bestool.exe` on your PATH, copy it next to the flash scripts, or pass `-Bestool`.

#### Option B — Official Windows programmer

From the [PINE64 PineBuds Pro software page](https://pine64.org/documentation/PineBuds_Pro/Software/):

- [Programmer utility](https://wiki.pine64.org/wiki/PineBuds_Pro#Firmware_images) (download zip)
- [User manual (PDF)](https://files.pine64.org/os/PineBudsPro/PineBuds%20Pro%20programmer%20user%20manual.pdf)

For **community** firmware: program **APP only** (untick Factory mode). For **factory restore**: APP + OTA BOOT as in the manual.

## Get a firmware image

**Preferred:** download a flash package from GitHub (bin + PowerShell helpers + `FLASH.md`):

- [pinebuds-cros-LATEST.zip](../flash-packages/pinebuds-cros-LATEST.zip) — always the newest iteration  
- [flash-packages/](../flash-packages/) — history + checksums  

Or build on WSL2/Linux/Docker, then run `./scripts/package-flash.sh` (or copy `open_source.bin` yourself).

Do **not** expect the Cloud Agent to reach your COM ports.

## Backup before first custom flash (strongly recommended)

BES2300 bootloader Sync only works if the bud **resets after** bestool starts. Per bud: out (LED on) → start bestool → **immediately reseat**. Details: [bestool-windows.md](bestool-windows.md).

```powershell
# From unzipped flash package (prompts Sync order for each port)
.\backup.ps1 -Port0 COM5 -Port1 COM6
```

Or manually (same Sync order for each port):

```powershell
# Bud OUT -> run -> IMMEDIATELY reseat
bestool.exe read-image backup-left.bin --port COM5
bestool.exe read-image backup-right.bin --port COM6
```

Keep those files somewhere safe. Factory images are also on the PINE64 wiki if a backup fails.

## Flash community firmware (`bestool`)

1. Case plugged into USB; note both COM ports.
2. Flash **each** port with Sync order (out → start → reseat). Same image to both:

```powershell
.\flash.ps1 -Port0 COM5 -Port1 COM6
# (from an unzipped flash-packages zip; BinPath defaults to .\open_source.bin)
```

Manual equivalent:

```powershell
bestool.exe write-image open_source.bin --port COM5
bestool.exe write-image open_source.bin --port COM6
```

3. After success: leave buds in case ~30s for TWS re-pair (LEDs / behavior per upstream docs).

## Flash with official `dld_main`

1. Production Config → select folder containing the APP `.bin` → tick **APP** only for community builds → Save.  
2. COM settings → tick **both** cradle COM ports → OK.  
3. Take buds out → **All Start** → reseat buds; wait for green success.  
4. Stop; reseat again and wait for self-pairing as in the manual.

## Flash budget

On-chip flash is rated for a limited number of erase cycles (~500). Treat each successful program as costly: validate builds in software first; flash only when you mean it.

## Troubleshooting

| Symptom | Try |
|---------|-----|
| No COM ports | CH342 driver; try another cable (data-capable); different USB port |
| Hangs on `Sent message type Sync` | Bud seated too early. Ctrl+C; out → start bestool → reseat |
| bestool times out | Confirm COM pair; close other serial apps; retry Sync order |
| Only one bud updates | Flash the other COM explicitly; don’t assume “All Start” hit both |
| Soft-brick | Restore factory APP (+ OTA if required) with `dld_main` + wiki images |
| WSL2 can’t see COM | Prefer **native Windows** bestool/`dld_main`; WSL serial passthrough is unreliable |

## What stays on Windows vs cloud

| Task | Where |
|------|--------|
| Compile firmware | Cloud Agent / Linux CI (or WSL if you want) |
| Flash / backup / UART log | **Windows PC + case** |
| Android companion later | Android device + Windows for USB debug as needed |
