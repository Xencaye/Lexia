# Ends a running Lexia (lexia.exe) and lexia_cli.exe, so that a build can overwrite them.
#
# Why: a Release build fails with LNK1104 (file locked) while build\Release\lexia.exe or
# build\cli\Release\lexia_cli.exe runs. When Lexia runs as administrator (Run as admin, ADR-0008), Stop-Process from a
# shell that is not elevated gets "Access is denied". Lexia has a clean way out: its hidden window (class LexiaApp)
# accepts the registered message "Lexia.Quit" through UIPI and answers with App::Quit(), the same clean end as the
# tray's Quit (speech stops, keys are freed, the icon goes). installer\close_lexia.isi does the same for Setup.
#
# What it does:
#   1. Finds every running lexia and lexia_cli process (any path). None: prints OK and ends with 0.
#   2. lexia_cli: Stop-Process (a dev tool, in practice never elevated).
#   3. lexia: posts Lexia.Quit to every top-level window of class LexiaApp, then waits up to -TimeoutSeconds for all
#      lexia processes to end.
#   4. A lexia that is still running gets Stop-Process (works for one that is not elevated and did not answer).
#      A process that survives that (probably elevated, or hung) gets one FAIL line for the owner and exit 1.
#
# Usage (before every build):   .\scripts\close_lexia.ps1 [-TimeoutSeconds 10]
# Exit codes: 0 = nothing of Lexia runs at the end; 1 = a process is still running (read the FAIL line).
#
# Safety: it only posts one registered message to windows of Lexia's own class and stops processes named lexia /
# lexia_cli. It never starts an elevated process, never asks for UAC, uses no network and writes no log file.
# Plain ASCII; runs in Windows PowerShell 5.1 and in pwsh.

param(
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'

# The three user32 calls, wrapped once per session: a second run in the same session must not fail on a type that
# exists already. CharSet Unicode, so the class and message names are read as UTF-16.
if (-not ('LexiaClose.Native' -as [type])) {
    Add-Type -Namespace LexiaClose -Name Native -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
public static extern uint RegisterWindowMessageW(string lpString);
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
public static extern System.IntPtr FindWindowExW(System.IntPtr hwndParent, System.IntPtr hwndChildAfter, string lpszClass, string lpszWindow);
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
public static extern bool PostMessageW(System.IntPtr hWnd, uint Msg, System.UIntPtr wParam, System.IntPtr lParam);
'@
}

# Running processes with this name (none = an empty array). Process names are matched without ".exe".
function Get-Running([string]$Name) {
    return @(Get-Process -Name $Name -ErrorAction SilentlyContinue)
}

# Lexia, or the dev tool: every process that blocks a build.
function Get-AnyRunning {
    return @(Get-Running 'lexia') + @(Get-Running 'lexia_cli')
}

# Waits until no process of this name runs, at most $Seconds. Returns the seconds waited when they all ended, else $null.
function Wait-ForEnd([string]$Name, [double]$Seconds) {
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        if ((Get-Running $Name).Count -eq 0) { return $watch.Elapsed.TotalSeconds }
        if ($watch.Elapsed.TotalSeconds -ge $Seconds) { return $null }
        Start-Sleep -Milliseconds 100
    }
}

# Stop-Process for each process; returns the ones that could not be stopped (access denied or still alive).
function Stop-Each($Processes, [string]$Label) {
    $left = @()
    foreach ($p in $Processes) {
        try {
            Stop-Process -Id $p.Id -Force -ErrorAction Stop
            Write-Host ("Stopped {0} (PID {1})." -f $Label, $p.Id)
        } catch {
            # Access denied = an elevated process; already gone = fine.
            if (Get-Process -Id $p.Id -ErrorAction SilentlyContinue) { $left += $p }
        }
    }
    return $left
}

$failed = $false

# Step 1: nothing to do when nothing runs.
if ((Get-AnyRunning).Count -eq 0) {
    Write-Host 'OK  Lexia is not running.'
    exit 0
}

# Step 2: the dev tool. Never elevated in practice; if it cannot be stopped, say so like an elevated Lexia.
$cli = Get-Running 'lexia_cli'
if ($cli.Count -gt 0) {
    $null = Stop-Each $cli 'lexia_cli'
    # Give the system a moment to release the file after a stop.
    $null = Wait-ForEnd 'lexia_cli' 3
    foreach ($p in (Get-Running 'lexia_cli')) {
        Write-Host ("FAIL lexia_cli (PID {0}) is still running and cannot be stopped from here (probably as administrator). Close it, then run the build again." -f $p.Id)
        $failed = $true
    }
}

# Step 3: Lexia. Post Lexia.Quit to every window of class LexiaApp (the class name is src/instance.h kAppWindowClass).
$lexia = Get-Running 'lexia'
if ($lexia.Count -gt 0) {
    $quit = [LexiaClose.Native]::RegisterWindowMessageW('Lexia.Quit')
    $hwnd = [IntPtr]::Zero
    $posted = 0
    while ($true) {
        # FindWindowEx with a null parent walks the top-level windows; the next call continues after the last found.
        # The title is [NullString]::Value, not $null: PowerShell turns $null into "" for a .NET string parameter,
        # and FindWindowEx would then look for a window with an EMPTY title (Lexia's window is titled "Lexia").
        $hwnd = [LexiaClose.Native]::FindWindowExW([IntPtr]::Zero, $hwnd, 'LexiaApp', [NullString]::Value)
        if ($hwnd -eq [IntPtr]::Zero) { break }
        if ([LexiaClose.Native]::PostMessageW($hwnd, $quit, [UIntPtr]::Zero, [IntPtr]::Zero)) { $posted++ }
    }
    if ($posted -gt 0) {
        foreach ($p in $lexia) { Write-Host ("Asked Lexia (PID {0}) to quit (Lexia.Quit)." -f $p.Id) }
    } else {
        Write-Host 'Lexia has no window to ask (it may be starting or hung).'
    }

    # Wait for a clean end.
    $seconds = Wait-ForEnd 'lexia' $TimeoutSeconds
    if ($null -ne $seconds) {
        Write-Host ("Lexia ended after {0:N1} s." -f $seconds)
    } else {
        # Step 4: it did not answer. Stop-Process works when it is not elevated.
        Write-Host ("Lexia did not end within {0} s; stopping it." -f $TimeoutSeconds)
        $null = Stop-Each (Get-Running 'lexia') 'Lexia'
        $null = Wait-ForEnd 'lexia' 3
        foreach ($p in (Get-Running 'lexia')) {
            Write-Host ("FAIL Lexia (PID {0}) is still running, probably as administrator. Quit it from the tray (right-click the Lexia icon > Quit), then run the build again." -f $p.Id)
            $failed = $true
        }
    }
}

if ($failed) { exit 1 }
exit 0
