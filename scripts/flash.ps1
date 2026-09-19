# Flash PineBuds Pro from Windows (bestool)
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
Write-Host "Seat both buds, wake them (remove ~3s / reseat, or long-hold rear button), then press Enter."
[void](Read-Host)

& $bestoolPath write-image $BinPath --port $Port0
if ($LASTEXITCODE -ne 0) { throw "Flash failed on $Port0" }

& $bestoolPath write-image $BinPath --port $Port1
if ($LASTEXITCODE -ne 0) { throw "Flash failed on $Port1" }

Write-Host "Done. Leave buds in case ~30s for TWS re-pair."
