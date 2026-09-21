# Decode flash package from base64 sidecar (workaround for agent binary upload limits)
param(
  [string]$B64Path = ".\pinebuds-cros-LATEST.zip.b64",
  [string]$OutPath = ".\pinebuds-cros-LATEST.zip"
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path $B64Path)) { throw "Missing $B64Path" }
$b64 = Get-Content -Raw -Path $B64Path
$bytes = [Convert]::FromBase64String($b64.Trim())
[IO.File]::WriteAllBytes((Resolve-Path .).Path + "\\" + (Split-Path $OutPath -Leaf), $bytes)
# write next to b64
$full = Join-Path (Split-Path (Resolve-Path $B64Path) -Parent) (Split-Path $OutPath -Leaf)
[IO.File]::WriteAllBytes($full, $bytes)
Write-Host "Wrote $full ($($bytes.Length) bytes)"
Write-Host "Unzip it, then: .\backup.ps1 -Port0 COM3 -Port1 COM4"
