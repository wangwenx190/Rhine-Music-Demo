[CmdletBinding()]
param([switch]$SkipWebCapture, [switch]$ReuseReviewRegression, [string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/rhi-shell'
$media = Join-Path $repository 'docs/media/rhi-shell'
New-Item -ItemType Directory -Force -Path $evidence, $media | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Save([string]$Name, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $Text, $utf8)
}
function Run([string]$Name, [scriptblock]$Command) {
    $text = & $Command 2>&1
    Save $Name ($text -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed"
    }
    Write-Output ($text -join "`n")
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    if (-not $SkipWebCapture) {
        Run 'web-export.txt' { node tools/review/export-rhi-shell.mjs } | Out-Null
    }
    Run 'shader-prepare.txt' { & $Python tools/assets/prepare-rhi-shell.py } | Out-Null
    Run 'release-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON } | Out-Null
    Run 'release-format.txt' { cmake --build --preset qt-release --target format } | Out-Null
    Run 'release-check-format.txt' { cmake --build --preset qt-release --target check-format } | Out-Null
    & ./tools/format/FormatTools.ps1 -Check
    if (-not $?) {
        throw 'PowerShell formatting failed'
    }
    Run 'release-build.txt' { cmake --build --preset qt-release } | Out-Null
    if ($ReuseReviewRegression) {
        # Preserve a complete successful Review run; only RHI/alpha-source changes may use this.
        $prior = Get-Content -Raw (Join-Path $evidence 'release-ctest.txt')
        $quick = Get-Content -Raw build/release/verification/rhi-shell-quick3d/runtime.json | ConvertFrom-Json
        $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
        $fixtureHash = (Get-FileHash tests/fixtures/rhi-shell/visual.json).Hash.ToLowerInvariant()
        if ($prior -notmatch '100% tests passed out of 35' -or $prior -match '\*\*\*Failed' -or -not $quick.passed -or -not $quick.windowPolicy.passed -or $quick.cases.Count -ne 61 -or $quick.fixtureSha256 -ne $fixtureHash -or $quick.qtVersion -ne $chain.qtVersion) {
            throw 'Reuse requires a complete Review pass, unchanged source fixture and same Qt Kit'
        }
        Run 'release-rhi-ctest.txt' { ctest --preset qt-release -R '^(source-format|verification-window-policy|rhi-material-parameters|rhi-opaque-gpu|rhi-shell-gpu)$' --output-on-failure } | Out-Null
    } else {
        Run 'release-ctest.txt' { ctest --preset qt-release --output-on-failure } | Out-Null
    }
    $lint = Run 'release-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    if ($lint -match '(?m)^(Warning|Error):') {
        throw 'QML diagnostics remain'
    }
    $report = Get-Content -Raw build/release/verification/rhi-shell/runtime.json | ConvertFrom-Json
    if (-not $report.passed -or -not $report.windowPolicy.passed -or $report.cases.Count -ne 61 -or $report.dpr -ne 1 -or $report.shadowPairs.Count -ne 6 -or $report.sorting.Count -ne 6 -or $report.neighborChannels.Count -ne 2 -or $report.captureIsolation.Count -ne 2 -or -not $report.equalDepthTie -or -not $report.duplicateIdRejected -or -not $report.alphaResolve.centerUnchanged -or $report.alphaResolve.darkFringePixels -ne 0 -or $report.maxProjectionPixels -gt 2) {
        throw 'Incomplete R10B actual GPU witnesses'
    }
    Save 'release-runtime.json' (Get-Content -Raw build/release/verification/rhi-shell/runtime.json)
    Save 'release-quick3d-runtime.json' (Get-Content -Raw build/release/verification/rhi-shell-quick3d/runtime.json)
    Save 'release-rhi-opaque-runtime.json' (Get-Content -Raw build/release/verification/rhi/runtime.json)
    Save 'release-toolchain.json' (Get-Content -Raw build/release/toolchain.json)
    foreach ($part in @('runtime', 'runtime-motion', 'runtime-library')) {
        Save ("release-window-policy-$part.json") (Get-Content -Raw "build/release/verification/$part.json")
    }
    foreach ($part in @('rhi-shell', 'rhi-shell-quick3d')) {
        $folder = if ($part -eq 'rhi-shell') {
            'rhi'
        } else {
            'quick3d'
        }
        $destination = Join-Path $media $folder
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        foreach ($file in Get-ChildItem -LiteralPath "build/release/verification/$part" -Filter '*.png') {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination $file.Name)
        }
    }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $shaders = @()
    foreach ($file in Get-ChildItem -LiteralPath build/release/src/render/rhi/.qsb -Filter '*.qsb') {
        $dump = & "$($chain.qtKit)/bin/qsb.exe" --dump $file.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $dump -notmatch 'MSL' -or $dump -notmatch 'HLSL' -or $dump -notmatch 'GLSL' -or $dump -notmatch 'SPIR-V') {
            throw 'Incomplete R10B cross-backend variants'
        }
        Save ($file.Name + '.txt') $dump
        $shaders += @{
            name = $file.Name
            sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
            targets = @('SPIR-V', 'GLSL330', 'GLSL ES300', 'HLSL50', 'MSL12')
            reflection = $file.Name + '.txt'
        }
    }
    if ($shaders.Count -ne 7) {
        throw 'Expected seven actual RHI shaders'
    }
    Save 'shader-variants.json' (@{
            qtVersion = $chain.qtVersion
            shaders = $shaders
        } | ConvertTo-Json -Depth 8)
    Run 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF } | Out-Null
    Run 'production-build.txt' { cmake --build --preset qt-release } | Out-Null
    Run 'production-ctest.txt' { ctest --preset qt-release --output-on-failure } | Out-Null
    Save 'summary.json' (@{
            round = 'R10B'
            configuration = 'Release'
            passed = $true
            qtVersion = $chain.qtVersion
            operatingSystem = [Environment]::OSVersion.VersionString
            device = $report.cases[0].metadata.device
            deviceSource = 'actual QRhi::driverInfo'
            fixtureSha256 = $report.fixtureSha256
            cases = 61
            reviewRegression = if ($ReuseReviewRegression) {
                'Preserved full 35-test pass; final RHI changes rerun all five affected tests including window policy'
            } else {
                'Full 35-test Review run'
            }
            sourceStage = 'Pre-AO/Bokeh/SMAA linear materials; original direct-material exceptions'
            meanEqualWeightLoss = $report.meanEqualWeightLoss
            quick3dMeanEqualWeightLoss = $report.quick3dMeanEqualWeightLoss
            residualCount = $report.residuals.Count
            allRoiParityPassed = ($report.residuals.Count -eq 0)
            maxProjectionPixels = $report.maxProjectionPixels
            productImplementationReady = $false
            atlasGpuUploadBytes = $null
            atlasGpuUploadStatus = 'R10C: unmeasured in this round'
            windowPolicy = 'Nonvisual tests hidden; exposed windows stay on bottom and do not accept focus'
            macos = '静态审计通过，实机待用户验证'
            linux = '静态审计通过，实机待用户验证'
        } | ConvertTo-Json -Depth 8)
    & ./tools/audit/audit-r10b.ps1
    if (-not $?) {
        throw 'R10B static audit failed'
    }
    Write-Output 'R10B Release verification passed; product replacement remains R10D.'
} finally {
    Pop-Location
}
