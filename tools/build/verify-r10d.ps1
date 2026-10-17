[CmdletBinding()]
param(
    [ValidateRange(12, 120)][int]$Seconds = 120,
    [ValidateSet('d3d11', 'opengl')][string]$Backend = 'd3d11'
)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository "verification/windows/rhi-integration/$Backend"
if (Test-Path -LiteralPath $evidence) {
    throw 'Choose a fresh checkout/output; preserve existing raw samples.'
}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Run([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1
    $code = $LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path $evidence $Name), ($output | Out-String), $utf8)
    if ($code -ne 0) {
        throw "$Name failed; raw output retained"
    }
}
$names = @('PATH', 'QT_PLUGIN_PATH', 'QT_SCALE_FACTOR', 'QSG_RHI_BACKEND', 'QSG_NO_VSYNC', 'QSG_RENDER_LOOP', 'YALAHOO_VERIFY_VISIBLE', 'YALAHOO_VERIFY_FOREGROUND', 'YALAHOO_VERIFY_LIFECYCLE')
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run 'configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_PERFORMANCE_RECORD_ONLY=ON }
    Run 'build.txt' { cmake --build --preset qt-release }
    Run 'format-lint.txt' { cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint }
    $chain = Get-Content -Raw -Encoding UTF8 build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QT_SCALE_FACTOR = '1'
    $env:QSG_RHI_BACKEND = $Backend
    $env:QSG_RENDER_LOOP = 'threaded'
    $env:YALAHOO_VERIFY_VISIBLE = '1'
    $env:YALAHOO_VERIFY_FOREGROUND = '0'
    $env:YALAHOO_VERIFY_LIFECYCLE = '0'
    Remove-Item Env:QSG_NO_VSYNC -ErrorAction SilentlyContinue
    Run 'source-tests.txt' { ctest --preset qt-release -R '^(source-format|rhi-material-parameters|scene|lighting|motion|render-quality|module-link-and-plugins|shader-tools)$' --output-on-failure --no-tests=error }
    Run 'hidden-tests.txt' { ctest --preset qt-release-render -R '^rhi-(bokeh|smaa)-gpu$' --output-on-failure --no-tests=error }
    Run 'native-tests.txt' { ctest --preset qt-release-visible -R '^(runtime-(scene-dpr-.*|render-backend|materials|lighting|occlusion|postprocess-dpr-.*)|rhi-(opaque|shell|postprocess|ao|fog|shadow)-gpu)$' --output-on-failure --no-tests=error }
    $executable = Join-Path $repository 'build/release/bin/yalahoo.exe'
    $binaryHash = (Get-FileHash -LiteralPath $executable).Hash.ToLowerInvariant()
    $runs = @()
    for ($number = 1; $number -le 3; ++$number) {
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
        $runs += @{
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
        runs = $runs
        performanceAcceptance = 'record-only'
        performanceCompliance = 'not established'
        scope = 'Actual one-case production IntegrationReview, three serial frameSwapped/CPU/Qt-submit samples. No full-product performance, scanout, photon, driver residency or historical noise explanation inferred. Original R10D numerical/noise findings remain R34/R35.'
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
