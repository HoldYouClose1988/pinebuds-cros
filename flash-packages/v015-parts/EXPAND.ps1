# Rebuild pinebuds-cros-v0.1.5.zip from part-*.b64 fragments
$ErrorActionPreference = "Stop"
$dir = $PSScriptRoot
$parts = Get-ChildItem -Path $dir -Filter "part-*.b64" | Sort-Object Name
if (-not $parts) { throw "No part-*.b64 files found in $dir" }
$b64 = ($parts | ForEach-Object { Get-Content $_.FullName -Raw }) -join ""
$out = Join-Path (Split-Path $dir -Parent) "pinebuds-cros-v0.1.5.zip"
$bytes = [Convert]::FromBase64String($b64)
[IO.File]::WriteAllBytes($out, $bytes)
Write-Host "Wrote $out ($($bytes.Length) bytes)"
$latest = Join-Path (Split-Path $dir -Parent) "pinebuds-cros-LATEST.zip"
Copy-Item $out $latest -Force
Write-Host "Also updated $latest"
