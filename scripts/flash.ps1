# Flash PineBuds Pro from Windows (bestool)
#
# BES2300 only enters the programmer bootloader if bestool's Sync is ACKed
# during reset. Correct order per bud:
#   1) bud OUT of case (awake / LEDs on)
#   2) start bestool (begins Sync)
#   3) IMMEDIATELY reseat that bud (case reset -> bootloader ACK)
param(
  [string]$BinPath = ".\open_source.bin",
  [Parameter(Mandatory = $true)]
  [string]$Port0,
  [Parameter(Mandatory = $true)]
  [string]$Port1,
  [string]$Bestool = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BinPath)) {
  throw "Firmware image not found: $BinPath"
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
  Write-Host "=== $BudLabel ($Port) ==="
  Write-Host "1. Remove the $BudLabel bud from the case; wait until its LED shows it is awake."
  Write-Host "2. Press Enter here - bestool will open $Port and start Sync."
  Write-Host "3. IMMEDIATELY reseat that bud (case contact = reset). Sync must catch the boot."
  Write-Host "   If it sits on 'Sent message type Sync' with no progress: Ctrl+C, then retry this port."
  [void](Read-Host "Ready for $BudLabel / $Port")

  & $BestoolPath @BestoolArgs
  if ($LASTEXITCODE -ne 0) {
    throw "bestool failed on $Port (exit $LASTEXITCODE). Ctrl+C if hung on Sync, then retry with reseat-after-start."
  }
}

$bestoolPath = Resolve-Bestool $Bestool
if (-not $bestoolPath) {
  throw @"
bestool not found. See BESTOOL.md in this package.
Build from https://github.com/Ralim/bestool then either:
  - copy bestool.exe into this folder, or
  - pass -Bestool path\to\bestool.exe, or
  - put bestool.exe on PATH.
"@
}

Write-Host "Firmware: $BinPath"
Write-Host "Bestool:  $bestoolPath"
Write-Host "Ports:    $Port0 , $Port1"
Write-Host "Flash runs ONE bud at a time. Do not leave both seated before starting Sync."

Invoke-BestoolWithReseat -BestoolPath $bestoolPath -BestoolArgs @("write-image", $BinPath, "--port", $Port0) -Port $Port0 -BudLabel "LEFT"
Invoke-BestoolWithReseat -BestoolPath $bestoolPath -BestoolArgs @("write-image", $BinPath, "--port", $Port1) -Port $Port1 -BudLabel "RIGHT"

Write-Host "Done. Leave both buds in the case ~30s for TWS re-pair."
