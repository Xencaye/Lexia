<#
.SYNOPSIS
  Opens the Settings window pages (ui\) in a browser with sample data, and can take screenshots.

.DESCRIPTION
  Copies ui\ to build\ui-preview\, writes the text of the C++ strings table beside it as
  dev\strings.js (from `lexia_cli --strings-json en`, so the table stays the one source of text),
  and opens build\ui-preview\index.html. It is a local file: no server, no network. The mock host
  (ui\dev\mock_host.js) gives the page its sample data.

  The icons and engravings of the pages are CSS masks. A page opened from file:// may load a mask
  only when Edge runs with --allow-file-access-from-files, so the preview starts Edge itself with that
  switch and its own profile folder (inside WebView2 the page is same-origin and needs nothing).

  URL switches: ?theme=system|light|dark  ?state=on|off  ?page=home|voices|settings|report|help|about  ?hover=<id>
  The Control bar and tray previews are dev\controlbar.html and dev\tray.html (switches in their .js).
  Voices page: ?voices=none  ?speed=<n> ?pitch=<n> ?volume=<n> (a changed value shows its reset button)
  Settings page: ?select=<read|pause|speech>  ?capture=<action>  ?startup=on ?admin=on ?left=on
  Sheets: ?page=voices&sheet=catalogue|downloading|done|failed|offline|added|refused|delete|deleteFailed|presetSaved|presetLoaded|presetFailed
  &problem=<code> (a fixed state, no timers); ?net=off (every download fails); ?file=<code> (Add from file is refused)

.PARAMETER Cli
  lexia_cli.exe, built (default build\cli\Release\lexia_cli.exe).

.PARAMETER NoOpen
  Do not open the browser.

.PARAMETER Screens
  Also write headless Edge screenshots (800 x 600, the size of the window) to build\ui-preview\shots\: the six
  pages in light and dark, Home On and Off with the power circle hovered, the Voices page with a changed pitch (its
  hover thumb and its reset button), the Settings page with each key selected, a key that waits for a key and the
  switches On, and the sheets (download, add from file, delete, preset results). Then the Control bar and tray previews.
#>
param(
    [string]$Cli = 'build\cli\Release\lexia_cli.exe',
    [switch]$NoOpen,
    [switch]$Screens
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Where Edge is: an override, the PATH, the registry, then the usual folders (CI's windows-latest has it).
function Find-Edge {
    if ($env:LEXIA_EDGE -and (Test-Path $env:LEXIA_EDGE)) { return $env:LEXIA_EDGE }
    $cmd = Get-Command msedge.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($key in 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe',
                     'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe') {
        $p = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).'(default)'
        if ($p -and (Test-Path $p)) { return $p }
    }
    foreach ($base in ${env:ProgramFiles(x86)}, $env:ProgramFiles, $env:LOCALAPPDATA) {
        if (-not $base) { continue }
        $p = Join-Path $base 'Microsoft\Edge\Application\msedge.exe'
        if (Test-Path $p) { return $p }
    }
    return $null
}

if (-not (Test-Path $Cli)) { throw "lexia_cli not found at '$Cli'. Build it first, or pass -Cli <exe>." }

$out = Join-Path $root 'build\ui-preview'
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item (Join-Path $root 'ui\*') $out -Recurse

# The strings, from the one C++ table. UTF-8 without a BOM.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$json = (& $Cli --strings-json en) -join "`n"
if ($LASTEXITCODE) { throw "lexia_cli --strings-json failed (exit $LASTEXITCODE)" }
$js = "// Written by scripts\ui_preview.ps1 from lexia_cli --strings-json en. Not part of the package.`nwindow.LEXIA_DEV_STRINGS = $json;`n"
[System.IO.File]::WriteAllText((Join-Path $out 'dev\strings.js'), $js, (New-Object System.Text.UTF8Encoding($false)))

$index = Join-Path $out 'index.html'
$base = ([Uri]$index).AbsoluteUri
Write-Host "Preview: $index"

$edge = Find-Edge
if ($Screens -and -not $edge) { throw 'Microsoft Edge was not found (set LEXIA_EDGE to msedge.exe).' }

if ($Screens) {
    $shots = Join-Path $out 'shots'
    New-Item -ItemType Directory -Path $shots | Out-Null
    $profile = Join-Path ([IO.Path]::GetTempPath()) ('lexia-ui-shots-' + [Guid]::NewGuid().ToString('N'))

    # The masks (icons, engravings) need --allow-file-access-from-files on file://.
    function Shoot($name, $query, $size = '800,600', $scale = 1) {
        $file = Join-Path $shots "$name.png"
        $edgeArgs = @('--headless', '--disable-gpu', '--hide-scrollbars', '--allow-file-access-from-files', "`"--user-data-dir=$profile`"",
                  "--window-size=$size", "--force-device-scale-factor=$scale",
                  '--virtual-time-budget=3000', "`"--screenshot=$file`"", "`"$base$query`"")
        Start-Process -FilePath $edge -ArgumentList $edgeArgs -Wait -NoNewWindow -RedirectStandardError (Join-Path $shots 'edge.log')
        if (-not (Test-Path $file)) { throw "Edge wrote no screenshot for $name" }
        Write-Host "  $file"
    }

    foreach ($theme in 'light', 'dark') {
        # The six pages. Home Off and On, each with the power circle hovered too (home_*_hover.png).
        Shoot "home-off-$theme" "?theme=$theme&state=off"
        Shoot "home-on-$theme" "?theme=$theme&state=on"
        Shoot "home-off-hover-$theme" "?theme=$theme&state=off&hover=power"
        Shoot "home-on-hover-$theme" "?theme=$theme&state=on&hover=power"
        Shoot "voices-$theme" "?theme=$theme&page=voices"
        # A changed pitch: its reset button shows, and the thumb is in its hover size (voices.png).
        Shoot "voices-changed-$theme" "?theme=$theme&page=voices&pitch=3&hover=pitch"
        Shoot "voices-user-voice-$theme" "?theme=$theme&page=voices&active=en_GB-alba-medium&hover=voice-delete"
        Shoot "voices-none-$theme" "?theme=$theme&page=voices&voices=none"
        foreach ($key in 'read', 'pause', 'speech') { Shoot "settings-$key-$theme" "?theme=$theme&page=settings&select=$key" }
        Shoot "settings-capture-$theme" "?theme=$theme&page=settings&capture=pause"
        Shoot "settings-switches-$theme" "?theme=$theme&page=settings&startup=on&admin=on&left=on"
        foreach ($page in 'report', 'help', 'about') { Shoot "$page-$theme" "?theme=$theme&page=$page" }

        # The sheets: each state is fixed (?sheet=...), so no timer decides what the shot shows.
        foreach ($sheet in 'catalogue', 'downloading', 'done', 'failed', 'offline', 'added', 'refused', 'delete', 'deleteFailed', 'presetSaved', 'presetLoaded', 'presetFailed') {
            Shoot "sheet-$sheet-$theme" "?theme=$theme&page=voices&sheet=$sheet"
        }
        Shoot "sheet-refused-notmedium-$theme" "?theme=$theme&page=voices&sheet=refused&problem=not-medium"
        Shoot "sheet-failed-baddownload-$theme" "?theme=$theme&page=voices&sheet=failed&problem=bad-download"
    }
    # 200 % zoom: a window about 400 CSS px wide; the fixed frame scrolls.
    Shoot 'home-on-light-zoom200' '?theme=light&state=on' '480,720' 2

    # --- D0 ui 7: Control bar and tray menu previews (dev\controlbar.html, dev\tray.html) ---
    function ShootPage($name, $page, $query, $size) {
        $url = ([Uri](Join-Path $out ('dev\' + $page))).AbsoluteUri
        $file = Join-Path $shots "$name.png"
        # --allow-file-access-from-files: the Control bar page draws its icons as CSS masks of ui\icons\*.svg
        $edgeArgs = @('--headless', '--disable-gpu', '--hide-scrollbars', '--allow-file-access-from-files', "`"--user-data-dir=$profile`"",
                  "--window-size=$size", '--force-device-scale-factor=1',
                  '--virtual-time-budget=3000', "`"--screenshot=$file`"", "`"$url$query`"")
        Start-Process -FilePath $edge -ArgumentList $edgeArgs -Wait -NoNewWindow -RedirectStandardError (Join-Path $shots 'edge.log')
        if (-not (Test-Path $file)) { throw "Edge wrote no screenshot for $name" }
        Write-Host "  $file"
    }
    foreach ($theme in 'light', 'dark') {
        foreach ($size in 'large', 'small') {
            foreach ($state in 'analysing', 'idle', 'speaking', 'paused') {
                ShootPage "controlbar-$size-$state-$theme" 'controlbar.html' "?chrome=0&size=$size&state=$state&theme=$theme" '1280,420'
                # The bar alone at 20,20 on grey (the PNG of the design is the 433 x 60 or 250 x 60 at that place)
                ShootPage "controlbar-bar-$size-$state-$theme" 'controlbar.html' "?chrome=0&pos=plain&size=$size&state=$state&theme=$theme" '480,100'
            }
            foreach ($hover in 'pause', 'skip', 'stop', 'help', 'size') {
                if ($hover -eq 'help' -and $size -eq 'small') { continue }  # Help is in the Large bar only
                ShootPage "controlbar-bar-$size-speaking-hover-$hover-$theme" 'controlbar.html' "?chrome=0&pos=plain&size=$size&state=speaking&hover=$hover&theme=$theme" '480,100'
            }
        }
        ShootPage "controlbar-large-speaking-game-$theme" 'controlbar.html' "?chrome=0&size=large&state=speaking&theme=$theme&shot=game" '1280,420'
        ShootPage "controlbar-small-paused-game-$theme" 'controlbar.html' "?chrome=0&size=small&state=paused&theme=$theme&shot=game&tip=skip" '1280,420'
        foreach ($state in 'on', 'off') { ShootPage "tray-$state-$theme" 'tray.html' "?chrome=0&theme=$theme&state=$state&hover=open" '800,420' }
    }
    Remove-Item $profile -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $shots 'edge.log') -ErrorAction SilentlyContinue
}

if (-not $NoOpen) {
    if ($edge) {
        # Edge with the switch that masks on file:// need, and a profile of its own (so a running Edge does not take the request over).
        $profile = Join-Path $out 'edge-profile'
        Start-Process -FilePath $edge -ArgumentList @('--allow-file-access-from-files', "`"--user-data-dir=$profile`"", '--no-first-run', "`"$base`"")
    } else {
        Write-Warning 'Edge was not found: the icons and pictures do not show in other browsers from file://.'
        Start-Process $index
    }
}
