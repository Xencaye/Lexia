# Runs the Fixture score (scripts\score_layout.ps1) with a newly trained rec_en.onnx, then puts the old one back.
#
#   .\training\rec_en\try_model.ps1                    # <work>\rec_en.onnx
#   .\training\rec_en\try_model.ps1 -Model <file.onnx>
#   .\training\rec_en\try_model.ps1 -Keep              # leave the new model in models\ (to try Lexia with it)
#
# lexia_cli reads the dev models\ folder, so the model is swapped there for the run. The model that was in models\
# is kept as <work>\rec_en.live.onnx and is always put back unless -Keep is given (also when the score fails or
# stops). To score many checkpoints without touching models\, use score_epochs.ps1.
# Needs a Release build (build\cli\Release\lexia_cli.exe).

param(
    [string]$Model = '',
    [switch]$Keep
)
. (Join-Path $PSScriptRoot 'common.ps1')
if (-not $Model) { $Model = Join-Path $Work 'rec_en.onnx' }
if (-not (Test-Path -LiteralPath $Model)) { throw "no model at ${Model}: run export.ps1" }

$live = Join-Path $Repo 'models\rec_en.onnx'
$saved = Join-Path $Work 'rec_en.live.onnx'
Copy-Item -LiteralPath $live $saved -Force

Write-Host '--- score with the current model ---'
& (Join-Path $Repo 'scripts\score_layout.ps1')
Copy-Item -LiteralPath $Model $live -Force
try {
    Write-Host '--- score with the new model ---'
    & (Join-Path $Repo 'scripts\score_layout.ps1')
    $code = $LASTEXITCODE
} finally {
    if (-not $Keep) { Copy-Item -LiteralPath $saved $live -Force; Write-Host 'models\rec_en.onnx put back' }
}
exit $code
