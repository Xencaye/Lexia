# Downloads what is not in git: ONNX Runtime into onnxruntime, the OCR models into models and the Piper Voices into voices
# (and, with -Installer, the WebView2 Runtime's bootstrapper into installer\redist).
# Every file is checked against a pinned SHA-256; files already present and correct are skipped.
# Network use in the repo: this script, and the build, which clones Piper and espeak-ng at pinned commits (CMakeLists.txt). ADR-0005 covers the app.

param(
    [switch]$SkipVoices,  # CI builds and tests without speaking, so it skips the 126 MB of Voices
    [switch]$Installer    # also the WebView2 Runtime's Evergreen Bootstrapper, for the installer (H4)
)

# --- Pins: to change a version, change the URL and the hash together ------------------------
# ONNX Runtime (MIT)
$ortVersion = '1.20.1'
$ortUrl = "https://github.com/microsoft/onnxruntime/releases/download/v$ortVersion/onnxruntime-win-x64-$ortVersion.zip"
$ortSha = '78D447051E48BD2E1E778BBA378BEC4ECE11191C9E538CF7B2C4A4565E8F5581'

# OCR models (PaddleOCR PP-OCR, Apache-2.0, ONNX conversions), each pinned to a commit
$rapid = 'https://huggingface.co/SWHL/RapidOCR/resolve/1cfba2e90fc938db55889873735088de210cc173'
# deepghs mirror is under its RDL licence, which defers to the original (PaddleOCR, Apache-2.0); keep PaddleOCR's notice in H2
$deeps = 'https://huggingface.co/deepghs/paddleocr/resolve/e7bd63d9c674c96bd4fdab435af0efb383919c18'
$models = @(
    @{ Name = 'det.onnx';           Url = "$rapid/PP-OCRv4/ch_PP-OCRv4_det_infer.onnx";        Sha = 'D2A7720D45A54257208B1E13E36A8479894CB74155A5EFE29462512D42F49DA9' }
    @{ Name = 'rec_en.onnx';        Url = "$deeps/rec/en_PP-OCRv3_rec/model.onnx";             Sha = '18FAE12E175A4E8616B57FB001ED51270EB81CE126C652D0F88E297A8DE53F49' }
    @{ Name = 'dict_en.txt';        Url = "$deeps/rec/en_PP-OCRv3_rec/dict.txt";               Sha = '5662DF9D2D03F0E8CA0D3B0649D6ACBAB904B6A14B3D3521463C71C37C668CE3' }
    @{ Name = 'rec_cyrillic.onnx';  Url = "$deeps/rec/cyrillic_PP-OCRv3_rec/model.onnx";       Sha = 'BEFB0C18A5648D44C6E7891EA10D8EE8EDD8EE98698E2B952C33474025273F31' }
    @{ Name = 'dict_cyrillic.txt';  Url = "$deeps/rec/cyrillic_PP-OCRv3_rec/dict.txt";         Sha = '369A82C6C8C479784A5D726448B83B1EAFB5FEF0A4129A5EAA3929625DDCD132' }
)
# ---------------------------------------------------------------------------------------------

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is very slow in 5.1 with the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

$root = Split-Path -Parent $PSScriptRoot

# -LiteralPath throughout: a clone path with [ or ] must not be read as a wildcard
function Test-Hash($path, $sha256) {
    (Test-Path -LiteralPath $path) -and ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -eq $sha256)
}

function Get-Verified($name, $url, $sha256, $dest) {
    $part = "$dest.part"   # a dropped download must never leave a truncated file where the build looks
    try {
        try {
            Invoke-WebRequest $url -OutFile $part -UseBasicParsing
        } catch {
            throw "${name}: download failed from $url`n  $($_.Exception.Message)"
        }
        $got = (Get-FileHash -LiteralPath $part -Algorithm SHA256).Hash
        if ($got -ne $sha256) { throw "${name}: SHA-256 mismatch for $url`n  expected $sha256`n  got      $got" }
        Move-Item -LiteralPath $part $dest -Force
    } finally {
        if (Test-Path -LiteralPath $part) { Remove-Item -LiteralPath $part }
    }
}

# --- ONNX Runtime ----------------------------------------------------------------------------
# The stamp is written into onnxruntime\ only once the whole folder is in place: the zip's hash on
# the first line, then "<SHA-256>  <path>" for every file the zip put there. The folder is present
# only if every one of those files still has its hash, so a hand-copied, older, edited or
# half-deleted folder (or a cache that restored one) is downloaded again.
function Test-OrtFolder($dir, $stamp, $zipSha) {
    if (-not (Test-Path -LiteralPath $stamp)) { return $false }
    $lines = @(Get-Content -LiteralPath $stamp)
    if ($lines.Count -lt 2 -or $lines[0] -ne $zipSha) { return $false }   # also an older stamp: the zip hash only
    foreach ($line in $lines[1..($lines.Count - 1)]) {
        $hash, $rel = $line -split '  ', 2
        if (-not $rel -or -not (Test-Hash (Join-Path $dir $rel) $hash)) { return $false }
    }
    return $true
}

$ortDir = Join-Path $root 'onnxruntime'
$ortStamp = Join-Path $ortDir '.fetched-sha256'
if (Test-OrtFolder $ortDir $ortStamp $ortSha) {
    Write-Host "onnxruntime ${ortVersion}: present (every file checked)"
} else {
    # same volume as $ortDir: Move-Item can't move a folder across drives (build\ is git-ignored)
    Get-ChildItem -LiteralPath (Join-Path $root 'build') -Directory -Filter 'ort_download_*' -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue   # left by a killed run
    $tmp = Join-Path $root ("build\ort_download_" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $tmp -Force | Out-Null
    try {
        Write-Host "onnxruntime ${ortVersion}: downloading"
        $zip = Join-Path $tmp 'ort.zip'
        Get-Verified "onnxruntime $ortVersion" $ortUrl $ortSha $zip
        Expand-Archive -LiteralPath $zip $tmp
        # the zip has one top-level folder; its contents become onnxruntime\
        $unpacked = (Get-Item -LiteralPath (Join-Path $tmp "onnxruntime-win-x64-$ortVersion")).FullName
        $stamp = @($ortSha) + @(Get-ChildItem -LiteralPath $unpacked -Recurse -File | ForEach-Object {
            "$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)  $($_.FullName.Substring($unpacked.Length + 1))"
        })
        [IO.File]::WriteAllLines((Join-Path $unpacked '.fetched-sha256'), [string[]]$stamp)
        if (Test-Path -LiteralPath $ortDir) {
            if (Test-Path -LiteralPath $ortStamp) { Remove-Item -LiteralPath $ortStamp }   # first, in case the delete stops halfway
            Remove-Item -LiteralPath $ortDir -Recurse -Force
        }
        Move-Item -LiteralPath $unpacked $ortDir
    } finally {
        # a locked leftover (antivirus, OneDrive) in a git-ignored folder must not hide the real error
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# --- OCR models ------------------------------------------------------------------------------
$modelsDir = Join-Path $root 'models'
New-Item -ItemType Directory $modelsDir -Force | Out-Null
foreach ($m in $models) {
    $dest = Join-Path $modelsDir $m.Name
    if (Test-Hash $dest $m.Sha) { Write-Host "$($m.Name): present"; continue }
    if (Test-Path $dest) { Write-Host "$($m.Name): wrong SHA-256, replacing" } else { Write-Host "$($m.Name): downloading" }
    Get-Verified $m.Name $m.Url $m.Sha $dest
}
# --- WebView2 Runtime bootstrapper (only with -Installer) -------------------------------------
# installer\redist\MicrosoftEdgeWebview2Setup.exe: Microsoft's "Evergreen Bootstrapper", which the
# installer runs when a PC has no WebView2 Runtime (Lexia's Settings window needs it). Microsoft allows
# apps to ship it. Its link always gives the newest build, so no SHA-256 can be pinned: the file is
# kept only if it carries a valid Authenticode signature of Microsoft Corporation.
if ($Installer) {
    $redist = Join-Path $root 'installer\redist'
    New-Item -ItemType Directory $redist -Force | Out-Null
    $boot = Join-Path $redist 'MicrosoftEdgeWebview2Setup.exe'
    function Test-MicrosoftSigned($path) {
        if (-not (Test-Path -LiteralPath $path)) { return $false }
        $sig = Get-AuthenticodeSignature -LiteralPath $path
        $sig.Status -eq 'Valid' -and $sig.SignerCertificate.Subject -match 'O=Microsoft Corporation'
    }
    if (Test-MicrosoftSigned $boot) {
        Write-Host 'MicrosoftEdgeWebview2Setup.exe: present'
    } else {
        Write-Host 'MicrosoftEdgeWebview2Setup.exe: downloading'
        $part = "$boot.part"
        Invoke-WebRequest 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile $part -UseBasicParsing
        if (-not (Test-MicrosoftSigned $part)) {
            Remove-Item -LiteralPath $part -Force
            throw 'MicrosoftEdgeWebview2Setup.exe is not signed by Microsoft Corporation: not kept.'
        }
        Move-Item -LiteralPath $part $boot -Force
    }
}

# --- Piper Voices ----------------------------------------------------------------------------
if ($SkipVoices) { Write-Host "voices: skipped"; Write-Host "Done."; exit 0 }
# voices\voices.json (tracked) lists the Voices, the pinned download source and the hashes. A
# Voice with "ship": false is fetched for local testing only: the package step never copies it.
$voicesDir = Join-Path $root 'voices'
New-Item -ItemType Directory $voicesDir -Force | Out-Null
$voiceList = Get-Content -LiteralPath (Join-Path $voicesDir 'voices.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($v in $voiceList.voices) {
    $note = ''
    if (-not $v.ship) { $note = ' (not cleared for release: local testing only, never packaged)' }
    foreach ($f in @(@{ Ext = '.onnx'; Sha = $v.onnxSha256 }, @{ Ext = '.onnx.json'; Sha = $v.jsonSha256 })) {
        $name = "$($v.name)$($f.Ext)"
        $dest = Join-Path $voicesDir $name
        if (Test-Hash $dest $f.Sha) { Write-Host "${name}: present$note"; continue }
        if (Test-Path -LiteralPath $dest) { Write-Host "${name}: wrong SHA-256, replacing" } else { Write-Host "${name}: downloading$note" }
        Get-Verified $name "$($voiceList.source)/$($v.path)/$name" $f.Sha $dest
    }
}
Write-Host 'Done.'
