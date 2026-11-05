# Writes build\clangd\compile_commands.json for clangd (the C++ language server; .clangd points it
# there), which the Visual Studio generator cannot produce: a Ninja configure of the same project,
# with nothing built. Needs Visual Studio's C++ tools and vcpkg: $env:VCPKG_ROOT, else the vcpkg
# toolchain the main build (build\) was configured with. Run it again after a source file is added
# or a compile option changes.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ($env:VCPKG_ROOT) {
    $toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
} else {
    $cache = Join-Path $root 'build\CMakeCache.txt'
    $line = if (Test-Path -LiteralPath $cache) { Select-String -LiteralPath $cache -Pattern '^CMAKE_TOOLCHAIN_FILE:[A-Z]*=(.+)$' | Select-Object -First 1 }
    if (-not $line) { throw 'Set $env:VCPKG_ROOT first (CLAUDE.md, Build), or configure build\ once.' }
    $toolchain = $line.Matches[0].Groups[1].Value
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'No Visual Studio with the C++ tools was found.' }

$out = Join-Path $root 'build\clangd'
# Ninja and cl.exe come from the Developer Command Prompt; a .cmd file avoids PowerShell 5.1's
# quoting of nested quotes for cmd.exe.
$cmdFile = Join-Path ([IO.Path]::GetTempPath()) ("lexia_compile_db_" + [guid]::NewGuid().ToString('N') + '.cmd')
@"
@set "PATH=$(Split-Path $vswhere);%PATH%"
@call "$vs\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -no_logo || exit /b 1
cmake -S "$root" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLEXIA_PACKAGE=OFF "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DVCPKG_TARGET_TRIPLET=x64-windows
"@ | Set-Content -LiteralPath $cmdFile -Encoding ascii
try {
    & cmd.exe /d /c $cmdFile
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
} finally {
    Remove-Item -LiteralPath $cmdFile -ErrorAction SilentlyContinue
}

# piper.h comes from the Piper build, which this configure does not run: take the main build's.
$piperH = Join-Path $root 'build\lp_install\include\piper.h'
if (Test-Path -LiteralPath $piperH) {
    Copy-Item -LiteralPath $piperH (Join-Path $out 'lp_install\include') -Force
} else {
    Write-Warning 'build\lp_install\include\piper.h is not there yet: build Lexia once and run this again (until then clangd reports piper.h as missing).'
}
Write-Host "Wrote $out\compile_commands.json"
