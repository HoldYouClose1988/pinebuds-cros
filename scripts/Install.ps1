# PineBuds Pro - BiCROS friendly installer (Windows)
#
# Double-click or run:  powershell -ExecutionPolicy Bypass -File .\Install.ps1
# Walks you through backup + flash with on-screen instructions.
# Requires: CH342 driver, case USB plugged in, this folder unzipped intact.
param(
  [string]$BinPath = ".\open_source.bin",
  [string]$Port0 = "",
  [string]$Port1 = "",
  [string]$Bestool = "",
  [switch]$SkipIntro
)

$ErrorActionPreference = "Stop"
$Here = $PSScriptRoot
if (-not $Here) { $Here = (Get-Location).Path }
Set-Location $Here

function Write-Banner {
  Write-Host ""
  Write-Host "============================================================" -ForegroundColor Cyan
  Write-Host "  PineBuds Pro - BiCROS flasher" -ForegroundColor Cyan
  Write-Host "  DIY / own-risk - NOT a hearing aid or medical device" -ForegroundColor Yellow
  Write-Host "============================================================" -ForegroundColor Cyan
  Write-Host ""
}

function Write-FlashInstructions {
  Write-Host "FLASHING INSTRUCTIONS (read once)" -ForegroundColor Green
  Write-Host "---------------------------------"
  Write-Host "1. Install the WCH CH342 USB driver if Device Manager does not"
  Write-Host "   show TWO COM ports when the case is plugged in:"
  Write-Host "   http://www.wch-ic.com/downloads/CH343SER_EXE.html"
  Write-Host ""
  Write-Host "2. Unzip this whole folder. Keep bestool.exe next to these scripts."
  Write-Host "   If Windows Defender quarantines bestool.exe, restore/allow it."
  Write-Host ""
  Write-Host "3. Plug the charging case into USB. Note the two COM ports"
  Write-Host "   (e.g. COM5 and COM6) under Device Manager -> Ports."
  Write-Host ""
  Write-Host "4. CRITICAL Sync order - do this for EACH bud, one at a time:"
  Write-Host "     a. Take that bud OUT of the case (LED awake)."
  Write-Host "     b. Press Enter in this window (bestool starts Sync)."
  Write-Host "     c. IMMEDIATELY reseat the bud (case reset catches Sync)."
  Write-Host "   If it hangs on 'Sent message type Sync': Ctrl+C and retry."
  Write-Host ""
  Write-Host "5. After flash: leave BOTH buds in the case 30-60 seconds"
  Write-Host "   so they re-pair (TWS). Then pair to an Android phone."
  Write-Host ""
  Write-Host "6. Day to day: wear both, QUAD-TAP to toggle BiCROS."
  Write-Host "   Optional: sideload CROScontrol.apk for knobs / Help."
  Write-Host ""
}

function Get-CandidateComPorts {
  $ports = @()
  try {
    $ports = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
      Sort-Object DeviceID |
      ForEach-Object { $_.DeviceID })
  } catch { }
  if (-not $ports -or $ports.Count -eq 0) {
    try {
      $ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    } catch { }
  }
  return @($ports)
}

function Resolve-BestoolLocal([string]$Hint) {
  if ($Hint -and (Test-Path $Hint)) { return (Resolve-Path $Hint).Path }
  if (Test-Path ".\bestool.exe") { return (Resolve-Path ".\bestool.exe").Path }
  $cmd = Get-Command "bestool.exe" -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $cmd = Get-Command "bestool" -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  return $null
}

function Select-ComPorts {
  param([string]$A, [string]$B)

  if ($A -and $B) { return @($A, $B) }

  $found = Get-CandidateComPorts
  Write-Host "Detected serial ports: $(if ($found.Count) { $found -join ', ' } else { '(none)' })" -ForegroundColor Yellow

  if ($found.Count -eq 2 -and -not $A -and -not $B) {
    Write-Host "Using detected pair: $($found[0]) + $($found[1])" -ForegroundColor Green
    $ok = Read-Host "OK? [Y/n]"
    if (-not $ok -or $ok -match '^[Yy]') { return @($found[0], $found[1]) }
  }

  if ($found.Count -gt 0) {
    Write-Host ""
    Write-Host "Pick ports (numbers below), or type COM names directly:"
    for ($i = 0; $i -lt $found.Count; $i++) {
      Write-Host ("  [{0}] {1}" -f ($i + 1), $found[$i])
    }
  }

  if (-not $A) {
    $raw = Read-Host "Port for LEFT bud (e.g. COM5 or 1)"
    if ($raw -match '^\d+$' -and $found.Count -ge [int]$raw) {
      $A = $found[[int]$raw - 1]
    } else {
      $A = $raw.Trim().ToUpper()
      if ($A -notmatch '^COM') { $A = "COM$A" }
    }
  }
  if (-not $B) {
    $raw = Read-Host "Port for RIGHT bud (e.g. COM6 or 2)"
    if ($raw -match '^\d+$' -and $found.Count -ge [int]$raw) {
      $B = $found[[int]$raw - 1]
    } else {
      $B = $raw.Trim().ToUpper()
      if ($B -notmatch '^COM') { $B = "COM$B" }
    }
  }

  if (-not $A -or -not $B) { throw "Need two COM ports (LEFT and RIGHT)." }
  if ($A -eq $B) { throw "LEFT and RIGHT ports must be different (got $A twice)." }
  return @($A, $B)
}

function Show-ApkHelp {
  $apk = Join-Path $Here "CROScontrol.apk"
  Write-Host ""
  Write-Host "Android app (CROS Control) - sideload" -ForegroundColor Green
  Write-Host "-------------------------------------"
  if (Test-Path $apk) {
    Write-Host "APK in this folder: CROScontrol.apk"
  } else {
    Write-Host "CROScontrol.apk should be in this zip. If missing, grab it from the"
    Write-Host "GitHub Release assets or build from android/cros-log/."
  }
  Write-Host ""
  Write-Host "1. Copy CROScontrol.apk to your Android phone."
  Write-Host "2. On the phone: Settings -> allow Install unknown apps for Files/Chrome."
  Write-Host "3. Open the APK and install (not on Play Store - sideload only)."
  Write-Host "4. Pair PineBuds Pro in Bluetooth settings first."
  Write-Host "5. Open CROS Control -> Connect -> Apply knobs once."
  Write-Host "   After Apply, knobs live on the buds (quad-tap works without the app)."
  Write-Host ""
  Write-Host "Full app source: android/cros-log/ in the GitHub repo."
  Write-Host ""
}

function Invoke-Backup {
  param([string]$P0, [string]$P1, [string]$Bt)
  $backupScript = Join-Path $Here "backup.ps1"
  if (-not (Test-Path $backupScript)) { throw "backup.ps1 missing from this folder." }
  & $backupScript -Port0 $P0 -Port1 $P1 -Bestool $Bt
}

function Invoke-Flash {
  param([string]$P0, [string]$P1, [string]$Bt, [string]$Bin)
  $flashScript = Join-Path $Here "flash.ps1"
  if (-not (Test-Path $flashScript)) { throw "flash.ps1 missing from this folder." }
  & $flashScript -Port0 $P0 -Port1 $P1 -Bestool $Bt -BinPath $Bin
}

# --- main ---
Write-Banner
if (-not $SkipIntro) {
  Write-FlashInstructions
  [void](Read-Host "Press Enter to continue")
}

$verFile = Join-Path $Here "VERSION"
$ver = if (Test-Path $verFile) { (Get-Content $verFile -Raw).Trim() } else { "?" }
Write-Host "Package version: $ver"
if (-not (Test-Path $BinPath)) {
  throw "Firmware image not found: $BinPath (unzip the full flash package)."
}
$bestoolPath = Resolve-BestoolLocal $Bestool
if (-not $bestoolPath) {
  throw "bestool.exe not found. It should sit next to Install.ps1 in this zip. See BESTOOL.md."
}
Write-Host "Firmware: $BinPath"
Write-Host "Bestool:  $bestoolPath"
Write-Host ""

$ports = Select-ComPorts -A $Port0 -B $Port1
$Port0 = $ports[0]
$Port1 = $ports[1]
Write-Host ""
Write-Host "LEFT  -> $Port0"
Write-Host "RIGHT -> $Port1"
Write-Host ""

while ($true) {
  Write-Host "What do you want to do?" -ForegroundColor Cyan
  Write-Host "  [1] Backup stock firmware (do this once before first custom flash)"
  Write-Host "  [2] Flash BiCROS firmware to both buds"
  Write-Host "  [3] Backup THEN flash  (recommended first time)"
  Write-Host "  [4] Show Android APK install steps"
  Write-Host "  [5] Re-print flashing instructions"
  Write-Host "  [6] Exit"
  $choice = Read-Host "Choice"

  switch ($choice) {
    "1" {
      Invoke-Backup -P0 $Port0 -P1 $Port1 -Bt $bestoolPath
      Write-Host ""
      Write-Host "Backup done. Keep backups\*.bin somewhere safe." -ForegroundColor Green
    }
    "2" {
      Invoke-Flash -P0 $Port0 -P1 $Port1 -Bt $bestoolPath -Bin $BinPath
      Write-Host ""
      Write-Host "Flash done. Leave both buds seated 30-60s for TWS re-pair." -ForegroundColor Green
      Show-ApkHelp
    }
    "3" {
      Invoke-Backup -P0 $Port0 -P1 $Port1 -Bt $bestoolPath
      Write-Host ""
      Write-Host "Backup saved. Starting flash..." -ForegroundColor Green
      Invoke-Flash -P0 $Port0 -P1 $Port1 -Bt $bestoolPath -Bin $BinPath
      Write-Host ""
      Write-Host "All set. Leave both buds seated 30-60s, then pair Android." -ForegroundColor Green
      Show-ApkHelp
    }
    "4" { Show-ApkHelp }
    "5" { Write-FlashInstructions }
    "6" { break }
    default { Write-Host "Enter 1-6." -ForegroundColor Yellow }
  }
  Write-Host ""
}

Write-Host "Bye. Quad-tap toggles BiCROS once the buds are paired on Android."
