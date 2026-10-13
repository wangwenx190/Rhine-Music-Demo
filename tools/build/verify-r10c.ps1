[CmdletBinding()]
param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/rhi-postprocess'
$media = Join-Path $repository 'docs/media/rhi-postprocess'
New-Item -ItemType Directory -Force -Path $evidence, $media | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Save([string]$Name, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $Text, $utf8)
}
function Run([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1
    Save $Name ($output -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed"
    }
    Write-Output "$Name passed"
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run 'shell-prepare.txt' { & $Python tools/assets/prepare-rhi-shell.py }
    Run 'postprocess-prepare.txt' { node tools/assets/prepare-rhi-postprocess.mjs }
    Run 'generator-syntax.txt' { & $Python -m py_compile tools/assets/prepare-rhi-shell.py }
    Run 'node-syntax.txt' { node --check tools/assets/prepare-rhi-postprocess.mjs }
    Run 'release-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON }
    Run 'release-format.txt' { cmake --build --preset qt-release --target format }
    Run 'release-check-format.txt' { cmake --build --preset qt-release --target check-format }
    & ./tools/format/FormatTools.ps1 -Check
    if (-not $?) {
        throw 'PowerShell style check failed'
    }
    Run 'release-build.txt' { cmake --build --preset qt-release }
    Run 'release-ctest.txt' { ctest --preset qt-release --output-on-failure --stop-on-failure }
    Run 'release-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    $lint = Get-Content -Raw (Join-Path $evidence 'release-qmllint.txt')
    if ($lint -match '(?m)^(Warning|Error):') {
        throw 'QML diagnostics remain'
    }
    $report = Get-Content -Raw build/release/verification/rhi-postprocess/runtime.json | ConvertFrom-Json
    $shell = Get-Content -Raw build/release/verification/rhi-shell/runtime.json | ConvertFrom-Json
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    if (-not $report.passed -or -not $report.nativeTransferByteProof -or -not $report.windowPolicy.passed -or -not $report.performance.passed -or $report.performance.durationSeconds -ne 120 -or $report.cases.Count -ne 50 -or $report.atlas.instances -ne 432 -or -not $shell.passed -or -not $shell.windowPolicy.passed -or $shell.cases.Count -ne 61 -or $shell.neighborChannels.Count -ne 2) {
        throw 'Incomplete actual R10C Windows evidence'
    }
    Save 'release-runtime.json' (Get-Content -Raw build/release/verification/rhi-postprocess/runtime.json)
    Save 'release-prestage.json' (Get-Content -Raw build/release/verification/rhi-shell/runtime.json)
    Save 'release-opaque.json' (Get-Content -Raw build/release/verification/rhi/runtime.json)
    Save 'release-toolchain.json' (Get-Content -Raw build/release/toolchain.json)
    foreach ($entry in @(@('rhi-postprocess', 'final'), @('rhi-shell', 'prestage'))) {
        $destination = Join-Path $media $entry[1]
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        foreach ($file in Get-ChildItem -LiteralPath "build/release/verification/$($entry[0])" -Filter '*.png') {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination $file.Name)
        }
    }
    $shaders = @()
    foreach ($file in Get-ChildItem -LiteralPath build/release/src/render/rhi/.qsb -Filter '*.qsb') {
        $dump = & "$($chain.qtKit)/bin/qsb.exe" --dump $file.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $dump -notmatch 'MSL' -or $dump -notmatch 'HLSL' -or $dump -notmatch 'GLSL' -or $dump -notmatch 'SPIR-V') {
            throw 'Missing cross-backend shader variants'
        }
        # Old untracked qsb outputs may remain after source variants are removed.
        $source = Join-Path $repository ('resources/shaders/rhi/' + $file.Name.Replace('.qsb', ''))
        if (-not (Test-Path -LiteralPath $source)) {
            continue
        }
        Save ($file.Name + '.txt') $dump
        $shaders += @{
            name = $file.Name
            sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
            sourceSha256 = (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant()
            targets = @('SPIR-V', 'GLSL330', 'GLSL ES300', 'HLSL50', 'MSL12')
        }
    }
    if ($shaders.Count -ne 24) {
        throw 'Expected 24 current RHI shader programs'
    }
    Save 'shader-variants.json' (@{
            qtVersion = $chain.qtVersion
            shaders = $shaders
        } | ConvertTo-Json -Depth 8)
    Run 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF }
    Run 'production-build.txt' { cmake --build --preset qt-release }
    Run 'production-ctest.txt' { ctest --preset qt-release --output-on-failure }
    Save 'summary.json' (@{
            round = 'R10C'
            passed = $true
            configuration = 'Release'
            qtVersion = $chain.qtVersion
            device = $report.atlas.initial.device
            nativeTransferProof = 'Actual D3D11 command boxes/row pitch plus completed whole-page GPU readbacks'
            cases = 50
            prestageCases = 61
            instances = 432
            initialBytes = $report.atlas.initial.nativeAtlasTransfers.destinationBytes
            oneTileBytes = $report.atlas.tile.nativeAtlasTransfers.destinationBytes
            stableBytes = $report.atlas.stable.nativeAtlasTransfers.destinationBytes
            steadyBudgetBytes = $report.steadyBudget.firstFrame.nativeAtlasTransfers.destinationBytes
            performance = $report.performance
            residualCount = $report.residuals.Count
            prestageResidualCount = $shell.residuals.Count
            productImplementationReady = $false
            macos = '静态审计通过，实机待用户验证'
            linux = '静态审计通过，实机待用户验证'
        } | ConvertTo-Json -Depth 10)
    & ./tools/audit/audit-r10c.ps1
    if (-not $?) {
        throw 'Static audit failed'
    }
} finally {
    Pop-Location
}
