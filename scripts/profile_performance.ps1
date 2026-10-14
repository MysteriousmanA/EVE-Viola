param(
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 30,
    [string]$ConfigDirectory,
    [string]$OutputDirectory,
    [string]$QtRoot = 'D:\Programming\Qt\6.11.2\msvc2022_64',
    [switch]$CompareLegacy
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $projectRoot 'build\qt-release\cpp\eve_viola_performance_probe.exe'
if (-not (Test-Path -LiteralPath $probe)) {
    throw 'Build the Release application first with scripts/build_qt.ps1 -SkipDeploy.'
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $projectRoot 'build\performance-runs'
}
if (-not $ConfigDirectory) {
    $runningApp = Get-Process -Name 'EVE-Viola' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path } | Select-Object -First 1
    if ($runningApp) {
        $candidate = Join-Path (Split-Path -Parent $runningApp.Path) 'cache'
        if (Test-Path -LiteralPath $candidate) { $ConfigDirectory = $candidate }
    }
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$probeArguments = @('--duration-ms', ($DurationSeconds * 1000).ToString())
if ($ConfigDirectory) { $probeArguments += @('--config', $ConfigDirectory) }
$previousPath = $env:PATH
try {
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
    if ($CompareLegacy) {
        $legacyReport = Join-Path $OutputDirectory "$stamp-legacy.json"
        & $probe @probeArguments --legacy-capture --output $legacyReport
        if ($LASTEXITCODE -ne 0) { throw "Legacy sampling failed: $LASTEXITCODE" }
    }
    $optimizedReport = Join-Path $OutputDirectory "$stamp-optimized.json"
    & $probe @probeArguments --output $optimizedReport
    if ($LASTEXITCODE -ne 0) { throw "Sampling failed: $LASTEXITCODE" }
    if ($CompareLegacy) {
        $before = Get-Content -LiteralPath $legacyReport -Raw | ConvertFrom-Json
        $after = Get-Content -LiteralPath $optimizedReport -Raw | ConvertFrom-Json
        $comparison = foreach ($stage in @('native.capture_total', 'statistics.scan', 'statistics.total')) {
            $oldSamples = $before.performance.stages | Where-Object stage -eq $stage
            $newSamples = $after.performance.stages | Where-Object stage -eq $stage
            $oldCount = ($oldSamples | Measure-Object count -Sum).Sum
            $newCount = ($newSamples | Measure-Object count -Sum).Sum
            if ($oldCount -gt 0 -and $newCount -gt 0) {
                $oldMean = ($oldSamples | Measure-Object total_ms -Sum).Sum / $oldCount
                $newMean = ($newSamples | Measure-Object total_ms -Sum).Sum / $newCount
                [pscustomobject]@{
                    Stage = $stage
                    LegacyMeanMs = [math]::Round($oldMean, 3)
                    OptimizedMeanMs = [math]::Round($newMean, 3)
                    ReductionPercent = if ($oldMean -gt 0) { [math]::Round((1 - $newMean / $oldMean) * 100, 1) } else { 0 }
                }
            }
        }
        $comparison | Format-Table -AutoSize
        Write-Host ('Sampling process CPU: {0:N1} ms -> {1:N1} ms; batches: {2} -> {3}; failed: {4} -> {5}' -f
            $before.process_cpu_ms, $after.process_cpu_ms,
            $before.completed_batches, $after.completed_batches,
            $before.failed_batches, $after.failed_batches)
    }
    Write-Host "Performance reports: $OutputDirectory"
} finally {
    $env:PATH = $previousPath
}
