# Backup both buds on Windows via bestool
param(
  [Parameter(Mandatory = $true)]
  [string]$Port0,
  [Parameter(Mandatory = $true)]
  [string]$Port1,
  [string]$OutDir = ".\backups",
  [string]$Bestool = ""
)

$ErrorActionPreference = "Stop"

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
  throw "bestool not found. See BESTOOL.md — build from https://github.com/Ralim/bestool"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMddTHHmmssZ"

$left = Join-Path $OutDir "left-$stamp.bin"
$right = Join-Path $OutDir "right-$stamp.bin"

Write-Host "Bestool: $bestoolPath"
Write-Host "Backing up $Port0 -> $left"
& $bestoolPath read-image $left --port $Port0
Write-Host "Backing up $Port1 -> $right"
& $bestoolPath read-image $right --port $Port1
Write-Host "Saved backups under $OutDir"
