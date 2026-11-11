# Exports every chosen epoch of a training run and scores each on the Fixtures, so the epoch to ship is chosen on
# real screens: the epoch with the best accuracy on the synthetic check lines is often not the best there (task L:
# round 2's best_accuracy was epoch 9, its best on the Fixtures epoch 5).
#
#   .\training\rec_en\score_epochs.ps1                          # output\, epochs 2-10
#   .\training\rec_en\score_epochs.ps1 -Run output_round2 -Epochs 4,5,6
#
# Prints one line per epoch: the average distance and Block-count error, and the Fixtures worse than the baseline.
# Then: .\training\rec_en\export.ps1 -Checkpoint iter_epoch_<n> for the chosen one.
#
# models\ is not touched: lexia_cli and its DLLs are copied to <work>\score_cli\ with a models\ folder of its own
# (lexia_cli reads the models beside itself first). Needs a Release build (build\cli\Release\lexia_cli.exe).

param(
    [string]$Run = 'output',
    [int[]]$Epochs = @(2, 3, 4, 5, 6, 7, 8, 9, 10)
)
. (Join-Path $PSScriptRoot 'common.ps1')

$runDir = Join-Path $Work $Run
if (-not (Test-Path -LiteralPath $runDir -PathType Container)) { throw "no run folder $runDir" }
$cliSrc = Join-Path $Repo 'build\cli\Release'
if (-not (Test-Path -LiteralPath (Join-Path $cliSrc 'lexia_cli.exe'))) { throw "no lexia_cli in ${cliSrc}: build Release first" }

# A private copy of lexia_cli with the dev models (det.onnx, the dictionaries); rec_en.onnx is swapped per epoch.
$cli = Join-Path $Work 'score_cli'
New-Item -ItemType Directory (Join-Path $cli 'models') -Force | Out-Null
Copy-Item -Path (Join-Path $cliSrc '*') -Destination $cli -Force
Copy-Item -Path (Join-Path $Repo 'models\*') -Destination (Join-Path $cli 'models') -Force
$scores = Join-Path $Work "scores_$Run"
New-Item -ItemType Directory $scores -Force | Out-Null

foreach ($e in $Epochs) {
    $ckpt = Join-Path $runDir "iter_epoch_$e"
    if (-not (Test-Path -LiteralPath "$ckpt.pdparams")) { Write-Host "epoch ${e}: no checkpoint"; continue }
    $inf = Join-Path $scores "inference_e$e"
    # The export is named after its checkpoint's write time and size: a later training run into the same folder
    # writes new checkpoints, so it never reuses an earlier run's export. Export-RecOnnx writes the file only once
    # it is complete, so a file with this name is a finished export of exactly this checkpoint.
    $src = Get-Item -LiteralPath "$ckpt.pdparams"
    $onnx = Join-Path $scores "rec_en_e${e}_$($src.LastWriteTimeUtc.Ticks)_$($src.Length).onnx"
    if (-not (Test-Path -LiteralPath $onnx)) { Export-RecOnnx $ckpt $inf $onnx }
    Copy-Item -LiteralPath $onnx (Join-Path $cli 'models\rec_en.onnx') -Force
    # score_layout.ps1 exits 0 (nothing worse) or 1 (a Fixture got worse): both are results. Anything else (2: the
    # CLI failed, for example on a broken model) means no score, and must not read as "worse: none".
    $ErrorActionPreference = 'Continue'
    $text = & (Join-Path $Repo 'scripts\score_layout.ps1') -Cli (Join-Path $cli 'lexia_cli.exe') *>&1 | Out-String -Width 250
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0 -and $code -ne 1) { Write-Host "epoch ${e}: NO SCORE (score_layout.ps1 exit $code)"; continue }
    $avg = ($text -split "`n" | Where-Object { $_ -match '^Average' }) -join ''
    # Case-sensitive (-cmatch): the table says WORSE; the summary lines ("OK: no Fixture is worse ...", "FAILED: ...
    # worse than ...") must not count. PowerShell's -match ignores case.
    $worse = ($text -split "`n" | Where-Object { $_ -cmatch '\bWORSE\b' -and $_ -notmatch '^(OK|FAILED)' } |
        ForEach-Object { ($_.Trim() -split '\s+')[0] }) -join ', '
    if (-not $worse) { $worse = 'none' }
    Write-Host "epoch ${e}: $($avg.Trim())  worse: $worse"
}
exit 0   # the last score's exit code is a result printed above, not this script's
