# Turns the best checkpoint into <work>\rec_en.onnx with the input and output Lexia expects, then checks it.
#
#   .\training\rec_en\export.ps1                         # output\best_accuracy
#   .\training\rec_en\export.ps1 -Checkpoint latest      # another checkpoint in output\
#
# Steps: PaddleOCR's export_model.py (an inference model with the CTC head only; the SAR head is for training),
# paddle2onnx (opset 14: hard_swish needs it; ONNX Runtime 1.20 runs it; input x [N, 3, 48, W]), check_onnx.py (shapes, and the old and new model on the check lines).

param([string]$Checkpoint = 'best_accuracy')
. (Join-Path $PSScriptRoot 'common.ps1')

function Yp([string]$p) { return $p.Replace('\', '/') }
$out = Join-Path $Work 'output'
$inf = Join-Path $Work 'inference'
$onnx = Join-Path $Work 'rec_en.onnx'

Push-Location $PaddleOcr
try {
    Invoke-Py tools/export_model.py -c (Join-Path $Here 'lexia_rec_en.yml') -o `
        "Global.pretrained_model=$(Yp (Join-Path $out $Checkpoint))" `
        "Global.save_inference_dir=$(Yp $inf)" `
        "Global.character_dict_path=$(Yp (Join-Path $Here 'dict_en.txt'))"
} finally {
    Pop-Location
}

$ErrorActionPreference = 'Continue'   # paddle2onnx logs to stderr; the exit code decides
& (Join-Path $Work 'venv\Scripts\paddle2onnx.exe') --model_dir $inf --model_filename inference.pdmodel `
    --params_filename inference.pdiparams --save_file $onnx --opset_version 14 --enable_onnx_checker True 2>&1 |
    ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { throw 'paddle2onnx failed' }
$ErrorActionPreference = 'Stop'

Invoke-Py (Join-Path $Here 'check_onnx.py') --new $onnx --old (Join-Path $Repo 'models\rec_en.onnx') --data (Join-Path $Work 'data')
Write-Host "Exported: $onnx  SHA-256 $((Get-FileHash -LiteralPath $onnx -Algorithm SHA256).Hash)"
Write-Host 'Next: .\training\rec_en\try_model.ps1 (the Fixture score with the new model)'
