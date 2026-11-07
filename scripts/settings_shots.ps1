<#
.SYNOPSIS
  Writes pictures of the native Settings window's pages with lexia_settingsshot, light and dark.

.DESCRIPTION
  Each picture is the real widget tree and the real Direct2D canvas of the window, off-screen, at 96 dpi
  (800 x 600), with the sample state of src\settingsshot_sample.cpp (the values of the old mock host). The
  file names are those of wireframe\html-reference\ (home-off-light.png, home-on-hover-dark.png, help-light.png,
  report-dark.png, about-light.png, voices-none-dark.png, sheet-catalogue-light.png ...), so each picture can be put
  next to its reference. K.3 added the Voices page and its sheets; K.4 adds the Settings page to the list below.

.PARAMETER Exe
  lexia_settingsshot.exe, built (default build\settingsshot\Release\lexia_settingsshot.exe).

.PARAMETER Out
  The folder for the pictures (default build\settingsshot\pages).

.PARAMETER Dpi
  The scale of the pictures (default 96; 144 and 192 check the sharpness at 150 % and 200 %).
#>
param(
    [string]$Exe = 'build\settingsshot\Release\lexia_settingsshot.exe',
    [string]$Out = 'build\settingsshot\pages',
    [int]$Dpi = 96
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not (Test-Path -LiteralPath $Exe)) { throw "lexia_settingsshot is not built: $Exe" }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# name, then the tool's arguments (the theme is added per picture)
$shots = @(
    @('home-off',       @('--page', 'home')),
    @('home-on',        @('--page', 'home', '--on')),
    @('home-off-hover', @('--page', 'home', '--hover', 'power')),
    @('home-on-hover',  @('--page', 'home', '--on', '--hover', 'power')),
    # K10: the window's minimise and close buttons, hovered (they are in every shot, idle)
    @('home-minimise-hover', @('--page', 'home', '--hover', 'minimise')),
    @('home-close-hover',    @('--page', 'home', '--hover', 'close')),
    @('help',           @('--page', 'help')),
    @('report',         @('--page', 'report')),
    @('about',          @('--page', 'about')),
    # K.3: the Voices page (variants of the old mock host's ?voices= / ?active= / ?pitch=) and its sheets
    @('voices',                  @('--page', 'voices')),
    @('voices-none',             @('--page', 'voices', '--voices', 'none')),
    @('voices-changed',          @('--page', 'voices', '--voices', 'changed', '--hover', 'pitch')),
    @('voices-user-voice',       @('--page', 'voices', '--voices', 'user', '--hover', 'delete')),
    @('sheet-delete',            @('--page', 'voices', '--sheet', 'delete')),
    @('sheet-deleteFailed',      @('--page', 'voices', '--sheet', 'deleteFailed')),
    @('sheet-catalogue',         @('--page', 'voices', '--sheet', 'catalogue')),
    @('sheet-offline',           @('--page', 'voices', '--sheet', 'offline')),
    @('sheet-downloading',       @('--page', 'voices', '--sheet', 'downloading')),
    @('sheet-done',              @('--page', 'voices', '--sheet', 'done')),
    @('sheet-failed',            @('--page', 'voices', '--sheet', 'failed')),
    @('sheet-failed-baddownload', @('--page', 'voices', '--sheet', 'failed-baddownload')),
    @('sheet-added',             @('--page', 'voices', '--sheet', 'added')),
    @('sheet-refused',           @('--page', 'voices', '--sheet', 'refused')),
    @('sheet-refused-notmedium', @('--page', 'voices', '--sheet', 'refused-notmedium')),
    @('sheet-presetSaved',       @('--page', 'voices', '--sheet', 'presetSaved')),
    @('sheet-presetLoaded',      @('--page', 'voices', '--sheet', 'presetLoaded')),
    @('sheet-presetFailed',      @('--page', 'voices', '--sheet', 'presetFailed')),
    @('sheet-presetLoadedOther', @('--page', 'voices', '--sheet', 'presetLoadedOther')),
    @('sheet-presetNotInstalled', @('--page', 'voices', '--sheet', 'presetNotInstalled')),
    # K.4, the Settings page: each Key's card, the capture state and the switches (the owner's reference look)
    @('settings-read',     @('--page', 'settings', '--select', 'read')),
    @('settings-pause',    @('--page', 'settings', '--select', 'pause')),
    @('settings-speech',   @('--page', 'settings', '--select', 'speech')),
    # K9: the card of each switch (no HTML reference: the owner's new page)
    @('settings-switch-readAlong',      @('--page', 'settings', '--select', 'readAlong')),
    @('settings-switch-runAtStartup',   @('--page', 'settings', '--select', 'runAtStartup')),
    @('settings-switch-startMinimised', @('--page', 'settings', '--select', 'startMinimised')),
    @('settings-switch-runAsAdmin',     @('--page', 'settings', '--select', 'runAsAdmin')),
    @('settings-switch-darkMode',       @('--page', 'settings', '--select', 'darkMode')),
    @('settings-switch-leftHanded',     @('--page', 'settings', '--select', 'leftHanded')),
    @('settings-capture',  @('--page', 'settings', '--capture', 'pause')),
    # K11: the read button while its card is read aloud: the stop icon (volume-07), no wiggle
    @('settings-reading',  @('--page', 'settings', '--select', 'read', '--reading')),
    @('settings-switches', @('--page', 'settings', '--switches'))
)

$failed = 0
foreach ($theme in 'light', 'dark') {
    foreach ($shot in $shots) {
        $file = Join-Path $Out ("{0}-{1}.png" -f $shot[0], $theme)
        & $Exe @($shot[1]) --theme $theme --dpi $Dpi $file | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "$($shot[0]) ($theme) failed with exit code $LASTEXITCODE"
            $failed++
        }
    }
}
Write-Host ("Wrote {0} pictures to {1}" -f (Get-ChildItem $Out -Filter *.png).Count, (Resolve-Path $Out))
if ($failed -gt 0) { exit 1 }
