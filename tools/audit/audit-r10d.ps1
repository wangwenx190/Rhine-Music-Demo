[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$summary = Get-Content -Raw -Encoding UTF8 (Join-Path $EvidenceDirectory 'summary.json') | ConvertFrom-Json
if (-not $summary.passed -or $summary.configuration -ne 'Release' -or $summary.requestedRuns -notin @(1, 2, 3) -or $summary.runs.Count -ne $summary.requestedRuns -or $summary.performanceAcceptance -ne 'record-only') {
    throw 'The explicitly requested Release integration samples must all be present'
}
$expectedApi = if ($summary.backend -eq 'd3d11') {
    4
} elseif ($summary.backend -eq 'opengl') {
    3
} else {
    throw 'Invalid backend'
}
foreach ($run in $summary.runs) {
    $path = Join-Path $EvidenceDirectory $run.report
    if ((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ne $run.sha256) {
        throw 'Integration sample changed'
    }
    $report = Get-Content -Raw -Encoding UTF8 $path | ConvertFrom-Json
    if (-not $report.passed -or -not $report.samplingComplete -or -not $report.windowPolicy.passed -or $report.graphicsApi -ne $expectedApi -or $report.durationSeconds -ne $summary.seconds -or $report.frameEvents.Count -le 100 -or $report.coalescedSamples -ne 0) {
        throw 'Invalid actual backend or incomplete sample'
    }
}
$bindings = @(
    @('COMMON-01', 'src/app/CMakeLists.txt', 'YalahooRhi'),
    @('COMMON-02', 'src/render/rhi/PreparedAssets.cpp', 'preparedAssets'),
    @('COMMON-03', 'src/render/rhi/RhiItem.cpp', 'clipSpaceCorrMatrix'),
    @('COMMON-04', 'src/app/main.cpp', 'PlaybackController playback'),
    @('COMMON-05', 'tools/build/verify-r10d.ps1', 'samplingComplete'),
    @('PERF-01', 'src/render/rhi/RhiItem.cpp', 'visibleEntities'),
    @('PERF-02', 'src/render/rhi/SceneItem.cpp', 'Mailbox'),
    @('PERF-03', 'src/render/rhi/RhiItem.cpp', 'updateDynamicBuffer'),
    @('PERF-04', 'src/render/rhi/ResourcePlan.h', 'resourcePlan'),
    @('PERF-05', 'src/render/rhi/Postprocess.cpp', 'Postprocess::prepare'),
    @('PERF-06', 'src/app/IntegrationReview.cpp', 'frameSwappedIntervals'),
    @('PERF-07', 'src/render/rhi/SceneItem.cpp', 'm_telemetryEpoch'),
    @('WINDOW-01', 'src/app/VerificationWindow.h', 'prepareWindow')
)
$checks = foreach ($binding in $bindings) {
    $path = Join-Path $repository $binding[1]
    if (-not [IO.File]::ReadAllText($path).Contains($binding[2])) {
        throw "Source binding drift: $($binding[1])::$($binding[2])"
    }
    @{
        checkId = $binding[0]
        sourcePath = $binding[1]
        lineOrSymbol = $binding[2]
        sha256 = (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()
        result = 'source-binding-verified'
        limitations = 'A named source binding is not a semantic audit. Current authoritative contracts, manual call-chain review and actual target evidence remain separate.'
    }
}
$result = @{
    checks = @($checks)
    backend = $summary.backend
    sampleCount = $summary.runs.Count
    performanceAcceptance = 'record-only'
    performanceCompliance = 'not established'
    limitations = 'Actual Windows backend only; no macOS/Linux target claim.'
}
[IO.File]::WriteAllText((Join-Path $EvidenceDirectory 'source-bindings.json'), ($result | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false))
Write-Output 'Integration source bindings and actual samples verified; numerical performance compliance remains unestablished.'
