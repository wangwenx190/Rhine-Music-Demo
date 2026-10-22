[CmdletBinding()]
param([ValidateSet('d3d11', 'opengl')][string[]]$Backends = @('d3d11'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ReviewSupport.ps1')
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/browse-ui'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
Push-Location -LiteralPath $repository
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR')
$saved = Save-ProcessEnvironment -Names $names
try {
    $chain = Initialize-ReviewBuild -OutputDirectory $evidence
    $nativeDpr = [double](& ./build/release/tools/ui-audit.exe --native-dpr)
    if ($LASTEXITCODE -ne 0 -or $nativeDpr -le 0) {
        throw 'Native screen DPR query failed'
    }
    Invoke-ReviewCommand -OutputDirectory $evidence 'web-source.txt' { node tools/review/export-browse-ui.mjs }
    foreach ($backend in $Backends) {
        $env:QSG_RHI_BACKEND = $backend
        foreach ($dpr in @(1, 1.5, 2)) {
            $env:QT_SCALE_FACTOR = ($dpr / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
            $directory = Join-Path $evidence "$backend-dpr-$dpr"
            Invoke-ReviewCommand -OutputDirectory $evidence "$backend-dpr-$dpr.txt" { & ./build/release/tools/ui-audit.exe $directory }
            $report = Get-Content -Raw (Join-Path $directory 'report.json') | ConvertFrom-Json
            if (-not $report.passed -or [Math]::Abs($report.probes[0].dpr - $dpr) -gt 0.001) {
                throw 'Drawing verification failed or DPR differs from requested input'
            }
        }
    }
    $env:QSG_RHI_BACKEND = 'd3d11'
    $env:QT_SCALE_FACTOR = (1 / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
    $process = Start-Process -FilePath './build/release/bin/yalahoo.exe' -ArgumentList @('--verify-browse', ('"' + (Join-Path $evidence 'production') + '"')) -Wait -PassThru
    if ($process.ExitCode -ne 0 -or -not (Get-Content -Raw (Join-Path $evidence 'production/report.json') | ConvertFrom-Json).passed) {
        throw 'Product browse verification failed'
    }
    Invoke-ReviewCommand -OutputDirectory $evidence 'ctest.txt' { ctest --preset qt-release -R '^(source-format|navigation|title-reels|render-quality|assets)$' --output-on-failure --no-tests=error }
    Invoke-ReviewCommand -OutputDirectory $evidence 'paired-ui.txt' { & ./tools/review/compare-browse-ui.ps1 -Backends $Backends }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = $true
                qtVersion = $chain.qtVersion
                compiler = $chain.compiler
                configuration = 'Release'
                nativeScreenDpr = $nativeDpr
                scope = 'R14 static browse chrome, real production integration, CanvasPainter/Canvas2D/Quick comparison and lifecycle. CPU submission excludes GPU completion and screen display; full product performance remains R28.'
            } | ConvertTo-Json -Depth 5), $utf8)
} finally {
    Restore-ProcessEnvironment -Saved $saved
    Pop-Location
}
