# Checks that CLAUDE.md, the module map (docs/module-map.md) and the plan are up to date. Run it before each merge; CI runs it on every PR
# (.github/workflows/docs.yml).
#
#   .\scripts\check_docs.ps1                  # content checks, and the diff checks against origin/main
#   .\scripts\check_docs.ps1 -Base main       # diff checks against another ref
#   .\scripts\check_docs.ps1 -NoDiff          # content checks only (CI on main, or a PR labelled docs-not-needed)
#
# Content checks (always):
#   - CLAUDE.md keeps its required rules: clangd, Security Guidance, comments in code, the Plan section, the
#     "How work runs" section, and the pointers to docs/module-map.md and .claude/agents/docs-reviewer.md.
#   - The version in vcpkg.json is named in CLAUDE.md and in the plan (so a release is not forgotten in the docs).
#   - docs/module-map.md exists, and each file in the first column of its table exists (src\ or the repo root).
#   - Each scripts\*.ps1, docs\*.md and .claude\agents\*.md path that CLAUDE.md or docs/module-map.md names
#     exists (a failure names the file that names the missing path).
#   - The plan has its "Status" and "The owner's open checks" sections.
# Diff checks (against -Base):
#   - A change to code (src, tests, cmake, installer, scripts, CMakeLists.txt, vcpkg.json) also changes the plan.
#   - A source file added to, removed from or renamed under src\ also changes docs/module-map.md.
#
# Exit 0 = all good, 1 = a check failed (each failure is printed). Works in Windows PowerShell 5.1 and pwsh.

param(
    [string]$Base = 'origin/main',
    [switch]$NoDiff
)

$ErrorActionPreference = 'Stop'
# git prints paths in UTF-8; without this, Windows PowerShell decodes them in the OEM code page and the
# plan's em dash no longer matches.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$root = Split-Path -Parent $PSScriptRoot
$failures = New-Object System.Collections.Generic.List[string]
function Fail([string]$message) { $failures.Add($message) }

# The plan's file name has an em dash; find it by pattern, so this file stays ASCII (PowerShell 5.1
# reads a script without a BOM in the ANSI code page).
$planFile = Get-ChildItem -LiteralPath (Join-Path $root 'docs') -Filter 'Lexia*Plan.md' | Select-Object -First 1
$claudeFile = Join-Path $root 'CLAUDE.md'
# The module map table lives in its own file, so CLAUDE.md stays short; both are checked here.
$mapFile = Join-Path $root 'docs\module-map.md'
if (-not (Test-Path -LiteralPath $claudeFile)) { Write-Host 'FAIL CLAUDE.md is missing'; exit 1 }
if (-not (Test-Path -LiteralPath $mapFile)) { Write-Host 'FAIL docs/module-map.md is missing (the module map table lives there)'; exit 1 }
if (-not $planFile) { Write-Host 'FAIL the plan (docs\Lexia*Plan.md) is missing'; exit 1 }

$claude = Get-Content -LiteralPath $claudeFile -Raw -Encoding UTF8
$map = Get-Content -LiteralPath $mapFile -Raw -Encoding UTF8
$plan = Get-Content -LiteralPath $planFile.FullName -Raw -Encoding UTF8

# --- Required rules in CLAUDE.md ---------------------------------------------------------------------
$required = [ordered]@{
    'the clangd rule ("Use clangd for C++")'                   = 'Use clangd for C\+\+'
    'the Security Guidance rule ("Use Security Guidance")'     = 'Use Security Guidance when you plan code'
    'the comments rule ("Comment the code you write")'         = 'Comment the code you write'
    'the docs check rule (check_docs.ps1)'                     = 'check_docs\.ps1'
    'the "## Plan" section'                                    = '(?m)^## Plan\s*$'
    'the "## How work runs" section'                           = '(?m)^## How work runs\s*$'
    'the pointer to docs/module-map.md'                        = 'docs/module-map\.md'
    'the pointer to .claude/agents/docs-reviewer.md'           = '\.claude/agents/docs-reviewer\.md'
}
foreach ($name in $required.Keys) {
    if ($claude -notmatch $required[$name]) { Fail "CLAUDE.md has lost $name" }
}

# --- Version ------------------------------------------------------------------------------------------
$version = (Get-Content -LiteralPath (Join-Path $root 'vcpkg.json') -Raw | ConvertFrom-Json).version
if ($version) {
    $v = [regex]::Escape($version)
    if ($claude -notmatch $v) { Fail "CLAUDE.md does not name the version $version of vcpkg.json" }
    if ($plan -notmatch $v) { Fail "the plan does not name the version $version of vcpkg.json" }
}

# --- Plan sections ------------------------------------------------------------------------------------
foreach ($heading in @('## Status', "## The owner's open checks")) {
    if ($plan -notmatch ('(?m)^' + [regex]::Escape($heading))) { Fail "the plan has no section '$heading'" }
}

# --- Module map: every file in the first column exists ------------------------------------------------
# The table is in docs/module-map.md; every "| <cell> | ... |" row of that file is read (the "File" header and
# the "---" separator are skipped). A cell looks like "app.*", "tray.*, instance.*, resource_ids.h",
# "native_ui/" or "LICENSE, licenses/".
foreach ($line in ($map -split "`r?`n")) {
    if ($line -notmatch '^\| ([^|]+) \|') { continue }
    $cell = $Matches[1].Trim()
    if ($cell -eq 'File' -or $cell -match '^-+$') { continue }
    foreach ($item in ($cell -split ',\s*')) {
        $item = $item.Trim().Trim('`').TrimEnd('/')
        if ($item -match '^res$') { $item = 'res' }
        $pattern = $item -replace '\.\*$', '.*'
        $found = $false
        foreach ($dir in @((Join-Path $root 'src'), $root)) {
            if ($pattern.EndsWith('.*')) {
                $parent = Join-Path $dir (Split-Path -Parent $pattern)
                $leaf = Split-Path -Leaf $pattern
                if ((Test-Path -LiteralPath $parent) -and (Get-ChildItem -LiteralPath $parent -Filter $leaf -ErrorAction SilentlyContinue)) { $found = $true }
            } elseif (Test-Path -LiteralPath (Join-Path $dir $pattern)) { $found = $true }
            if ($found) { break }
        }
        if (-not $found) { Fail "docs/module-map.md names '$item', which is not in src\ or the repo root" }
    }
}

# --- Script, doc and agent paths named in CLAUDE.md and docs/module-map.md exist ------------------------
# Both files are scanned; each failure names the file that names the missing path, so it is clear what to fix.
$pathPattern = '(?:scripts[\\/][A-Za-z0-9_]+\.ps1|docs[\\/][A-Za-z0-9_./\\-]+\.md|\.claude[\\/]agents[\\/][A-Za-z0-9_-]+\.md)'
foreach ($doc in @(@{ Name = 'CLAUDE.md'; Text = $claude }, @{ Name = 'docs/module-map.md'; Text = $map })) {
    $paths = [regex]::Matches($doc.Text, $pathPattern) | ForEach-Object { $_.Value } | Sort-Object -Unique
    foreach ($p in $paths) {
        if (-not (Test-Path -LiteralPath (Join-Path $root ($p -replace '/', '\')))) { Fail "$($doc.Name) names '$p', which does not exist" }
    }
}

# --- Diff checks ----------------------------------------------------------------------------------------
if (-not $NoDiff) {
    $mergeBase = git -c core.quotepath=off -C $root merge-base $Base HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $mergeBase) {
        Fail "cannot find the merge base with '$Base' (fetch it, or pass -Base / -NoDiff)"
    } else {
        # Committed changes since the merge base, plus what is not committed yet (a local run before a commit).
        $changed = @(git -c core.quotepath=off -C $root diff --name-only $mergeBase) + @(git -c core.quotepath=off -C $root ls-files --others --exclude-standard) |
            Where-Object { $_ } | Sort-Object -Unique
        $status = @(git -c core.quotepath=off -C $root diff --name-status $mergeBase) | Where-Object { $_ }
        $planPath = 'docs/' + $planFile.Name
        $codeChanged = $changed | Where-Object { $_ -match '^(src|tests|cmake|installer|scripts)/' -or $_ -in @('CMakeLists.txt', 'vcpkg.json') }
        if ($codeChanged -and ($changed -notcontains $planPath)) {
            Fail "code changed ($(@($codeChanged).Count) files) but the plan did not: update '$planPath' (or label the PR docs-not-needed)"
        }
        # A (added), D (deleted) or R (renamed) source file under src/ changes the module map (docs/module-map.md).
        $srcAddedOrRemoved = $status | Where-Object { $_ -match '^(A|D|R\d*)\s+src/' }
        $srcAddedOrRemoved += @(git -c core.quotepath=off -C $root ls-files --others --exclude-standard -- src) | Where-Object { $_ }
        if ($srcAddedOrRemoved -and ($changed -notcontains 'docs/module-map.md')) {
            Fail "a source file was added, removed or renamed under src/ but docs/module-map.md did not change: update the module map"
        }
    }
}

if ($failures.Count) {
    foreach ($f in $failures) { Write-Host "FAIL $f" }
    exit 1
}
Write-Host "OK  CLAUDE.md, the module map and the plan pass the checks (version $version)."
exit 0
