# Turns a checkpoint into <work>\rec_en.onnx with the input and output Lexia expects, then checks it.
#
#   .\training\rec_en\export.ps1                            # output\best_accuracy
#   .\training\rec_en\export.ps1 -Checkpoint iter_epoch_5   # another checkpoint in output\ (score_epochs.ps1 finds it)
#   .\training\rec_en\export.ps1 -Run output_round2 -Checkpoint iter_epoch_5   # a run kept in another folder
#   .\training\rec_en\export.ps1 -Commit 5eae7a3            # name the training commit by hand (an older run)
#
# Steps: Export-RecOnnx (common.ps1: export_model.py, then paddle2onnx), stamp_onnx.py (the Apache-2.0 notice and the
# commit the training ran at), check_onnx.py (shapes, and PaddleOCR's and the new model on the check lines).

param(
    [string]$Run = 'output',
    [string]$Checkpoint = 'best_accuracy',
    [string]$Commit = ''
)
. (Join-Path $PSScriptRoot 'common.ps1')

# The checkpoint and its training_commit.txt are read from the same run folder, so a model is never stamped with
# another run's commit.
$out = Join-Path $Work $Run
if (-not (Test-Path -LiteralPath $out -PathType Container)) { throw "no run folder $out" }
$inf = Join-Path $Work 'inference'
$onnx = Join-Path $Work 'rec_en.onnx'

Export-RecOnnx (Join-Path $out $Checkpoint) $inf $onnx

# The commit the scripts were at when the model was trained, not now: train.ps1 writes it beside the checkpoints.
if (-not $Commit) {
    $stamp = Join-Path $out 'training_commit.txt'
    if (Test-Path -LiteralPath $stamp) {
        $Commit = (Get-Content -LiteralPath $stamp -TotalCount 1).Trim()
    } else {
        throw "no ${stamp} (a run from before train.ps1 wrote it): give the training commit with -Commit"
    }
}
Invoke-Py (Join-Path $Here 'stamp_onnx.py') $onnx --commit $Commit

# Measured against PaddleOCR's own model (setup.ps1 keeps it), not models\, which holds Lexia's earlier fine-tune.
Invoke-Py (Join-Path $Here 'check_onnx.py') --new $onnx --old (Join-Path $Work 'rec_en.original.onnx') --data (Join-Path $Work 'data')
Write-Host "Exported: $onnx  (training commit $Commit)  SHA-256 $((Get-FileHash -LiteralPath $onnx -Algorithm SHA256).Hash)"
Write-Host 'Next: .\training\rec_en\try_model.ps1 (the Fixture score with the new model)'
