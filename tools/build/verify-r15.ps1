[CmdletBinding()]
param([ValidateSet('d3d11', 'opengl')][string[]]$Backends = @('d3d11'))
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/navigation'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Run([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1
    $code = $LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path $evidence $Name), ($output | Out-String), $utf8)
    if ($code -ne 0) {
        throw "$Name failed; output retained"
    }
    Write-Output "$Name passed"
}
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR')
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run 'configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON }
    Run 'format.txt' { cmake --build --preset qt-release --target format }
    Run 'check-format.txt' { cmake --build --preset qt-release --target check-format }
    Run 'build.txt' { cmake --build --preset qt-release }
    Run 'qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint ui-audit_qmllint }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QSG_RENDER_LOOP = 'threaded'
    Remove-Item Env:QSG_NO_VSYNC -ErrorAction SilentlyContinue
    $env:QT_SCALE_FACTOR = '1'
    $nativeDpr = [double](& ./build/release/tools/ui-audit.exe --native-dpr)
    if ($LASTEXITCODE -ne 0 -or $nativeDpr -le 0) {
        throw 'Native DPR query failed'
    }
    Run 'web-source.txt' { node tools/review/export-ruler.mjs }
    $reports = @()
    foreach ($backend in $Backends) {
        $env:QSG_RHI_BACKEND = $backend
        foreach ($dpr in @(1, 1.5, 2)) {
            $env:QT_SCALE_FACTOR = ($dpr / $nativeDpr).ToString([Globalization.CultureInfo]::InvariantCulture)
            $directory = Join-Path $evidence "$backend-dpr-$dpr"
            Run "$backend-dpr-$dpr.txt" { & ./build/release/tools/ui-audit.exe --ruler $directory }
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
    Run 'ctest.txt' { ctest --preset qt-release -R '^(source-format|navigation|motion)$' --output-on-failure --no-tests=error }
    Run 'shader.txt' { & "$($chain.qtKit)/bin/qsb.exe" --dump build/release/src/app/.qsb/edge-fade.frag.qsb }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = $true
                qtVersion = $chain.qtVersion
                compiler = $chain.compiler
                configuration = 'Release'
                reports = $reports
                scope = 'R15 bounded ruler, shared input and decimal tracks; local drawing diagnostics are not product FPS or screen input-to-photon measurements.'
            } | ConvertTo-Json -Depth 8), $utf8)
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
