$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/real-columns'
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
Push-Location -LiteralPath $repository
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR', 'QT_ENABLE_HIGHDPI_SCALING', 'YALAHOO_REAL_COLUMN_GOLDEN')
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run 'frozen-input.txt' { node tools/review/export-real-columns-golden.mjs }
    Run 'filled-input.txt' { node tools/review/export-archive-pool-golden.mjs }
    Run 'configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_PERFORMANCE_RECORD_ONLY=OFF }
    Run 'format.txt' { cmake --build --preset qt-release --target format }
    Run 'check-format.txt' { cmake --build --preset qt-release --target check-format }
    Run 'build.txt' { cmake --build --preset qt-release }
    Run 'qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QSG_RENDER_LOOP = 'threaded'
    $env:QSG_NO_VSYNC = ''
    $env:QT_SCALE_FACTOR = '1'
    $env:QT_ENABLE_HIGHDPI_SCALING = '0'
    $env:QSG_RHI_BACKEND = 'd3d11'
    $env:YALAHOO_REAL_COLUMN_GOLDEN = Join-Path $evidence 'full-trajectories.json'
    Run 'full-source-trajectories.txt' { & ./build/release/RealColumnsTest.exe -o (Join-Path $evidence 'full-source-trajectories.xml,junitxml') }
    $env:YALAHOO_REAL_COLUMN_GOLDEN = ''
    for ($run = 1; $run -le 3; ++$run) {
        Run "d3d11-$run.txt" { & ./build/release/tools/archive-pool-audit.exe (Join-Path $evidence "d3d11-$run") --real-columns }
    }
    Run 'affected-ctest.txt' { ctest --preset qt-release -R '^(real-columns|archive-pool|archive-pool-gpu|cover-cache|cover-gpu|rhi-atlas-queue|rhi-material-parameters|scene|lighting|motion|library-data|runtime-rhi-integration|runtime-scene-dpr-.*|runtime-filled|verification-window-policy|source-format)$' --output-on-failure }
    $env:QSG_RHI_BACKEND = 'opengl'
    Run 'opengl.txt' { & ./build/release/tools/archive-pool-audit.exe (Join-Path $evidence 'opengl') --real-columns }
    $env:QSG_RHI_BACKEND = 'd3d11'
    Run 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF -DYALAHOO_PERFORMANCE_RECORD_ONLY=OFF }
    Run 'production-build.txt' { cmake --build --preset qt-release }
    Run 'production-ctest.txt' { ctest --preset qt-release --output-on-failure }
    Run 'production-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = $true
                qtVersion = $chain.qtVersion
                compiler = $chain.compiler
                configuration = 'Release'
                sampleDirectories = @('d3d11-1', 'd3d11-2', 'd3d11-3', 'opengl')
                scope = 'R13 finite columns, independent source x/v/a and field, navigation boundaries, session memory, preference saves, image/cover/lifecycle and affected filled regression. CPU submission diagnostic excludes GPU completion and actual display; full product performance remains R28.'
            } | ConvertTo-Json -Depth 5), $utf8)
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
