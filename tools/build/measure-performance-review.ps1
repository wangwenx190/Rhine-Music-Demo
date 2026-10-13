[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BeforeExecutable,
    [string]$Output = 'build/release/performance-review',
    [ValidateRange(1, 10)][int]$FirstRun = 1,
    [ValidateRange(1, 10)][int]$Runs = 3
)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location -LiteralPath $repository
try {
    $before = (Resolve-Path -LiteralPath $BeforeExecutable).Path
    $after = (Resolve-Path -LiteralPath 'build/release/tools/rhi-postprocess-audit.exe').Path
    if (-not (Select-String -LiteralPath build/release/CMakeCache.txt -Pattern '^YALAHOO_REVIEW:BOOL=ON$')) {
        throw 'Configure qt-release with YALAHOO_REVIEW=ON and build rhi-postprocess-audit before measuring.'
    }
    $kit = (Get-Content build/release/CMakeCache.txt | Select-String '^YALAHOO_SELECTED_QT:PATH=(.*)$').Matches.Groups[1].Value
    if (-not $kit) {
        throw 'Configure and build the latest Kit Release Review before measuring.'
    }
    $env:PATH = "$kit/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$kit/plugins"
    $env:QT_SCALE_FACTOR = '1'
    $env:QT_ENABLE_HIGHDPI_SCALING = '0'
    $env:QSG_RENDER_LOOP = 'threaded'
    $env:QSG_NO_VSYNC = '1'
    for ($run = $FirstRun; $run -lt $FirstRun + $Runs; ++$run) {
        $order = if ($run % 2) {
            @('before', 'after')
        } else {
            @('after', 'before')
        }
        foreach ($variant in $order) {
            $destination = Join-Path $Output "$variant/run-$run"
            if (Test-Path -LiteralPath $destination) {
                throw "Keep prior samples; choose an unused run number: $destination"
            }
            New-Item -ItemType Directory -Force -Path $destination | Out-Null
            $executable = if ($variant -eq 'before') {
                $before
            } else {
                $after
            }
            $start = [Diagnostics.ProcessStartInfo]::new()
            $start.FileName = $executable
            $start.ArgumentList.Add((Join-Path $repository $destination))
            $start.WorkingDirectory = $repository
            $start.UseShellExecute = $false
            $start.CreateNoWindow = $true
            $start.RedirectStandardOutput = $true
            $start.RedirectStandardError = $true
            $measurementProcess = [Diagnostics.Process]::Start($start)
            try {
                $stdout = $measurementProcess.StandardOutput.ReadToEndAsync()
                $stderr = $measurementProcess.StandardError.ReadToEndAsync()
                if (-not $measurementProcess.WaitForExit(420000)) {
                    $measurementProcess.Kill($true)
                    $measurementProcess.WaitForExit()
                    throw "Measurement timed out: $variant run $run"
                }
                $exitCode = $measurementProcess.ExitCode
                ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()) | Set-Content -LiteralPath (Join-Path $destination 'process.txt') -Encoding utf8
            } finally {
                $measurementProcess.Dispose()
            }
            $reportPath = Join-Path $destination 'runtime.json'
            if (-not (Test-Path -LiteralPath $reportPath)) {
                throw "Incomplete measurement: $variant run $run, exit $exitCode"
            }
            $report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
            if ($report.performance.durationSeconds -ne 120 -or -not $report.windowPolicy.passed -or $report.cases.Count -ne 50) {
                throw "Invalid measurement scope/window policy: $variant run $run"
            }
            # A numeric gate failure remains evidence; it is not silently discarded or relaxed.
            [pscustomobject]@{
                exitCode = $exitCode
                executableSha256 = (Get-FileHash -LiteralPath $executable).Hash.ToLowerInvariant()
                gatePassed = $report.performance.passed
                scope = 'Renderer camera-motion throughput; no product/compositor claim'
            } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'measurement.json') -Encoding utf8
            Write-Output "$variant run $run retained, numeric gate=$($report.performance.passed)"
        }
        $beforeReport = Get-Content -Raw (Join-Path $Output "before/run-$run/runtime.json") | ConvertFrom-Json
        $afterReport = Get-Content -Raw (Join-Path $Output "after/run-$run/runtime.json") | ConvertFrom-Json
        if ($beforeReport.performance.driver -ne $afterReport.performance.driver) {
            throw 'Different presentation protocols. Samples retained; compile both renderers with the same measurement driver before comparing gains.'
        }
    }
} finally {
    Pop-Location
}
