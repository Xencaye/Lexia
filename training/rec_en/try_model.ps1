# Runs the Fixture score (scripts\score_layout.ps1) with a newly trained rec_en.onnx, then puts the old one back.
#
#   .\training\rec_en\try_model.ps1                    # <work>\rec_en.onnx
#   .\training\rec_en\try_model.ps1 -Model <file.onnx>
#   .\training\rec_en\try_model.ps1 -Keep              # leave the new model in models\ (to try Lexia with it)
#
# lexia_cli reads the dev models\ folder, so the model is swapped there for the run. Afterwards the shipped model,
# ocr_models\rec_en.onnx, is put back (as fetch_deps.ps1 would), unless -Keep is given; also when the score fails or
# stops. Putting back the shipped file, not a copy of what was in models\, means a model left there by an earlier
# -Keep is never taken for the real one. To score many checkpoints without touching models\, use score_epochs.ps1.
# Needs a Release build (build\cli\Release\lexia_cli.exe).

param(
    [string]$Model = '',
    [switch]$Keep
)
. (Join-Path $PSScriptRoot 'common.ps1')
if (-not $Model) { $Model = Join-Path $Work 'rec_en.onnx' }
if (-not (Test-Path -LiteralPath $Model)) { throw "no model at ${Model}: run export.ps1" }

$live = Join-Path $Repo 'models\rec_en.onnx'
$shipped = Join-Path $Repo 'ocr_models\rec_en.onnx'

Write-Host '--- score with the current model ---'
& (Join-Path $Repo 'scripts\score_layout.ps1')
Copy-Item -LiteralPath $Model $live -Force
try {
    Write-Host '--- score with the new model ---'
    & (Join-Path $Repo 'scripts\score_layout.ps1')
    $code = $LASTEXITCODE
} finally {
    if (-not $Keep) { Copy-Item -LiteralPath $shipped $live -Force; Write-Host 'models\rec_en.onnx: the shipped model put back' }
}
exit $code
