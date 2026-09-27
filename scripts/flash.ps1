# Flash PineBuds Pro from Windows (bestool)
#
# Friendly path: run Install.ps1 instead (menu + instructions).
#
# BES2300 only enters the programmer bootloader if bestool's Sync is ACKed
# during reset. Correct order per bud:
#   1) bud OUT of case (awake / LEDs on)
#   2) start bestool (begins Sync)
#   3) IMMEDIATELY reseat that bud (case reset -> bootloader ACK)
param(
  [string]$BinPath = ".\open_source.bin",
  [string]$Port0 = "",
  [string]$Port1 = "",
  [string]$Bestool = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BinPath)) {
  throw "Firmware image not found: $BinPath"
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

function Resolve-Ports([string]$A, [string]$B) {
  if ($A -and $B) { return @($A, $B) }
  $found = Get-CandidateComPorts
  Write-Host "Detected COM ports: $(if ($found.Count) { $found -join ', ' } else { '(none - check CH342 driver)' })"
  if (-not $A -and -not $B -and $found.Count -eq 2) {
    Write-Host "Auto-selecting $($found[0]) (LEFT) and $($found[1]) (RIGHT)."
    return @($found[0], $found[1])
  }
  if (-not $A) {
    $A = Read-Host "COM port for LEFT bud (e.g. COM5)"
    $A = $A.Trim().ToUpper()
    if ($A -notmatch '^COM') { $A = "COM$A" }
  }
  if (-not $B) {
    $B = Read-Host "COM port for RIGHT bud (e.g. COM6)"
    $B = $B.Trim().ToUpper()
    if ($B -notmatch '^COM') { $B = "COM$B" }
  }
  if (-not $A -or -not $B) { throw "Need -Port0 and -Port1 (or run Install.ps1)." }
  if ($A -eq $B) { throw "LEFT and RIGHT ports must differ." }
  return @($A, $B)
}

function Resolve-Bestool([string]$Hint) {
  if ($Hint -and (Test-Path $Hint)) { return (Resolve-Path $Hint).Path }
  if (Test-Path ".\bestool.exe") { return (Resolve-Path ".\bestool.exe").Path }
  $cmd = Get-Command "bestool.exe" -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $cmd = Get-Command "bestool" -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  return $null
}

function Invoke-BestoolWithReseat {
  param(
    [string]$BestoolPath,
    [string[]]$BestoolArgs,
    [string]$Port,
    [string]$BudLabel
  )

  Write-Host ""
  Write-Host "=== $BudLabel ($Port) ===" -ForegroundColor Cyan
  Write-Host "Sync order (required or bestool hangs):"
  Write-Host "  1. Remove the $BudLabel bud from the case; wait until its LED shows it is awake."
  Write-Host "  2. Press Enter here - bestool will open $Port and start Sync."
  Write-Host "  3. IMMEDIATELY reseat that bud (case contact = reset). Sync must catch the boot."
  Write-Host "     If it sits on 'Sent message type Sync' with no progress: Ctrl+C, then retry."
  [void](Read-Host "Ready for $BudLabel / $Port - Enter to start Sync")

  & $BestoolPath @BestoolArgs
  if ($LASTEXITCODE -ne 0) {
    throw "bestool failed on $Port (exit $LASTEXITCODE). Ctrl+C if hung on Sync, then retry with reseat-after-start."
  }
}

$bestoolPath = Resolve-Bestool $Bestool
if (-not $bestoolPath) {
  throw @"
bestool not found. See BESTOOL.md in this package.
Or run Install.ps1 from the unzipped flash folder (bestool.exe ships in the zip).
"@
}

$resolved = Resolve-Ports $Port0 $Port1
$Port0 = $resolved[0]
$Port1 = $resolved[1]

Write-Host ""
Write-Host "PineBuds Pro BiCROS flash" -ForegroundColor Cyan
Write-Host "Firmware: $BinPath"
Write-Host "Bestool:  $bestoolPath"
Write-Host "LEFT:     $Port0"
Write-Host "RIGHT:    $Port1"
Write-Host "Flash runs ONE bud at a time. Do not leave both seated before starting Sync."
Write-Host "Tip: for a guided menu, run .\Install.ps1 instead."
Write-Host ""

Invoke-BestoolWithReseat -BestoolPath $bestoolPath -BestoolArgs @("write-image", $BinPath, "--port", $Port0) -Port $Port0 -BudLabel "LEFT"
Invoke-BestoolWithReseat -BestoolPath $bestoolPath -BestoolArgs @("write-image", $BinPath, "--port", $Port1) -Port $Port1 -BudLabel "RIGHT"

Write-Host ""
Write-Host "Done. Leave both buds in the case ~30-60s for TWS re-pair." -ForegroundColor Green
Write-Host "Then pair an Android phone. Quad-tap toggles BiCROS."
Write-Host "Optional: sideload CROScontrol.apk from this folder for knobs / Help."
