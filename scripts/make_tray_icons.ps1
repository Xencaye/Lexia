# Makes the tray icons from a square picture with transparency: -Source <png>. The committed icons come from
# the Lexia logo src/res/lexia_logo.png (a public-domain engraving, licenses/components/ui-engravings.txt).
#   src/res/lexia.ico      the On look: the picture as it is (also the icon of lexia.exe)
#   src/res/lexia_off.ico  the Off look: the picture in grey with a slash, so it does not rely on colour
# Each .ico holds 16, 20, 24, 32 and 48 px frames (PNG-compressed; Windows 10 and 11 read them).
# The .ico files are committed; run this only when the picture or the look changes:
#   powershell -ExecutionPolicy Bypass -File scripts\make_tray_icons.ps1 -Source <picture.png>
param([Parameter(Mandatory = $true)][string]$Source)
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$picture = [System.Drawing.Image]::FromFile((Resolve-Path $Source).Path)
$sizes = 16, 20, 24, 32, 48

function New-Frame([int]$size, [bool]$off) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)
    if ($off) {
        # Grey: luminance matrix, a little lighter and see-through.
        $m = New-Object System.Drawing.Imaging.ColorMatrix
        $m.Matrix00 = 0.30; $m.Matrix01 = 0.30; $m.Matrix02 = 0.30
        $m.Matrix10 = 0.59; $m.Matrix11 = 0.59; $m.Matrix12 = 0.59
        $m.Matrix20 = 0.11; $m.Matrix21 = 0.11; $m.Matrix22 = 0.11
        $m.Matrix33 = 0.7; $m.Matrix40 = 0.25; $m.Matrix41 = 0.25; $m.Matrix42 = 0.25
        $attr = New-Object System.Drawing.Imaging.ImageAttributes
        $attr.SetColorMatrix($m)
        $rect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
        $g.DrawImage($picture, $rect, 0, 0, $picture.Width, $picture.Height, [System.Drawing.GraphicsUnit]::Pixel, $attr)
        # The slash: a light under-stroke for contrast on a dark taskbar, a dark stroke on top.
        $w = [Math]::Max(1.5, $size / 11.0)
        $a = [single]($size * 0.14); $b = [single]($size * 0.86)
        $under = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(200, 255, 255, 255)), ($w + [Math]::Max(1.0, $size / 20.0))
        $under.StartCap = 'Round'; $under.EndCap = 'Round'
        $top = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 40, 40, 40)), $w
        $top.StartCap = 'Round'; $top.EndCap = 'Round'
        $g.DrawLine($under, $a, $a, $b, $b)
        $g.DrawLine($top, $a, $a, $b, $b)
    } else {
        $g.DrawImage($picture, 0, 0, $size, $size)
    }
    $g.Dispose()
    return $bmp
}

function Write-Ico([string]$path, [bool]$off) {
    $pngs = @()
    foreach ($s in $sizes) {
        $bmp = New-Frame $s $off
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngs += , $ms.ToArray()
        $bmp.Dispose()
    }
    $out = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $out
    $bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $bw.Write([byte]$sizes[$i]); $bw.Write([byte]$sizes[$i]); $bw.Write([byte]0); $bw.Write([byte]0)
        $bw.Write([uint16]1); $bw.Write([uint16]32)
        $bw.Write([uint32]$pngs[$i].Length); $bw.Write([uint32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($p in $pngs) { $bw.Write($p) }
    $bw.Flush()
    [System.IO.File]::WriteAllBytes($path, $out.ToArray())
}

$dest = Join-Path $root 'src\res'
New-Item -ItemType Directory -Force $dest | Out-Null
Write-Ico (Join-Path $dest 'lexia.ico') $false
Write-Ico (Join-Path $dest 'lexia_off.ico') $true
$picture.Dispose()
Write-Host "Wrote $dest\lexia.ico and lexia_off.ico"
