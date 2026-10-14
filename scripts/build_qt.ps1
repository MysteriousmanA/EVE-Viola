param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipDeploy,
    [string]$QtRoot = "D:\Programming\Qt\6.11.2\msvc2022_64"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$qtInstallRoot = Split-Path -Parent (Split-Path -Parent $QtRoot)
$cmake = Join-Path $qtInstallRoot "Tools\CMake_64\bin\cmake.exe"
$ninja = Join-Path $qtInstallRoot "Tools\Ninja\ninja.exe"
$qtConfig = Join-Path $QtRoot "lib\cmake\Qt6\Qt6Config.cmake"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

foreach ($required in @($cmake, $ninja, $qtConfig, $vswhere)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing build dependency: $required"
    }
}

$vcvars = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find "VC\Auxiliary\Build\vcvars64.bat"
if (-not $vcvars) {
    throw "MSVC x64 tools were not found."
}

$preset = if ($Configuration -eq "Debug") { "qt-debug" } else { "qt-release" }
$command = 'call "{0}" >nul && set VSLANG=1033&& set QT_ROOT={1}&& "{2}" --preset {3} --fresh && "{2}" --build --preset {3}' -f $vcvars, $QtRoot, $cmake, $preset

Push-Location -LiteralPath $projectRoot
try {
    cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Qt build failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Host "Built build\$preset\cpp\EVE-Viola.exe"

if (-not $SkipDeploy -and $Configuration -eq "Release") {
    & (Join-Path $PSScriptRoot "deploy_qt.ps1") -QtRoot $QtRoot
}
