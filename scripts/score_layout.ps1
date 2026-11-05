<#
.SYNOPSIS
  Scores Lexia's grouping and Reading order against the Fixtures, and checks it did not get worse.

.DESCRIPTION
  Runs lexia_cli with --score on every tests\fixtures\<name>\ (screen.png + expected.txt),
  prints one row per Fixture and the average, and compares each row with the committed baseline
  (tests\fixtures\baseline.json). Exit code 0 = nothing got worse, 1 = at least one Fixture got
  worse, or a baseline entry is missing, 2 = the script could not run (no lexia_cli, a Fixture
  without both files, lexia_cli failed).

  Columns:
    Dist    normalised edit distance of the joined Block text, 0 = identical, 1 = nothing in common
            (code points; see src\score.h)
    BlkErr  |produced Blocks - expected Blocks|
    Exp/Got the expected and the produced number of Blocks
    Base    the baseline Dist, with the baseline BlkErr in brackets

  A Fixture is WORSE when its Dist is more than -Tolerance above the baseline, or when its BlkErr
  is above the baseline at all (a count: one extra Block is a real change, not noise).
  A Fixture that is not in the baseline is reported as NEW and does not fail; run -SaveBaseline
  to add it. A baseline entry whose Fixture folder is gone is MISSING and fails, so a Fixture is
  never dropped unnoticed.

  Run it before and after any change to grouping or Reading order, and report both (CLAUDE.md).

.PARAMETER Cli
  The lexia_cli.exe to use. Default: build\cli\Release\lexia_cli.exe. For an agent tree:
  -Cli build\agent\cli\Release\lexia_cli.exe

.PARAMETER Tolerance
  How much a Fixture's Dist may rise above its baseline before the run fails. Default 0.02
  (two percentage points of the text). Raise it only on purpose, and say why in the PR.

.PARAMETER SaveBaseline
  Write the current scores to the baseline file instead of comparing. Commit the result, and say
  in the PR why the baseline moved.

.PARAMETER Baseline
  The baseline file. Default: tests\fixtures\baseline.json

.EXAMPLE
  .\scripts\score_layout.ps1
  .\scripts\score_layout.ps1 -Cli build\agent\cli\Release\lexia_cli.exe
  .\scripts\score_layout.ps1 -SaveBaseline
#>
[CmdletBinding()]
param(
    [string]$Cli = '',
    [double]$Tolerance = 0.02,
    [switch]$SaveBaseline,
    [string]$Baseline = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$fixturesDir = Join-Path $root 'tests\fixtures'
if (-not $Cli) { $Cli = Join-Path $root 'build\cli\Release\lexia_cli.exe' }
if (-not $Baseline) { $Baseline = Join-Path $fixturesDir 'baseline.json' }
$inv = [System.Globalization.CultureInfo]::InvariantCulture

function Fail([string]$msg) {
    [Console]::Error.WriteLine("score_layout: $msg")
    exit 2
}

if (-not (Test-Path -LiteralPath $Cli)) {
    Fail "lexia_cli not found at '$Cli'. Build it (cmake --build build --config Release --target lexia_cli) or pass -Cli."
}
$Cli = (Resolve-Path -LiteralPath $Cli).Path

$dirs = @(Get-ChildItem -LiteralPath $fixturesDir -Directory | Sort-Object Name)
if ($dirs.Count -eq 0) { Fail "no Fixtures in $fixturesDir" }

# ---- run every Fixture -------------------------------------------------------------------
$rows = @()
foreach ($d in $dirs) {
    $png = Join-Path $d.FullName 'screen.png'
    $exp = Join-Path $d.FullName 'expected.txt'
    if (-not (Test-Path -LiteralPath $png) -or -not (Test-Path -LiteralPath $exp)) {
        Fail "Fixture '$($d.Name)' needs both screen.png and expected.txt"
    }
    $out = & $Cli $png --score $exp | Out-String
    if ($LASTEXITCODE -ne 0) { Fail "lexia_cli failed on '$($d.Name)' (exit $LASTEXITCODE)" }
    $s = $out | ConvertFrom-Json
    $rows += [pscustomobject]@{
        Name = $d.Name
        Dist = [double]$s.distance
        BlkErr = [int]$s.blockCountError
        Exp = [int]$s.expectedBlocks
        Got = [int]$s.producedBlocks
    }
}

# ---- save the baseline -------------------------------------------------------------------
if ($SaveBaseline) {
    # Written by hand: ConvertTo-Json in Windows PowerShell 5.1 indents badly, and a plain
    # layout keeps the baseline's diffs readable.
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("{`n  `"fixtures`": {`n")
    for ($i = 0; $i -lt $rows.Count; $i++) {
        $r = $rows[$i]
        $comma = if ($i -lt $rows.Count - 1) { ',' } else { '' }
        $name = $r.Name.Replace('\', '\\').Replace('"', '\"')
        [void]$sb.Append(('    "{0}": {{ "distance": {1}, "blockCountError": {2} }}{3}' -f $name, ([math]::Round($r.Dist, 6)).ToString('0.######', $inv), $r.BlkErr, $comma) + "`n")
    }
    [void]$sb.Append("  }`n}`n")
    [System.IO.File]::WriteAllText($Baseline, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
    "Saved the baseline for $($rows.Count) Fixture(s) to $Baseline"
}

# ---- compare with the baseline -----------------------------------------------------------
$base = $null
if (-not $SaveBaseline) {
    if (-not (Test-Path -LiteralPath $Baseline)) {
        Fail "no baseline at '$Baseline'. Run with -SaveBaseline first."
    }
    $base = (Get-Content -LiteralPath $Baseline -Raw -Encoding UTF8) | ConvertFrom-Json
}

$worse = 0
$table = @()
foreach ($r in $rows) {
    $status = ''
    $baseText = ''
    if (-not $SaveBaseline) {
        $b = $base.fixtures.PSObject.Properties[$r.Name]
        if ($null -eq $b) {
            $status = 'NEW'
        } else {
            $bd = [double]$b.Value.distance
            $be = [int]$b.Value.blockCountError
            $baseText = '{0} ({1})' -f $bd.ToString('0.0000', $inv), $be
            if ($r.Dist -gt ($bd + $Tolerance + 1e-9) -or $r.BlkErr -gt $be) {
                $status = 'WORSE'
                $worse++
            } elseif ($r.Dist -lt ($bd - 1e-9) -or $r.BlkErr -lt $be) {
                $status = 'better'
            } else {
                $status = 'ok'
            }
        }
    }
    $table += [pscustomobject]@{
        Fixture = $r.Name
        Dist = $r.Dist.ToString('0.0000', $inv)
        BlkErr = $r.BlkErr
        Exp = $r.Exp
        Got = $r.Got
        Base = $baseText
        Status = $status
    }
}

$missing = @()
if (-not $SaveBaseline) {
    foreach ($p in $base.fixtures.PSObject.Properties) {
        if (-not ($rows | Where-Object { $_.Name -eq $p.Name })) { $missing += $p.Name }
    }
}

$cols = @('Fixture', 'Dist', 'BlkErr', 'Exp', 'Got')
if (-not $SaveBaseline) { $cols += @('Base', 'Status') }
($table | Format-Table -Property $cols -AutoSize | Out-String).TrimEnd()

$avgDist = ($rows | Measure-Object -Property Dist -Average).Average
$avgErr = ($rows | Measure-Object -Property BlkErr -Average).Average
''
'Average: Dist {0}, BlkErr {1} over {2} Fixture(s)' -f $avgDist.ToString('0.0000', $inv), $avgErr.ToString('0.00', $inv), $rows.Count

if ($SaveBaseline) { exit 0 }

foreach ($m in $missing) { "MISSING: '$m' is in the baseline but has no Fixture folder (re-run with -SaveBaseline if it was removed on purpose)" }
if ($worse -gt 0 -or $missing.Count -gt 0) {
    "FAILED: $worse Fixture(s) worse than the baseline (Tolerance $($Tolerance.ToString('0.####', $inv)) on Dist, none on BlkErr), $($missing.Count) missing"
    exit 1
}
"OK: no Fixture is worse than the baseline (Tolerance $($Tolerance.ToString('0.####', $inv)) on Dist, none on BlkErr)"
exit 0
