# Makes the training work folder (default %USERPROFILE%\LexiaTrain, or $env:LEXIA_TRAIN_DIR): the Python 3.12 venv
# with requirements.txt, PaddleOCR at the pinned commit, PaddleOCR's pretrained en_PP-OCRv3_rec training checkpoint
# (checked by SHA-256), then the fonts and texts (fetch_sources.py) and the training lines (gen_lines.py).
# Safe to run again: each step that is already done is skipped.
#
#   .\training\rec_en\setup.ps1                 # everything, 400 000 training lines
#   .\training\rec_en\setup.ps1 -TrainLines 2000   # a small trial
#
# Network use: pip (PyPI), github.com (PaddleOCR, fonts), paddleocr.bj.bcebos.com (the checkpoint), gutenberg.org
# (texts), huggingface.co (PaddleOCR's rec_en.onnx, the model a new one is compared with). Only when this script is run; Lexia itself is not involved (ADR-0005).

param(
    [int]$TrainLines = 400000,
    [switch]$SkipData
)
. (Join-Path $PSScriptRoot 'common.ps1')

$paddleOcrCommit = '07603421c20a96bb94bb87d0c4211032527ae706'   # tag v2.9.1
$ckptUrl = 'https://paddleocr.bj.bcebos.com/PP-OCRv3/english/en_PP-OCRv3_rec_train.tar'
$ckptSha = '0740DCBE5670860DED93EB9CB071D7B5ECBF56B742FA8215A48A284A295E1A12'

New-Item -ItemType Directory $Work -Force | Out-Null

# --- venv ---
if (-not (Test-Path -LiteralPath $Py)) {
    Write-Host 'venv: creating (Python 3.12)'
    & py -3.12 -m venv (Join-Path $Work 'venv')
    if ($LASTEXITCODE -ne 0) { throw 'Python 3.12 is needed: winget install Python.Python.3.12' }
}
# pip runs only when requirements.txt changed since the last good install (its hash is kept in the venv).
$reqHash = (Get-FileHash -LiteralPath (Join-Path $Here 'requirements.txt') -Algorithm SHA256).Hash
$reqStamp = Join-Path $Work 'venv\lexia-requirements.sha256'
if (-not ((Test-Path -LiteralPath $reqStamp) -and (Get-Content -LiteralPath $reqStamp -TotalCount 1) -eq $reqHash)) {
    Write-Host 'venv: installing requirements.txt'
    & $Py -I -m pip install -q --upgrade pip
    & $Py -I -m pip install -q -r (Join-Path $Here 'requirements.txt')
    if ($LASTEXITCODE -ne 0) { throw 'pip install failed' }
    Set-Content -LiteralPath $reqStamp -Value $reqHash -Encoding ASCII
} else {
    Write-Host 'venv: requirements.txt already installed'
}
. (Join-Path $PSScriptRoot 'common.ps1')   # again: the nvidia DLL folders exist now
Invoke-Py -c "import paddle; paddle.utils.run_check()"

# --- PaddleOCR ---
if (-not (Test-Path -LiteralPath (Join-Path $PaddleOcr '.git'))) {
    Write-Host 'PaddleOCR: cloning v2.9.1'
    git clone -q --depth 1 --branch v2.9.1 https://github.com/PaddlePaddle/PaddleOCR.git $PaddleOcr
}
$head = (git -C $PaddleOcr rev-parse HEAD).Trim()
if ($head -ne $paddleOcrCommit) { throw "PaddleOCR is at $head, not the pinned $paddleOcrCommit" }

# --- pretrained checkpoint ---
$tar = Join-Path $Work 'pretrained\en_PP-OCRv3_rec_train.tar'
if (-not (Test-Path -LiteralPath (Join-Path $Work 'pretrained\en_PP-OCRv3_rec_train\best_accuracy.pdparams'))) {
    New-Item -ItemType Directory (Split-Path $tar) -Force | Out-Null
    if (-not (Test-Path -LiteralPath $tar)) {
        Write-Host 'checkpoint: downloading'
        Invoke-WebRequest $ckptUrl -OutFile $tar -UseBasicParsing
    }
    # The .pdparams files are pickles: unpack (and later load) them only when the archive is the pinned one.
    if ((Get-FileHash -LiteralPath $tar -Algorithm SHA256).Hash -ne $ckptSha) { Remove-Item -LiteralPath $tar; throw 'checkpoint: SHA-256 mismatch' }
    tar -xf $tar -C (Split-Path $tar)
} else {
    Write-Host 'checkpoint: present'
}

# --- PaddleOCR's own rec_en.onnx: what check_onnx.py measures a new model against ---
# The ONNX conversion Lexia shipped before task L (deepghs/paddleocr, pinned commit and SHA-256). models\ now holds
# Lexia's fine-tuned model, so the starting point is kept here.
$origUrl = 'https://huggingface.co/deepghs/paddleocr/resolve/e7bd63d9c674c96bd4fdab435af0efb383919c18/rec/en_PP-OCRv3_rec/model.onnx'
$origSha = '18FAE12E175A4E8616B57FB001ED51270EB81CE126C652D0F88E297A8DE53F49'
$orig = Join-Path $Work 'rec_en.original.onnx'
if (-not ((Test-Path -LiteralPath $orig) -and (Get-FileHash -LiteralPath $orig -Algorithm SHA256).Hash -eq $origSha)) {
    Write-Host 'rec_en.original.onnx: downloading'
    Invoke-WebRequest $origUrl -OutFile "$orig.part" -UseBasicParsing
    if ((Get-FileHash -LiteralPath "$orig.part" -Algorithm SHA256).Hash -ne $origSha) {
        Remove-Item -LiteralPath "$orig.part"; throw 'rec_en.original.onnx: SHA-256 mismatch'
    }
    Move-Item -LiteralPath "$orig.part" $orig -Force
}

if ($SkipData) { Write-Host 'Done (no data).'; exit 0 }

# --- fonts, texts, lines ---
Invoke-Py (Join-Path $Here 'fetch_sources.py') --work $Work
if (Test-Path -LiteralPath (Join-Path $Work 'data\train_list.txt')) {
    Write-Host "data: present ($Work\data); delete the folder to make it again"
} else {
    Invoke-Py (Join-Path $Here 'gen_lines.py') --work $Work --train $TrainLines
}
Write-Host "Done. Next: .\training\rec_en\train.ps1"
