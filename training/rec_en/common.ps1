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

# Runs the venv's Python in isolated mode (-I): nothing from the current folder or the user's site is imported.
# Paddle logs to stderr; in Windows PowerShell 5.1 with 'Stop' each stderr line would end the script, so only the
# exit code decides here.
function Invoke-Py {
    $ErrorActionPreference = 'Continue'
    & $Py -I @args 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "python failed ($LASTEXITCODE): $args" }
}
