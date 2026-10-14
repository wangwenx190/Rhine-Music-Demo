[CmdletBinding()]
param(
    [switch]$FullMatrix,
    [switch]$BackendMatrix,
    [switch]$TitleContext,
    [switch]$TitleDiagnostics,
    [switch]$UseExistingWebSource,
    [ValidateSet('d3d11', 'opengl')][string]$Backend = 'd3d11',
    [ValidateSet(1, 1.5, 2)][double]$Dpr = 1,
    [string]$EvidenceDirectory = 'verification/windows/title-reels'
)

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository $EvidenceDirectory
if ([int][bool]$FullMatrix + [int][bool]$BackendMatrix + [int][bool]$TitleContext -gt 1) {
    throw 'Choose FullMatrix, BackendMatrix or TitleContext'
}
if ($TitleDiagnostics -and -not $TitleContext) {
    throw 'TitleDiagnostics requires an explicit single TitleContext'
}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Run([string]$Name, [scriptblock]$Command, [int[]]$AllowedCodes = @(0)) {
    $output = & $Command 2>&1 | Out-String
    $code = $LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $output, $utf8)
    if ($code -notin $AllowedCodes) {
        throw "$Name failed; output retained"
    }
    Write-Output "$Name completed with exit $code"
}
function RunGui([string]$Name, [string]$Executable, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        $start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $code = $process.ExitCode
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $stdout.Result + $stderr.Result, $utf8)
    $process.Dispose()
    if ($code -ne 0) {
        throw "$Name failed with actual GUI exit $code; output retained"
    }
    Write-Output "$Name completed with actual GUI exit $code"
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
    if ($UseExistingWebSource) {
        $sourceReport = Get-Content -Raw verification/windows/title-reels/web/report.json | ConvertFrom-Json
        $requiredDprs = if ($FullMatrix -or $BackendMatrix) {
            @(1, 1.5, 2)
        } else {
            @($Dpr)
        }
        foreach ($requiredDpr in $requiredDprs) {
            if (@($sourceReport.captures | Where-Object { [Math]::Abs($_.dpr - $requiredDpr) -lt 0.001 }).Count -ne 20) {
                throw 'Existing frozen title source does not contain the requested DPR matrix'
            }
        }
        Copy-Item -LiteralPath verification/windows/title-reels/web/report.json -Destination (Join-Path $evidence 'web-source.json')
    } elseif ($FullMatrix -or $BackendMatrix) {
        Run 'web-source.txt' { node tools/review/export-title.mjs }
    } elseif ($TitleContext) {
        Run 'web-source.txt' { node tools/review/export-title.mjs --dpr $Dpr }
    }
    $reports = @()
    $backends = if ($FullMatrix) {
        @('d3d11', 'opengl')
    } elseif ($TitleContext -or $BackendMatrix) {
        @($Backend)
    } else {
        @('d3d11')
    }
    foreach ($backend in $backends) {
        $env:QSG_RHI_BACKEND = $backend
        $dprs = if ($FullMatrix -or $BackendMatrix) {
            @(1, 1.5, 2)
        } elseif ($TitleContext) {
            @($Dpr)
        } else {
            @()
        }
        foreach ($dpr in $dprs) {
            $env:QT_SCALE_FACTOR = ($dpr / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
            $directory = Join-Path $evidence "$backend-dpr-$dpr"
            $titleExitCodes = if ($TitleDiagnostics) {
                @(0, 6)
            } else {
                @(0)
            }
            Run "$backend-dpr-$dpr.txt" { & ./build/release/tools/ui-audit.exe --titles $directory } $titleExitCodes
            $report = Get-Content -Raw (Join-Path $directory 'report.json') | ConvertFrom-Json
            if ((-not $report.passed -and -not $TitleDiagnostics) -or $report.backend -ne $backend -or [Math]::Abs($report.dpr - $dpr) -gt 0.001 -or -not $report.released -or @($report.captures | Where-Object mode -eq 0).Count -ne 20 -or $report.animatedCosts.Count -ne 9 -or $report.headerChecks.Count -ne 18 -or $report.lifecycle.Count -ne 10) {
                throw 'Title glyph, animation, transport, DPR or lifecycle verification failed'
            }
            $production = @($report.captures | Where-Object mode -eq 0)
            if ($report.motionError -gt 0.000001 -or -not $report.windowPolicy.passed -or @($production | Where-Object { -not $_.valid -or -not $_.geometry.passed -or -not $_.raw.valid -or -not $_.antialiasBand.valid }).Count -gt 0 -or @($report.headerChecks | Where-Object { -not $_.passed }).Count -gt 0 -or @($report.lifecycle | Where-Object { -not $_.passed }).Count -gt 0) {
                throw 'Diagnostics cannot waive structure, motion, header, window or lifecycle gates'
            }
            foreach ($mode in 0..2) {
                $samples = @($report.animatedCosts | Where-Object { $_.mode -eq $mode })
                if ($samples.Count -ne 3 -or @($samples.run | Sort-Object -Unique).Count -ne 3) {
                    throw 'Three distinct serial samples are required for each drawing path'
                }
                foreach ($sample in $samples) {
                    if (-not $sample.passed -or $sample.frames -ne 120 -or $sample.samples.Count -ne 120 -or $null -ne $sample.gpuTime -or $null -eq $sample.guiStateNs -or $null -eq $sample.canvasPaintCpuNs -or $sample.resources.cacheBytes -gt 8388608 -or $sample.resources.cacheEntries -gt 96) {
                        throw 'Incomplete title cost, cache or raw-sequence evidence'
                    }
                }
            }
            $production = @($report.captures | Where-Object { $_.mode -eq 0 })
            if ($production.Count -ne 20) {
                throw 'Twenty production title cases are required at each DPR'
            }
            $reports += @{
                backend = $backend
                dpr = $dpr
                nativeScreenDpr = $nativeDpr
                scaleFactor = $dpr / $nativeDpr
                motionError = $report.motionError
                minimumSsimOutsideGlyphEdge = ($production.antialiasBand.ssim | Measure-Object -Minimum).Minimum
                maximumRgbOutsideGlyphEdge = ($production.antialiasBand.meanAbsoluteRgb | Measure-Object -Maximum).Maximum
                maximumGeometryError = ($production.geometry.maximumError | Measure-Object -Maximum).Maximum
                captures = $production.Count
                pixelAcceptancePassed = [bool]$report.passed
                pendingPixelCases = @($production | Where-Object { -not $_.candidatePassed } | Select-Object -ExpandProperty case)
                candidatePassCounts = @(foreach ($mode in 0..2) {
                        @($report.captures | Where-Object { $_.mode -eq $mode -and $_.candidatePassed }).Count
                    })
                animatedCosts = @($report.animatedCosts | Select-Object run, mode, frames, guiStateNs, clockTickCpuNs, syncCpuNs, renderSubmitCpuNs, canvasSyncCpuNs, canvasPaintCpuNs, canvasPaints, gpuTime, glyphNodesCreated, glyphNodesReused, resources)
            }
        }
        $productionDpr = if ($TitleContext) {
            $Dpr
        } else {
            1
        }
        $env:QT_SCALE_FACTOR = ($productionDpr / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
        RunGui "$backend-production.txt" (Join-Path $repository 'build/release/bin/yalahoo.exe') @('--verify-browse', (Join-Path $evidence "$backend-production"))
    }
    Run 'ctest.txt' { ctest --preset qt-release --output-on-failure }
    Run 'shader.txt' { & "$($chain.qtKit)/bin/qsb.exe" --dump build/release/src/app/.qsb/reel-slot.frag.qsb }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = @($reports | Where-Object { -not $_.pixelAcceptancePassed }).Count -eq 0
                diagnosticsPassed = $true
                diagnosticMode = [bool]$TitleDiagnostics
                fullMatrixCompleted = [bool]$FullMatrix
                backendMatrixCompleted = [bool]$BackendMatrix
                requestedBackend = $Backend
                titleContextCompleted = [bool]$TitleContext
                qtVersion = $chain.qtVersion
                compiler = $chain.compiler
                configuration = 'Release'
                fixtureSha256 = (Get-FileHash -LiteralPath tests/fixtures/text/titles.json).Hash.ToLowerInvariant()
                webReferenceSha256 = (Get-FileHash -LiteralPath verification/windows/title-reels/web/report.json).Hash.ToLowerInvariant()
                titleBinarySha256 = (Get-FileHash -LiteralPath build/release/tools/ui-audit.exe).Hash.ToLowerInvariant()
                productionBinarySha256 = (Get-FileHash -LiteralPath build/release/bin/yalahoo.exe).Hash.ToLowerInvariant()
                reports = $reports
                scope = 'Release implementation checks and production browse captures. Full title/path/DPR/backend selection is passed only when fullMatrixCompleted is true. CPU drawing diagnostics include initialization and bounded diagnostic submissions; GPU timestamps unavailable. No complete-product FPS claim.'
            } | ConvertTo-Json -Depth 12), $utf8)
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
