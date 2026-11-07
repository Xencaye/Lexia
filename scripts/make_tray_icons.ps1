# Makes the icons of lexia.exe from the Lexia logo, the owner's own drawing as two SVG files (K12):
#   src/res/lexia.ico      the On look, src/res/lexia_logo.svg (also the icon of lexia.exe and of Setup)
#   src/res/lexia_off.ico  the Off look, src/res/lexia_logo_off.svg (the engraving faded, with a large muted speaker;
#                          the tray shows it while Lexia is Off). There is no grey filter and no slash any more:
#                          the Off file is the look.
# System.Drawing cannot read SVG, so each frame is drawn by lexia_svgshot (Direct2D, the renderer the app uses for
# its icons; build\svgshot\Release\lexia_svgshot.exe, built with the project: `cmake --build build --config Release
# --target lexia_svgshot`). Each .ico holds 16, 20, 24, 32, 48, 64 and 256 px frames, all PNG-compressed (Windows 10
# and 11 read that; the 256 px frame is the sharp one for Explorer and Setup).
# The .ico files are committed; run this only when a logo file changes:
#   powershell -ExecutionPolicy Bypass -File scripts\make_tray_icons.ps1
param(
    [string]$Source = 'src\res\lexia_logo.svg',
    [string]$OffSource = 'src\res\lexia_logo_off.svg',
    [string]$Svgshot = 'build\svgshot\Release\lexia_svgshot.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Resolve-InRoot([string]$p) { if ([System.IO.Path]::IsPathRooted($p)) { $p } else { Join-Path $root $p } }
$Source = Resolve-InRoot $Source
$OffSource = Resolve-InRoot $OffSource
$Svgshot = Resolve-InRoot $Svgshot
foreach ($f in $Source, $OffSource, $Svgshot) {
    if (-not (Test-Path $f)) { throw "Missing: $f (build lexia_svgshot first, see the header of this script)" }
}
$sizes = 16, 20, 24, 32, 48, 64, 256
$work = Join-Path ([System.IO.Path]::GetTempPath()) ('lexia_icons_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $work | Out-Null

# Draws the SVG at every size and writes the .ico (a PNG frame per size).
function Write-Ico([string]$svg, [string]$path) {
    $pngs = @()
    foreach ($s in $sizes) {
        $png = Join-Path $work ("f$s.png")
        & $Svgshot --px $s $svg $png
        if ($LASTEXITCODE -ne 0) { throw "lexia_svgshot failed for $svg at $s px" }
        $pngs += , [System.IO.File]::ReadAllBytes($png)
    }
    $out = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $out
    $bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        # In an .ico directory entry the size 256 is written as 0.
        $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
        $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
        $bw.Write([uint16]1); $bw.Write([uint16]32)
        $bw.Write([uint32]$pngs[$i].Length); $bw.Write([uint32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($p in $pngs) { $bw.Write($p) }
    $bw.Flush()
    [System.IO.File]::WriteAllBytes($path, $out.ToArray())
}

try {
    $dest = Join-Path $root 'src\res'
    New-Item -ItemType Directory -Force $dest | Out-Null
    Write-Ico $Source (Join-Path $dest 'lexia.ico')
    Write-Ico $OffSource (Join-Path $dest 'lexia_off.ico')
    Write-Host "Wrote $dest\lexia.ico and lexia_off.ico"
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
