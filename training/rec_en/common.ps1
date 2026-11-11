# Shared by the training scripts (dot-source it): the work folder, the venv's Python, and the CUDA 11.8 / cuDNN 8.9
# DLLs that pip put in the venv on PATH (PaddlePaddle 2.6.2 for Windows loads them by name from PATH).
# The work folder is outside the repo and outside OneDrive: 400 000 images must not be synced or committed.

param()
$ErrorActionPreference = 'Stop'
if (-not $env:LEXIA_TRAIN_DIR) { $env:LEXIA_TRAIN_DIR = Join-Path $env:USERPROFILE 'LexiaTrain' }
$Work = $env:LEXIA_TRAIN_DIR
$Here = $PSScriptRoot
$Repo = Split-Path -Parent (Split-Path -Parent $Here)
$Py = Join-Path $Work 'venv\Scripts\python.exe'
$PaddleOcr = Join-Path $Work 'PaddleOCR'

$nvidia = Join-Path $Work 'venv\Lib\site-packages\nvidia'
if (Test-Path -LiteralPath $nvidia) {
    $bins = Get-ChildItem -LiteralPath $nvidia -Directory | ForEach-Object { Join-Path $_.FullName 'bin' } |
        Where-Object { Test-Path -LiteralPath $_ }
    $env:PATH = ($bins -join ';') + ';' + $env:PATH
}

# PaddleOCR reads its -o values as YAML: forward slashes keep the Windows paths plain.
function ToYamlPath([string]$p) { return $p.Replace('\', '/') }

# Exports one training checkpoint to ONNX: PaddleOCR's export_model.py (an inference model with the CTC head only;
# the SAR head is for training), then paddle2onnx (opset 14: hard_swish needs it; ONNX Runtime 1.20 runs it; input
# x [N, 3, 48, W]). $Checkpoint is the path without the .pdparams extension. The ONNX file is written as
# "$Onnx.part" and renamed only when paddle2onnx succeeded, so an interrupted export never leaves a file that looks
# finished (score_epochs.ps1 reuses finished ones).
function Export-RecOnnx([string]$Checkpoint, [string]$InferenceDir, [string]$Onnx) {
    # PaddleOCR's tools import ppocr from the repo root, so run from there. The output is kept and shown only on a
    # failure (Invoke-Py throws when Python exits non-zero; the lines it printed are in $exportLog by then).
    Push-Location $PaddleOcr
    $exportLog = @()
    try {
        Invoke-Py tools/export_model.py -c (Join-Path $Here 'lexia_rec_en.yml') -o `
            "Global.pretrained_model=$(ToYamlPath $Checkpoint)" `
            "Global.save_inference_dir=$(ToYamlPath $InferenceDir)" `
            "Global.character_dict_path=$(ToYamlPath (Join-Path $Here 'dict_en.txt'))" |
            ForEach-Object { $exportLog += $_ }
    } catch {
        $exportLog | Select-Object -Last 25 | ForEach-Object { Write-Host $_ }
        throw
    } finally {
        Pop-Location
    }
    $part = "$Onnx.part"
    $ErrorActionPreference = 'Continue'   # paddle2onnx logs to stderr; the exit code decides
    # Kept, not printed: shown only when the conversion fails, so the reason is not lost.
    $log = & (Join-Path $Work 'venv\Scripts\paddle2onnx.exe') --model_dir $InferenceDir --model_filename inference.pdmodel `
        --params_filename inference.pdiparams --save_file $part --opset_version 14 --enable_onnx_checker True 2>&1 |
        ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) {
        $log | Select-Object -Last 25 | ForEach-Object { Write-Host $_ }
        Remove-Item -LiteralPath $part -ErrorAction SilentlyContinue
        throw "paddle2onnx failed for $Checkpoint (exit $code; its last lines are above)"
    }
    Move-Item -LiteralPath $part $Onnx -Force
}

# Runs the venv's Python in isolated mode (-I): nothing from the current folder or the user's site is imported.
# Paddle logs to stderr; in Windows PowerShell 5.1 with 'Stop' each stderr line would end the script, so only the
# exit code decides here.
function Invoke-Py {
    $ErrorActionPreference = 'Continue'
    & $Py -I @args 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "python failed ($LASTEXITCODE): $args" }
}
