[CmdletBinding()]
param(
    [ValidateRange(0, 120)][int]$Seconds = 0,
    [ValidateRange(1, 3)][int]$Runs = 1,
    [ValidateSet('d3d11', 'opengl')][string]$Backend = 'd3d11'
)
$ErrorActionPreference = 'Stop'
if ($Seconds -lt 12) {
    throw 'Specify -Seconds 12..120 for a concrete unresolved timing question; no default performance sample.'
}
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository "verification/windows/rhi-integration/$Backend"
if (Test-Path -LiteralPath $evidence) {
    throw 'Choose a fresh checkout/output; preserve existing raw samples.'
}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$names = @('PATH', 'QT_PLUGIN_PATH', 'QT_SCALE_FACTOR', 'QSG_RHI_BACKEND', 'QSG_NO_VSYNC', 'QSG_RENDER_LOOP', 'YALAHOO_VERIFY_VISIBLE', 'YALAHOO_VERIFY_FOREGROUND', 'YALAHOO_VERIFY_LIFECYCLE', 'YALAHOO_VERIFY_STRESS')
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location -LiteralPath $repository
try {
    $chain = Get-Content -Raw -Encoding UTF8 build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QT_SCALE_FACTOR = '1'
    $env:QSG_RHI_BACKEND = $Backend
    $env:QSG_RENDER_LOOP = 'threaded'
    $env:YALAHOO_VERIFY_VISIBLE = '1'
    $env:YALAHOO_VERIFY_FOREGROUND = '0'
    $env:YALAHOO_VERIFY_LIFECYCLE = '0'
    $env:YALAHOO_VERIFY_STRESS = '0'
    Remove-Item Env:QSG_NO_VSYNC -ErrorAction SilentlyContinue
    $executable = Join-Path $repository 'build/release/bin/yalahoo.exe'
    $binaryHash = (Get-FileHash -LiteralPath $executable).Hash.ToLowerInvariant()
    $sampleReports = @()
    for ($number = 1; $number -le $Runs; ++$number) {
        $destination = Join-Path $evidence "run-$number"
        $start = [Diagnostics.ProcessStartInfo]::new()
        $start.FileName = $executable
        $start.WorkingDirectory = $repository
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        foreach ($argument in @('--verify-integration', $destination, '--integration-seconds', [string]$Seconds)) {
            $start.ArgumentList.Add($argument)
        }
        $native = [Diagnostics.Process]::Start($start)
        try {
            $stdout = $native.StandardOutput.ReadToEndAsync()
            $stderr = $native.StandardError.ReadToEndAsync()
            if (-not $native.WaitForExit(210000)) {
                $native.Kill($true)
                $native.WaitForExit()
                throw 'Integration measurement timed out'
            }
            [IO.File]::WriteAllText((Join-Path $evidence "run-$number.log"), ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()), $utf8)
            if ($native.ExitCode -ne 0) {
                throw 'Integration measurement failed'
            }
        } finally {
            $native.Dispose()
        }
        $report = Get-Content -Raw -Encoding UTF8 (Join-Path $destination 'runtime.json') | ConvertFrom-Json
        $expectedApi = if ($Backend -eq 'd3d11') {
            4
        } else {
            3
        }
        if (-not $report.passed -or -not $report.samplingComplete -or -not $report.windowPolicy.passed -or $report.graphicsApi -ne $expectedApi -or $report.durationSeconds -ne $Seconds -or $report.vsyncDisabled -or $report.coalescedSamples -ne 0 -or $report.inputRevisionsSubmitted -ne 4) {
            throw 'Incomplete actual backend/integration sample'
        }
        $sampleReports += @{
            report = "run-$number/runtime.json"
            sha256 = (Get-FileHash -LiteralPath (Join-Path $destination 'runtime.json')).Hash.ToLowerInvariant()
        }
    }
    $summary = @{
        passed = $true
        configuration = 'Release'
        qtVersion = $chain.qtVersion
        backend = $Backend
        seconds = $Seconds
        formalDuration = ($Seconds -eq 120)
        binarySha256 = $binaryHash
        requestedRuns = $Runs
        runs = $sampleReports
        performanceAcceptance = 'record-only'
        performanceCompliance = 'not established'
        scope = 'Actual one-case production IntegrationReview, explicitly selected frameSwapped/CPU/Qt-submit samples. No full-product performance, scanout, photon, driver residency or historical noise explanation inferred.'
    }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), ($summary | ConvertTo-Json -Depth 10), $utf8)
    & (Join-Path $repository 'tools/audit/audit-r10d.ps1') -EvidenceDirectory $evidence
    if (-not $?) {
        throw 'Integration source/evidence audit failed'
    }
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
