param(
    [string]$ToolsRoot = "D:\Programming\Qt\Tools"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$cmake = Join-Path $ToolsRoot "CMake_64\bin\cmake.exe"
$ninja = Join-Path $ToolsRoot "Ninja\ninja.exe"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

foreach ($required in @($cmake, $ninja, $vswhere)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing build tool: $required"
    }
}

$vcvars = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find "VC\Auxiliary\Build\vcvars64.bat"

if (-not $vcvars) {
    throw "MSVC x64 tools were not found. Install the Desktop development with C++ workload."
}

$buildDirectory = Join-Path $projectRoot "build\windows-release"
$command = 'call "{0}" >nul && "{1}" -S "{2}" -B "{3}" --fresh -G Ninja -DCMAKE_MAKE_PROGRAM="{4}" -DCMAKE_BUILD_TYPE=Release && "{1}" --build "{3}"' -f $vcvars, $cmake, $projectRoot, $buildDirectory, $ninja

cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Native build failed with exit code $LASTEXITCODE"
}

Write-Host "Built build\windows-release\native\eve_viola_native.dll"
