$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/covers'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Run([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1
    $code = $LASTEXITCODE
    $text = ($output | Out-String).Replace($repository, '<REPOSITORY>').Replace($repository.Replace('\', '/'), '<REPOSITORY>')
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $text, $utf8)
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
    $samples = @()
    for ($run = 1; $run -le 3; ++$run) {
        Run "cache-$run.txt" { ctest --preset qt-release -R '^cover-cache$' --output-on-failure }
        $sample = Get-Content -Raw build/release/verification/cover-cache.json | ConvertFrom-Json
        Copy-Item -LiteralPath build/release/verification/cover-cache.json -Destination (Join-Path $evidence "cache-$run.json")
        $samples += $sample
    }
    Run 'review-ctest.txt' { ctest --preset qt-release -R '^(cover-gpu|rhi-atlas-queue|rhi-material-parameters|runtime-rhi-integration|verification-window-policy|source-format|runtime-scene-dpr-.*)$' --output-on-failure }
    Copy-Item -LiteralPath build/release/verification/covers/runtime.json -Destination (Join-Path $evidence 'd3d11.json')
    Run 'rhi-diagnostics.txt' { & ./build/release/tools/rhi-postprocess-audit.exe (Join-Path $evidence 'rhi-diagnostics') --diagnostics }
    $env:QSG_RHI_BACKEND = 'opengl'
    Run 'opengl.txt' { & ./build/release/tools/cover-audit.exe (Join-Path $evidence 'opengl') }
    $env:QSG_RHI_BACKEND = 'd3d11'
    $dump = & "$($chain.qtKit)/bin/qsb.exe" --dump build/release/src/render/rhi/.qsb/opaque.frag.qsb 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $dump -notmatch 'MSL' -or $dump -notmatch 'HLSL' -or $dump -notmatch 'GLSL' -or $dump -notmatch 'SPIR-V') {
        throw 'Opaque shader lacks cross-backend variants'
    }
    [IO.File]::WriteAllText((Join-Path $evidence 'opaque-variants.txt'), $dump, $utf8)
    Run 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF -DYALAHOO_PERFORMANCE_RECORD_ONLY=OFF }
    Run 'production-build.txt' { cmake --build --preset qt-release }
    Run 'production-ctest.txt' { ctest --preset qt-release --output-on-failure }
    Run 'production-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    [IO.File]::WriteAllText((Join-Path $evidence 'summary.json'), (@{
                passed = $true
                qtVersion = $chain.qtVersion
                configuration = 'Release'
                samples = $samples
                scope = 'R11 bounded decode/lease/upload lifecycle and affected renderer regressions; no full-product performance acceptance'
            } | ConvertTo-Json -Depth 8), $utf8)
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
    Pop-Location
}
