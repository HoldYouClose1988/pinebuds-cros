# PineBuds Pro - BiCROS firmware + NV dumper (Windows)
#
# Dumps both buds via bestool, hashes them vs open_source.bin (if present),
# and scans for BiCROS NV knob blobs (magic 0xC7).
#
#   powershell -ExecutionPolicy Bypass -File .\Dump.ps1
#   powershell -ExecutionPolicy Bypass -File .\Dump.ps1 -AnalyzeOnly -LeftBin .\backups\left.bin -RightBin .\backups\right.bin
#
# Zip the dump folder and send REPORT.txt + both .bin files for diagnosis.
param(
  [string]$Port0 = "",
  [string]$Port1 = "",
  [string]$Bestool = "",
  [string]$OutDir = "",
  [string]$RefBin = ".\open_source.bin",
  [switch]$AnalyzeOnly,
  [string]$LeftBin = "",
  [string]$RightBin = ""
)

$ErrorActionPreference = "Stop"
$Here = $PSScriptRoot
if (-not $Here) { $Here = (Get-Location).Path }
Set-Location $Here

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
  if (-not $A -or -not $B) { throw "Need two COM ports." }
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
  Write-Host "=== Dump $BudLabel ($Port) ===" -ForegroundColor Cyan
  Write-Host "Sync order (required or bestool hangs):"
  Write-Host "  1. Remove the $BudLabel bud from the case; wait until LED is awake."
  Write-Host "  2. Press Enter here - bestool opens $Port and starts Sync."
  Write-Host "  3. IMMEDIATELY reseat that bud (case reset catches Sync)."
  Write-Host "     Hang on 'Sent message type Sync': Ctrl+C, then retry."
  [void](Read-Host "Ready for $BudLabel / $Port - Enter to start Sync")
  & $BestoolPath @BestoolArgs
  if ($LASTEXITCODE -ne 0) {
    throw "bestool failed on $Port (exit $LASTEXITCODE)."
  }
}

function Get-FileSha256([string]$Path) {
  return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function Format-NvCandidate {
  param([byte[]]$Bytes, [int]$Offset)
  # Layout: [0]=0xC7 magic, [1]=poor_is_right, [2]=mix int8, [3]=bass,
  # [4]=treble, [5]=sco/vol 0..15, [6]=a2dp 0..15, [7]=noise 0..5
  $poor = $Bytes[$Offset + 1]
  $mix = [int]([sbyte]$Bytes[$Offset + 2])
  $bass = [int]([sbyte]$Bytes[$Offset + 3])
  $treble = [int]([sbyte]$Bytes[$Offset + 4])
  $sco = [int]$Bytes[$Offset + 5]
  $a2dp = [int]$Bytes[$Offset + 6]
  $noise = [int]$Bytes[$Offset + 7]
  $hex = ($Bytes[$Offset..($Offset + 7)] | ForEach-Object { '{0:X2}' -f $_ }) -join ' '
  $poorSide = if ($poor -eq 1) { 'RIGHT' } else { 'LEFT' }
  return [pscustomobject]@{
    Offset = ('0x{0:X8}' -f $Offset)
    Hex    = $hex
    Poor   = $poorSide
    MixDb  = $mix
    BassDb = $bass
    TrebleDb = $treble
    Sco    = $sco
    A2dp   = $a2dp
    Noise  = $noise
    Score  = 0
  }
}

function Test-NvPlausible($Cand) {
  # Score how likely this 8-byte window is a real BiCROS NV blob.
  $score = 0
  if ($Cand.Poor -in @('LEFT', 'RIGHT')) { $score += 2 } else { return -1 }
  # mix stored as signed; product range -30..-12
  if ($Cand.MixDb -ge -30 -and $Cand.MixDb -le -12 -and (($Cand.MixDb % 2) -eq 0)) {
    $score += 5
  } elseif ($Cand.MixDb -ge -30 -and $Cand.MixDb -le 0) {
    $score += 1
  } else {
    return -1
  }
  if ($Cand.BassDb -ge -6 -and $Cand.BassDb -le 6) { $score += 2 } else { return -1 }
  if ($Cand.TrebleDb -ge -6 -and $Cand.TrebleDb -le 6) { $score += 2 } else { return -1 }
  if ($Cand.Sco -ge 0 -and $Cand.Sco -le 15) { $score += 2 } else { return -1 }
  if ($Cand.A2dp -ge 0 -and $Cand.A2dp -le 15) { $score += 1 } else { return -1 }
  if ($Cand.Noise -ge 0 -and $Cand.Noise -le 5) { $score += 3 } else { return -1 }
  # Prefer defaults-ish / known ear values
  if ($Cand.Sco -eq 8) { $score += 1 }
  if ($Cand.Noise -eq 3) { $score += 1 }
  if ($Cand.MixDb -eq -20) { $score += 1 }
  return $score
}

function Find-CrosNvBlobs([string]$BinPath) {
  $bytes = [System.IO.File]::ReadAllBytes($BinPath)
  $hits = New-Object System.Collections.Generic.List[object]
  for ($i = 0; $i -le ($bytes.Length - 8); $i++) {
    if ($bytes[$i] -ne 0xC7) { continue }
    $cand = Format-NvCandidate -Bytes $bytes -Offset $i
    $score = Test-NvPlausible $cand
    if ($score -lt 0) { continue }
    $cand.Score = $score
    $hits.Add($cand)
  }
  return @($hits | Sort-Object Score -Descending)
}

function Write-BinReport {
  param(
    [string]$Label,
    [string]$BinPath,
    [string]$RefSha,
    [System.Text.StringBuilder]$Report
  )
  if (-not (Test-Path $BinPath)) {
    [void]$Report.AppendLine("### $Label - MISSING $BinPath")
    [void]$Report.AppendLine("")
    return
  }
  $fi = Get-Item $BinPath
  $sha = Get-FileSha256 $BinPath
  [void]$Report.AppendLine("### $Label")
  [void]$Report.AppendLine("file:     $($fi.FullName)")
  [void]$Report.AppendLine("bytes:    $($fi.Length)")
  [void]$Report.AppendLine("sha256:   $sha")
  if ($RefSha) {
    if ($sha -eq $RefSha) {
      [void]$Report.AppendLine("vs_ref:   MATCH open_source.bin (same firmware image bytes)")
    } else {
      [void]$Report.AppendLine("vs_ref:   DIFFERENT from open_source.bin")
      [void]$Report.AppendLine("ref_sha:  $RefSha")
    }
  }
  [void]$Report.AppendLine("")
  [void]$Report.AppendLine("BiCROS NV candidates (magic 0xC7 + plausible knobs):")
  $hits = Find-CrosNvBlobs $BinPath
  if (-not $hits -or $hits.Count -eq 0) {
    [void]$Report.AppendLine("  (none found - NV never Applied, or layout not in this dump)")
  } else {
    $best = $hits | Select-Object -First 5
    foreach ($h in $best) {
      [void]$Report.AppendLine(
        ("  score={0} off={1} hex=[{2}] poor={3} mix={4}dB bass={5} treble={6} sco={7}/15 a2dp={8} noise={9}/5" -f `
          $h.Score, $h.Offset, $h.Hex, $h.Poor, $h.MixDb, $h.BassDb, $h.TrebleDb, $h.Sco, $h.A2dp, $h.Noise)
      )
    }
    if ($hits.Count -gt 5) {
      [void]$Report.AppendLine("  ... +$($hits.Count - 5) more lower-score hits (see nv-hits-$Label.txt)")
    }
    $hitFile = Join-Path (Split-Path $BinPath -Parent) ("nv-hits-{0}.txt" -f $Label.ToLower())
    $hits | ForEach-Object {
      "score=$($_.Score) offset=$($_.Offset) hex=$($_.Hex) poor=$($_.Poor) mix=$($_.MixDb) bass=$($_.BassDb) treble=$($_.TrebleDb) sco=$($_.Sco) a2dp=$($_.A2dp) noise=$($_.Noise)"
    } | Set-Content -Path $hitFile -Encoding ASCII
  }
  [void]$Report.AppendLine("")
}

# --- main ---
Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  PineBuds Pro - BiCROS dump (firmware + NV knobs)" -ForegroundColor Cyan
Write-Host "  DIY / own-risk - NOT a hearing aid" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if (-not $OutDir) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $OutDir = Join-Path $Here ("dumps\dump-$stamp")
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

$leftPath = Join-Path $OutDir "left.bin"
$rightPath = Join-Path $OutDir "right.bin"
$reportPath = Join-Path $OutDir "REPORT.txt"

if (-not $AnalyzeOnly) {
  $bestoolPath = Resolve-Bestool $Bestool
  if (-not $bestoolPath) {
    throw "bestool.exe not found. Put it next to Dump.ps1 (flash zip includes it)."
  }
  $ports = Resolve-Ports $Port0 $Port1
  $Port0 = $ports[0]
  $Port1 = $ports[1]
  Write-Host "Bestool: $bestoolPath"
  Write-Host "LEFT:    $Port0 -> $leftPath"
  Write-Host "RIGHT:   $Port1 -> $rightPath"
  Write-Host "OutDir:  $OutDir"
  Write-Host ""
  Write-Host "This reads the FULL flash image from each bud (same Sync dance as backup)."
  Write-Host ""

  Invoke-BestoolWithReseat -BestoolPath $bestoolPath `
    -BestoolArgs @("read-image", $leftPath, "--port", $Port0) `
    -Port $Port0 -BudLabel "LEFT"
  Write-Host "Saved $leftPath" -ForegroundColor Green

  Invoke-BestoolWithReseat -BestoolPath $bestoolPath `
    -BestoolArgs @("read-image", $rightPath, "--port", $Port1) `
    -Port $Port1 -BudLabel "RIGHT"
  Write-Host "Saved $rightPath" -ForegroundColor Green
} else {
  if (-not $LeftBin -or -not $RightBin) {
    throw "AnalyzeOnly requires -LeftBin and -RightBin paths."
  }
  Copy-Item -Force $LeftBin $leftPath
  Copy-Item -Force $RightBin $rightPath
  Write-Host "Analyze-only: copied bins into $OutDir"
}

$refSha = $null
$refNote = "(no open_source.bin in this folder)"
if (Test-Path $RefBin) {
  $refSha = Get-FileSha256 $RefBin
  $refNote = "open_source.bin sha256=$refSha"
  Copy-Item -Force $RefBin (Join-Path $OutDir "open_source.ref.bin")
}

# Known product bin (v0.3.65 / v0.3.66 package)
$known065 = "f019e7b183a3b4490ecdc08fe15e4bbfe774d6e8bafecf3e7c0fe8b70abe9971"

$report = New-Object System.Text.StringBuilder
[void]$report.AppendLine("PineBuds Pro BiCROS dump report")
[void]$report.AppendLine("created_utc: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))")
[void]$report.AppendLine("host:        $env:COMPUTERNAME")
[void]$report.AppendLine("outdir:      $OutDir")
[void]$report.AppendLine("ref:         $refNote")
[void]$report.AppendLine("known_0.3.65_sha256: $known065")
[void]$report.AppendLine("")
[void]$report.AppendLine("NV layout (if magic 0xC7 present):")
[void]$report.AppendLine("  [0]=magic C7  [1]=poor_is_right  [2]=mix_db  [3]=bass  [4]=treble  [5]=sco  [6]=a2dp  [7]=noise")
[void]$report.AppendLine("Ear-preferred knobs: sco=8  noise=3  mix=-20")
[void]$report.AppendLine("Loud+hiss often: sco>=11 and/or noise=0 (phone AbsVol rocker while BiCROS on saves sco into NV).")
[void]$report.AppendLine("")

Write-BinReport -Label "LEFT" -BinPath $leftPath -RefSha $refSha -Report $report
Write-BinReport -Label "RIGHT" -BinPath $rightPath -RefSha $refSha -Report $report

# Cross-bud compare
if ((Test-Path $leftPath) -and (Test-Path $rightPath)) {
  $ls = Get-FileSha256 $leftPath
  $rs = Get-FileSha256 $rightPath
  [void]$report.AppendLine("### Cross-check")
  if ($ls -eq $rs) {
    [void]$report.AppendLine("left_vs_right: IDENTICAL image bytes (unusual for full dumps with unique BT addrs - verify read)")
  } else {
    [void]$report.AppendLine("left_vs_right: different (expected - unique calib/BT/NV)")
  }
  if ($known065) {
    [void]$report.AppendLine("left_vs_0.3.65_ref:  $(if ($ls -eq $known065) { 'MATCH' } else { 'DIFF (normal - dump includes NV/calib, not only APP)' })")
    [void]$report.AppendLine("right_vs_0.3.65_ref: $(if ($rs -eq $known065) { 'MATCH' } else { 'DIFF (normal - dump includes NV/calib, not only APP)' })")
  }
  [void]$report.AppendLine("")
  [void]$report.AppendLine("Note: full read-image dumps are NOT expected to equal open_source.bin SHA.")
  [void]$report.AppendLine("Compare NV sco/noise/mix lines above. Also paste CROS Control status banner after Connect/Get.")
}

[System.IO.File]::WriteAllText($reportPath, $report.ToString(), [System.Text.UTF8Encoding]::new($false))

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "Report: $reportPath"
Write-Host ""
Get-Content $reportPath | Write-Host
Write-Host ""
Write-Host "Zip this folder and send it:" -ForegroundColor Yellow
Write-Host "  $OutDir"
Write-Host "Include REPORT.txt + left.bin + right.bin (+ nv-hits-*.txt)."
