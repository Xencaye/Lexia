<#
.SYNOPSIS
  Checks the colour pairs of ui\css\tokens.css against WCAG: text 4.5:1, icons, edges and states 3:1.

.DESCRIPTION
  Reads the light and the dark token blocks and prints the contrast ratio of each pair. Exit code 1
  if any pair is below its minimum. Run it after a change to tokens.css.

  Some pairs are colours that the owner chose in the design (wireframe\palette_and_buttons.png): the idle
  icon button (line colour on paper), the dotted rules and the Control bar tiles. They are not changed to pass.
  They are marked "design" and are listed in the summary so that the owner can decide. With -AllowDesign they do
  not make the exit code 1.

  The dark palette is a placeholder (TODO(owner): final dark palette); its pairs are checked too.
#>
param(
    [string]$Tokens = (Join-Path (Split-Path -Parent $PSScriptRoot) 'ui\css\tokens.css'),
    [switch]$AllowDesign
)

$css = [System.IO.File]::ReadAllText($Tokens)

function Get-Vars([string]$block) {
    $v = @{}
    foreach ($m in [regex]::Matches($block, '--([\w-]+):\s*(#[0-9a-fA-F]{6})')) { $v[$m.Groups[1].Value] = $m.Groups[2].Value }
    return $v
}
function Get-Luminance([string]$hex) {
    $c = foreach ($i in 1, 3, 5) {
        $x = [Convert]::ToInt32($hex.Substring($i, 2), 16) / 255.0
        if ($x -le 0.03928) { $x / 12.92 } else { [Math]::Pow(($x + 0.055) / 1.055, 2.4) }
    }
    return 0.2126 * $c[0] + 0.7152 * $c[1] + 0.0722 * $c[2]
}
function Get-Ratio([string]$a, [string]$b) {
    $x = Get-Luminance $a; $y = Get-Luminance $b
    if ($x -lt $y) { $x, $y = $y, $x }
    return ($x + 0.05) / ($y + 0.05)
}

$lightStart = $css.IndexOf(':root {')
$lightEnd = $css.IndexOf(':root[data-theme="light"]')
$darkStart = $css.IndexOf(':root[data-theme="dark"] {')
if ($lightStart -lt 0 -or $lightEnd -lt 0 -or $darkStart -lt 0) { throw "tokens.css does not have the expected blocks" }
$light = Get-Vars $css.Substring($lightStart, $lightEnd - $lightStart)
$dark = Get-Vars $css.Substring($darkStart)

# name, foreground, background, minimum, design (the pair is the owner's colours)
$pairs = @(
    @('ink on paper (text)', 'ink', 'paper', 4.5, $false),
    @('ink on card (the key card, the select)', 'ink', 'card', 4.5, $false),
    @('on-accent on accent (key chip, enable button, switch knob)', 'on-accent', 'accent', 4.5, $false),
    @('accent on line (the disable button, the quiet button)', 'accent', 'line', 4.5, $false),
    @('danger on paper (error text)', 'danger', 'paper', 4.5, $false),
    @('danger on card (error text in a sheet)', 'danger', 'card', 4.5, $false),
    @('accent on paper (link icons, slider thumb, switch outline, focus ring)', 'accent', 'paper', 3.0, $false),
    @('accent on card (focus ring, play button in the card)', 'accent', 'card', 3.0, $false),
    @('accent on line (an icon button on hover)', 'accent', 'line', 3.0, $false),
    @('on-accent on accent (an icon button pressed; the On power glyph)', 'on-accent', 'accent', 3.0, $false),
    @('accent on power-off-hover (the power glyph, Off, hovered)', 'accent', 'power-off-hover', 3.0, $false),
    @('on-accent on power-on-hover (the power glyph, On, hovered)', 'on-accent', 'power-on-hover', 3.0, $false),
    @('line on paper (an idle icon button; the slider track)', 'line', 'paper', 3.0, $true),
    @('line on card (the select border)', 'line', 'card', 3.0, $true),
    @('accent on tile (an icon on a Control bar tile)', 'accent', 'tile', 3.0, $true),
    @('tile on paper (the edge of a Control bar tile)', 'tile', 'paper', 3.0, $true)
)

$fail = 0
$failDesign = 0
foreach ($theme in 'light', 'dark') {
    $t = @{} + $light
    if ($theme -eq 'dark') { foreach ($k in $dark.Keys) { $t[$k] = $dark[$k] } }
    foreach ($p in $pairs) {
        $r = Get-Ratio $t[$p[1]] $t[$p[2]]
        $ok = $r -ge $p[3]
        if (-not $ok) { if ($p[4]) { $failDesign++ } else { $fail++ } }
        '{0} {1,-5} {2,5:N2}:1 (min {3})  {4}  {5} on {6}{7}' -f $(if ($ok) { 'ok  ' } else { 'FAIL' }), $theme, $r, $p[3], $p[0], $t[$p[1]], $t[$p[2]], $(if (-not $ok -and $p[4]) { '  [design colour]' } else { '' })
    }
}
if ($fail -or $failDesign) {
    Write-Host ("{0} pair(s) below the minimum: {1} of the code's colours and {2} of the owner's design colours." -f ($fail + $failDesign), $fail, $failDesign)
    if ($fail -or -not $AllowDesign) { exit 1 }
    exit 0
}
Write-Host 'All pairs pass.'
