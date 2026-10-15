[CmdletBinding()]
param([switch]$SkipWebCapture)
$ErrorActionPreference = 'Stop'
throw 'Historical image verification retired. Use source math, rhi-postprocess-gpu and the coverage map in docs/contracts/source-fidelity-review.md.'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/occlusion'
$preview = Join-Path $repository 'docs/media/occlusion/qt'
New-Item -ItemType Directory -Force -Path $evidence, $preview | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Name, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    [IO.File]::WriteAllText((Join-Path $evidence $Name), $Text, $utf8)
}
function Invoke-Recorded([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1
    Save-PublicText $Name ($output -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed"
    }
    return ($output -join "`n")
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Invoke-Recorded 'ssao-data.txt' { node tools/assets/export-ssao-data.mjs } | Out-Null
    Invoke-Recorded 'quality-oracle.txt' { node tools/review/export-quality-golden.mjs } | Out-Null
    Invoke-Recorded 'capture-harness.txt' { python tools/review/prepare-occlusion-capture.py } | Out-Null
    if (-not $SkipWebCapture) {
        Invoke-Recorded 'web-capture.txt' { node tools/review/export-occlusion-visual.mjs } | Out-Null
    }
    Invoke-Recorded 'shared-shaders.txt' { python tools/assets/expand-lighting-shaders.py --check } | Out-Null
    Invoke-Recorded 'release-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON } | Out-Null
    Invoke-Recorded 'release-build.txt' { cmake --build --preset qt-release } | Out-Null
    $tests = Invoke-Recorded 'release-ctest.txt' { ctest --preset qt-release --output-on-failure }
    if ($tests -notmatch '100% tests passed') {
        throw 'Review regression failed'
    }
    $lint = Invoke-Recorded 'release-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    if ($lint -match '(?m)^(Warning|Error):') {
        throw 'QML diagnostics remain'
    }
    Invoke-Recorded 'shader-bake.txt' { python tools/audit/bake-occlusion-shaders.py } | Out-Null
    Invoke-Recorded 'material-shader-bake.txt' { python tools/audit/bake-material-shaders.py --occlusion } | Out-Null
    $run = Get-Content -Raw build/release/verification/occlusion/runtime.json | ConvertFrom-Json
    $gpu = Get-Content -Raw build/release/verification/occlusion/gpu-checks.json | ConvertFrom-Json
    if (-not $run.passed -or -not $gpu.passed -or $run.cases.Count -ne 88 -or $run.graphicsApi -ne 4 -or $run.dpr -ne 1) {
        throw 'GPU gates failed'
    }
    foreach ($case in $run.cases) {
        foreach ($name in @($case.screenshot, "diff-$($case.screenshot)")) {
            Copy-Item -LiteralPath "build/release/verification/occlusion/$name" -Destination (Join-Path $preview $name)
        }
    }
    foreach ($pair in @(@('release-runtime.json', 'build/release/verification/occlusion/runtime.json'), @('release-gpu-checks.json', 'build/release/verification/occlusion/gpu-checks.json'), @('release-render-quality-test.xml', 'build/release/verification/render-quality-test.xml'), @('release-toolchain.json', 'build/release/toolchain.json'))) {
        Save-PublicText $pair[0] (Get-Content -Raw $pair[1])
    }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    Invoke-Recorded 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF } | Out-Null
    Invoke-Recorded 'production-build.txt' { cmake --build --preset qt-release } | Out-Null
    $production = Invoke-Recorded 'production-ctest.txt' { ctest --preset qt-release --output-on-failure }
    if ($production -notmatch '100% tests passed') {
        throw 'Production regression failed'
    }
    $savedPath = $env:PATH
    try {
        $env:PATH = "$($chain.qtKit)/bin;$savedPath"
        $process = Start-Process -FilePath './build/release/bin/yalahoo.exe' -WorkingDirectory $repository -ArgumentList @('--verify-runtime', 'build/release/verification/production-shading.json', '--verify-shading') -Wait -PassThru -WindowStyle Hidden -RedirectStandardError 'build/production-shading-stderr.txt' -RedirectStandardOutput 'build/production-shading-stdout.txt'
        Save-PublicText 'production-shading.txt' ((Get-Content -Raw build/production-shading-stdout.txt) + (Get-Content -Raw build/production-shading-stderr.txt))
        if ($process.ExitCode -ne 0) {
            throw 'Production shading process failed'
        }
        $shading = Get-Content -Raw build/release/verification/production-shading.json | ConvertFrom-Json
        if (-not $shading.passed -or -not $shading.shadingProbe) {
            throw 'Production shading failed'
        }
        Save-PublicText 'production-shading.json' (Get-Content -Raw build/release/verification/production-shading.json)
        Copy-Item build/release/verification/runtime.png (Join-Path $preview 'production.png')
        $rejected = Start-Process -FilePath './build/release/bin/yalahoo.exe' -WorkingDirectory $repository -ArgumentList @('--verify-materials', 'tests/fixtures/occlusion/visual.json') -Wait -PassThru -WindowStyle Hidden -RedirectStandardError 'build/production-review-stderr.txt' -RedirectStandardOutput 'build/production-review-stdout.txt'
        if ($rejected.ExitCode -ne 1) {
            throw 'Production accepted Review CLI'
        }
    } finally {
        $env:PATH = $savedPath
    }
    $reviewCount = [int]([regex]::Match($tests, '100% tests passed[^\r\n]*out of (\d+)').Groups[1].Value)
    $productionCount = [int]([regex]::Match($production, '100% tests passed[^\r\n]*out of (\d+)').Groups[1].Value)
    if ($reviewCount -lt 22 -or $productionCount -lt 15) {
        throw 'Regression coverage missing'
    }
    $summary = @{
        round = 'R09'
        configuration = 'Release'
        qtVersion = $chain.qtVersion
        baseline = $run.baseline
        fixtureSha256 = (Get-FileHash tests/fixtures/occlusion/visual.json).Hash.ToLowerInvariant()
        kernelSha256 = (Get-FileHash tests/fixtures/occlusion/kernel.json).Hash.ToLowerInvariant()
        qualitySha256 = (Get-FileHash tests/fixtures/occlusion/quality.json).Hash.ToLowerInvariant()
        gpuCases = 88
        graphicsApi = $run.graphicsApi
        actualDpr = $run.dpr
        maxProjectionPixels = $run.maxProjectionPixels
        gpu = $gpu
        ctest = "$reviewCount/$reviewCount passed"
        qmllint = 'passed'
        production = @{
            reviewEnabled = $false
            ctest = "$productionCount/$productionCount passed"
            reviewCliExitCode = 1
            shadingPassed = $shading.passed
        }
        operatingSystem = [Environment]::OSVersion.VersionString
        graphicsAdapters = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    }
    Save-PublicText 'summary.json' ($summary | ConvertTo-Json -Depth 16)
    & ./tools/audit/audit-r09.ps1
    if (-not $?) {
        throw 'Static audit failed'
    }
    Write-Output 'R09 Release verification passed.'
} finally {
    Pop-Location
}
