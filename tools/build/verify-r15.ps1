[CmdletBinding()]
param([ValidateSet('d3d11', 'opengl')][string[]]$Backends = @('d3d11'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ReviewSupport.ps1')
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/navigation'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR')
$saved = Save-ProcessEnvironment -Names $names
Push-Location -LiteralPath $repository
try {
    Initialize-ReviewBuild -OutputDirectory $evidence
    $nativeDpr = [double](& ./build/release/tools/ui-audit.exe --native-dpr)
    if ($LASTEXITCODE -ne 0 -or $nativeDpr -le 0) {
        throw 'Native DPR query failed'
    }
    Invoke-ReviewCommand -OutputDirectory $evidence 'web-source.txt' { node tools/review/export-ruler.mjs }
    $reports = @()
    foreach ($backend in $Backends) {
        $env:QSG_RHI_BACKEND = $backend
        foreach ($dpr in @(1, 1.5, 2)) {
            $env:QT_SCALE_FACTOR = ($dpr / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
            $directory = Join-Path $evidence "$backend-dpr-$dpr"
            Invoke-ReviewCommand -OutputDirectory $evidence "$backend-dpr-$dpr.txt" { & ./build/release/tools/ui-audit.exe --ruler $directory }
            $report = Get-Content -Raw (Join-Path $directory 'report.json') | ConvertFrom-Json
            if (-not $report.passed -or [Math]::Abs($report.dpr - $dpr) -gt 0.001 -or $report.lifecycle.Count -ne 33) {
                throw 'Ruler drawing, DPR or lifecycle verification failed'
            }
            $reports += @{
                backend = $backend
                dpr = $dpr
                maximumStateError = $report.maximumStateError
                ordinalError = $report.ordinalError
                captures = $report.captures.Count
            }
        }
        $env:QT_SCALE_FACTOR = (1 / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
        $process = Start-Process -FilePath './build/release/bin/yalahoo.exe' -ArgumentList @('--verify-navigation', ('"' + (Join-Path $evidence "$backend-production") + '"')) -Wait -PassThru
        if ($process.ExitCode -ne 0 -or -not (Get-Content -Raw (Join-Path $evidence "$backend-production/report.json") | ConvertFrom-Json).passed) {
            throw 'Product navigation verification failed'
        }
    }
    Invoke-ReviewCommand -OutputDirectory $evidence 'ctest.txt' { ctest --preset qt-release -R '^(source-format|navigation|motion)$' --output-on-failure --no-tests=error }
    Invoke-ReviewCommand -OutputDirectory $evidence 'shader.txt' { & "$($chain.qtKit)/bin/qsb.exe" --dump build/release/src/app/.qsb/edge-fade.frag.qsb }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = $true
                qtVersion = $chain.qtVersion
                compiler = $chain.compiler
                configuration = 'Release'
                reports = $reports
                scope = 'R15 bounded ruler, shared input and decimal tracks; local drawing diagnostics are not product FPS or screen input-to-photon measurements.'
            } | ConvertTo-Json -Depth 8), $utf8)
} finally {
    Restore-ProcessEnvironment -Saved $saved
    Pop-Location
}
