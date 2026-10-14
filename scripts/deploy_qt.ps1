param(
    [string]$QtRoot = "D:\Programming\Qt\6.11.2\msvc2022_64"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceExe = Join-Path $projectRoot "build\qt-release\cpp\EVE-Viola.exe"
$sourceDll = Join-Path $projectRoot "build\qt-release\cpp\eve_viola_native.dll"
$deployRoot = Join-Path $projectRoot "dist-cpp\EVE-Viola"
$deployExe = Join-Path $deployRoot "EVE-Viola.exe"
$windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

foreach ($required in @($sourceExe, $sourceDll, $windeployqt, $vswhere)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing deployment input: $required"
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

$vsInstall = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$crtDirectory = Get-ChildItem -LiteralPath (Join-Path $vsInstall "VC\Redist\MSVC") -Directory |
    Sort-Object Name -Descending |
    ForEach-Object {
        Get-ChildItem -LiteralPath (Join-Path $_.FullName "x64") `
            -Directory `
            -Filter "Microsoft.VC*.CRT" `
            -ErrorAction SilentlyContinue
    } |
    Select-Object -ExpandProperty FullName |
    Select-Object -First 1
if (-not $crtDirectory) {
    throw "MSVC app-local runtime directory was not found."
}

$resolvedProject = [IO.Path]::GetFullPath($projectRoot)
$resolvedDeploy = [IO.Path]::GetFullPath($deployRoot)
if (-not $resolvedDeploy.StartsWith($resolvedProject + [IO.Path]::DirectorySeparatorChar)) {
    throw "Deployment directory escaped the project root."
}

# Update the application payload without deleting users' cache or logs.
New-Item -ItemType Directory -Path $deployRoot -Force | Out-Null
Copy-Item -LiteralPath $sourceExe -Destination $deployExe -Force
Copy-Item -LiteralPath $sourceDll -Destination $deployRoot -Force

$deployCommand = 'call "{0}" >nul && "{1}" --release --no-compiler-runtime --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw "{2}"' -f $vcvars, $windeployqt, $deployExe
cmd.exe /d /s /c $deployCommand
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Get-ChildItem -LiteralPath $crtDirectory -Filter "*.dll" -File |
    Copy-Item -Destination $deployRoot -Force

if (-not (Test-Path -LiteralPath (Join-Path $deployRoot "platforms\qwindows.dll"))) {
    throw "The deployed Windows platform plugin is missing."
}

$forbidden = @("qml", "translations", "Qt6Quick.dll", "Qt6WebEngineCore.dll", "Qt6Sql.dll")
foreach ($name in $forbidden) {
    if (Test-Path -LiteralPath (Join-Path $deployRoot $name)) {
        throw "Unexpected deployment payload: $name"
    }
}

$smokeEnvironment = @{
    "QT_QPA_PLATFORM" = "windows"
    "QT_PLUGIN_PATH" = $null
    "QT_QPA_PLATFORM_PLUGIN_PATH" = $null
    "PATH" = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
}
$savedEnvironment = @{}
try {
    foreach ($name in $smokeEnvironment.Keys) {
        $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
        [Environment]::SetEnvironmentVariable($name, $smokeEnvironment[$name], "Process")
    }
    # Smoke mode uses temporary configuration and its own single-instance key.
    $smokeProcess = Start-Process -FilePath $deployExe `
        -ArgumentList "--smoke-exit-ms", "500" `
        -WorkingDirectory $deployRoot `
        -WindowStyle Hidden `
        -PassThru
} finally {
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], "Process")
    }
}
if (-not $smokeProcess.WaitForExit(15000)) {
    Stop-Process -Id $smokeProcess.Id -ErrorAction SilentlyContinue
    throw "The packaged Windows startup test timed out. See $deployRoot\logs."
}
if ($smokeProcess.ExitCode -ne 0) {
    throw "The packaged Windows startup test failed with exit code $($smokeProcess.ExitCode). See $deployRoot\logs."
}

$bytes = (Get-ChildItem -LiteralPath $deployRoot -File -Recurse | Measure-Object Length -Sum).Sum
Write-Host ("Deployed dist-cpp\EVE-Viola ({0:N1} MB)" -f ($bytes / 1MB))
Write-Host "Packaged Windows startup test passed without the Qt SDK on PATH."
