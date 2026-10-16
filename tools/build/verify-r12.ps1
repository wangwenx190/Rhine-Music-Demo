$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/archive-pool'
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
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR', 'QT_ENABLE_HIGHDPI_SCALING')
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run 'frozen-input.txt' { node tools/review/export-archive-pool-golden.mjs }
    Run 'configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_PERFORMANCE_RECORD_ONLY=OFF }
    Run 'format.txt' { cmake --build --preset qt-release --target format }
    Run 'check-format.txt' { cmake --build --preset qt-release --target check-format }
    Run 'build.txt' { cmake --build --preset qt-release }
    Run 'qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QSG_RENDER_LOOP = 'threaded'
    Remove-Item Env:QSG_NO_VSYNC -ErrorAction SilentlyContinue
    $env:QT_SCALE_FACTOR = '1'
    $env:QT_ENABLE_HIGHDPI_SCALING = '0'
    $env:QSG_RHI_BACKEND = 'd3d11'
    for ($run = 1; $run -le 3; ++$run) {
        Run "d3d11-$run.txt" { & ./build/release/tools/archive-pool-audit.exe (Join-Path $evidence "d3d11-$run") }
    }
    Run 'affected-ctest.txt' { ctest --preset qt-release -R '^(archive-pool|cover-cache|cover-gpu|rhi-atlas-queue|rhi-material-parameters|scene|lighting|motion|runtime-rhi-integration|runtime-scene-dpr-.*|verification-window-policy|source-format)$' --output-on-failure }
    $env:QSG_RHI_BACKEND = 'opengl'
    Run 'opengl.txt' { & ./build/release/tools/archive-pool-audit.exe (Join-Path $evidence 'opengl') }
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
                scope = 'R12 fixed filled pool, source transforms, picking, selection/return, rebase, upload, resource and lifecycle; CPU submit diagnostics include Review readback command costs, exclude GPU completion/actual display/PNG/SSIM, no full-product performance claim.'
            } | ConvertTo-Json -Depth 5), $utf8)
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
