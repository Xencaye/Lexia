# Fine-tunes the recogniser on the RTX 3080 (task L). Run setup.ps1 first.
#
#   .\training\rec_en\train.ps1              # from PaddleOCR's pretrained weights
#   .\training\rec_en\train.ps1 -Resume      # go on from <work>\output\latest after a stop (Ctrl+C is safe)
#   .\training\rec_en\train.ps1 -Epochs 2    # a short trial
#
# The best checkpoint (by accuracy on the check lines) is <work>\output\best_accuracy.*; the log is
# <work>\output\train.log. Then: export.ps1.

param(
    [switch]$Resume,
    [int]$Epochs = 0,
    [int]$BatchSize = 0,
    [string[]]$Set = @()   # more PaddleOCR -o values, such as 'Global.print_batch_step=5'
)
. (Join-Path $PSScriptRoot 'common.ps1')

$out = Join-Path $Work 'output'
$data = Join-Path $Work 'data'
if (-not (Test-Path -LiteralPath (Join-Path $data 'train_list.txt'))) { throw "no training lines in ${data}: run setup.ps1" }

# PaddleOCR reads the -o values as YAML: forward slashes keep the Windows paths plain.
function Yp([string]$p) { return $p.Replace('\', '/') }
$opts = @(
    "Global.save_model_dir=$(Yp $out)",
    "Global.save_res_path=$(Yp (Join-Path $out 'predicts.txt'))",
    "Global.character_dict_path=$(Yp (Join-Path $Here 'dict_en.txt'))",
    "Train.dataset.data_dir=$(Yp $data)",
    "Train.dataset.label_file_list=['$(Yp (Join-Path $data 'train_list.txt'))']",
    "Eval.dataset.data_dir=$(Yp $data)",
    "Eval.dataset.label_file_list=['$(Yp (Join-Path $data 'val_list.txt'))']"
)
if ($Resume) {
    $opts += "Global.checkpoints=$(Yp (Join-Path $out 'latest'))"
} else {
    $opts += "Global.pretrained_model=$(Yp (Join-Path $Work 'pretrained\en_PP-OCRv3_rec_train\best_accuracy'))"
}
if ($Epochs -gt 0) { $opts += "Global.epoch_num=$Epochs" }
if ($BatchSize -gt 0) { $opts += "Train.loader.batch_size_per_card=$BatchSize" }
$opts += $Set

# Read every image once in parallel first: a cold first epoch is 4x slower (warm_files.py says why).
Write-Host 'Reading the training images into the file cache ...'
Invoke-Py (Join-Path $Here 'warm_files.py') $data

# PaddleOCR's tools import ppocr from the repo root, so run from there.
Push-Location $PaddleOcr
try {
    Invoke-Py tools/train.py -c (Join-Path $Here 'lexia_rec_en.yml') -o @opts
} finally {
    Pop-Location
}
Write-Host "Done. Best checkpoint: $out\best_accuracy. Next: .\training\rec_en\export.ps1"
