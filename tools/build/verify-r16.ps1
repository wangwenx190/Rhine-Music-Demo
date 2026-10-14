[CmdletBinding()]
param(
    [switch]$FullMatrix,
    [switch]$TitleContext,
    [ValidateSet('d3d11', 'opengl')][string]$Backend = 'd3d11',
    [ValidateSet(1, 1.5, 2)][double]$Dpr = 1,
    [string]$EvidenceDirectory = 'verification/windows/title-reels'
)

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository $EvidenceDirectory
if ($FullMatrix -and $TitleContext) {
    throw 'Choose FullMatrix or TitleContext'
}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Run([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1 | Out-String
    $code = $LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $output, $utf8)
    if ($code -ne 0) {
        throw "$Name failed; output retained"
    }
    Write-Output "$Name passed"
}
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR', 'QCPAINTER_DEBUG_COLLECT')
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run 'configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF }
    Run 'format.txt' { cmake --build --preset qt-release --target format }
    Run 'check-format.txt' { cmake --build --preset qt-release --target check-format }
    Run 'build.txt' { cmake --build --preset qt-release }
    Run 'qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint ui-audit_qmllint }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QSG_RENDER_LOOP = 'threaded'
    $env:QSG_NO_VSYNC = ''
    $env:QCPAINTER_DEBUG_COLLECT = '0'
    $env:QT_SCALE_FACTOR = '1'
    $nativeDpr = [double]((& ./build/release/tools/ui-audit.exe --native-dpr 2>&1 | Out-String).Trim())
    if ($LASTEXITCODE -ne 0 -or $nativeDpr -le 0) {
        throw 'Native DPR query failed'
    }
    if ($FullMatrix) {
        Run 'web-source.txt' { node tools/review/export-title.mjs }
    } elseif ($TitleContext) {
        Run 'web-source.txt' { node tools/review/export-title.mjs --dpr $Dpr }
    }
    $reports = @()
    $backends = if ($FullMatrix) {
        @('d3d11', 'opengl')
    } elseif ($TitleContext) {
        @($Backend)
    } else {
        @('d3d11')
    }
    foreach ($backend in $backends) {
        $env:QSG_RHI_BACKEND = $backend
        $dprs = if ($FullMatrix) {
            @(1, 1.5, 2)
        } elseif ($TitleContext) {
            @($Dpr)
        } else {
            @()
        }
        foreach ($dpr in $dprs) {
            $env:QT_SCALE_FACTOR = ($dpr / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
            $directory = Join-Path $evidence "$backend-dpr-$dpr"
            Run "$backend-dpr-$dpr.txt" { & ./build/release/tools/ui-audit.exe --titles $directory }
            $report = Get-Content -Raw (Join-Path $directory 'report.json') | ConvertFrom-Json
            if (-not $report.passed -or $report.backend -ne $backend -or [Math]::Abs($report.dpr - $dpr) -gt 0.001 -or -not $report.released -or $report.animatedCosts.Count -ne 9 -or $report.headerChecks.Count -ne 18 -or $report.lifecycle.Count -ne 10) {
                throw 'Title glyph, animation, transport, DPR or lifecycle verification failed'
            }
            $production = @($report.captures | Where-Object { $_.mode -eq 0 })
            $reports += @{
                backend = $backend
                dpr = $dpr
                motionError = $report.motionError
                minimumSsimOutsideGlyphEdge = ($production.antialiasBand.ssim | Measure-Object -Minimum).Minimum
                maximumRgbOutsideGlyphEdge = ($production.antialiasBand.meanAbsoluteRgb | Measure-Object -Maximum).Maximum
                maximumGeometryError = ($production.geometry.maximumError | Measure-Object -Maximum).Maximum
                captures = $production.Count
            }
        }
        $env:QT_SCALE_FACTOR = (1 / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
        Run "$backend-production.txt" { & ./build/release/bin/yalahoo.exe --verify-browse (Join-Path $evidence "$backend-production") }
    }
    Run 'ctest.txt' { ctest --preset qt-release --output-on-failure }
    Run 'shader.txt' { & "$($chain.qtKit)/bin/qsb.exe" --dump build/release/src/app/.qsb/reel-slot.frag.qsb }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = $true
                fullMatrixCompleted = [bool]$FullMatrix
                titleContextCompleted = [bool]$TitleContext
                qtVersion = $chain.qtVersion
                compiler = $chain.compiler
                configuration = 'Release'
                reports = $reports
                scope = 'Release implementation checks and production browse captures. Full title/path/DPR/backend selection is passed only when fullMatrixCompleted is true. CPU drawing diagnostics include initialization and bounded diagnostic submissions; GPU timestamps unavailable. No complete-product FPS claim.'
            } | ConvertTo-Json -Depth 8), $utf8)
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
