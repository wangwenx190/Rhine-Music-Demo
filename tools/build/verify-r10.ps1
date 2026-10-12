[CmdletBinding()]
param([switch]$SkipWebCapture, [switch]$ResumeAfterRegression)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/render-backend'
$preview = Join-Path $repository 'docs/media/render-backend/qt'
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
    if (-not $SkipWebCapture) {
        Invoke-Recorded 'web-export.txt' { node tools/review/export-render-backend.mjs } | Out-Null
    }
    Invoke-Recorded 'release-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON } | Out-Null
    Invoke-Recorded 'release-build.txt' { cmake --build --preset qt-release } | Out-Null
    if ($ResumeAfterRegression) {
        # Resume only the isolated R10 harness failure; every other Review test
        # must already have passed in the preserved full regression transcript.
        $prior = Get-Content -Raw (Join-Path $evidence 'release-ctest.txt')
        if ($prior -notmatch '1 tests failed out of 29' -or $prior -notmatch '5 - render-backend-gpu-audit \(Failed\)' -or ([regex]::Matches($prior, '\*\*\*Failed')).Count -ne 1) {
            throw 'Resume requires a single isolated R10 audit failure'
        }
        $tests = Invoke-Recorded 'release-ctest-retry.txt' { ctest --preset qt-release -R 'runtime-render-backend|render-backend-gpu-audit' --output-on-failure }
    } else {
        $tests = Invoke-Recorded 'release-ctest.txt' { ctest --preset qt-release --output-on-failure }
    }
    if ($tests -notmatch '100% tests passed') {
        throw 'Review regression failed'
    }
    $lint = Invoke-Recorded 'release-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    if ($lint -match '(?m)^(Warning|Error):') {
        throw 'QML diagnostics remain'
    }
    Invoke-Recorded 'shader-bake.txt' { node tools/audit/bake-render-probe.mjs } | Out-Null
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $gpu = Get-Content -Raw build/release/verification/render-backend/gpu-checks.json | ConvertFrom-Json
    if (-not $gpu.passed -or $gpu.selectedBackend -ne 'rhi' -or $gpu.cases -ne 70) {
        throw 'R10 witness gates failed'
    }
    Save-PublicText 'release-runtime.json' (Get-Content -Raw build/release/verification/render-backend/runtime.json)
    Save-PublicText 'release-gpu-checks.json' (Get-Content -Raw build/release/verification/render-backend/gpu-checks.json)
    Save-PublicText 'release-toolchain.json' (Get-Content -Raw build/release/toolchain.json)
    foreach ($file in Get-ChildItem -LiteralPath build/release/verification/render-backend -Filter '*.png') {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $preview $file.Name)
    }
    Invoke-Recorded 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF } | Out-Null
    Invoke-Recorded 'production-build.txt' { cmake --build --preset qt-release } | Out-Null
    $production = Invoke-Recorded 'production-ctest.txt' { ctest --preset qt-release --output-on-failure }
    if ($production -notmatch '100% tests passed') {
        throw 'Production regression failed'
    }
    $savedPath = $env:PATH
    try {
        $env:PATH = "$($chain.qtKit)/bin;$savedPath"
        # Wait for the GPU executable; its verification flags keep exposed windows on bottom/no-focus.
        Invoke-Recorded 'production-shading.txt' { & ./build/release/bin/yalahoo.exe --verify-runtime build/release/verification/production-r10.json --verify-shading | Out-String } | Out-Null
        $shading = Get-Content -Raw build/release/verification/production-r10.json | ConvertFrom-Json
        if (-not $shading.passed) {
            throw 'Production bootstrap failed'
        }
        Save-PublicText 'production-shading.json' (Get-Content -Raw build/release/verification/production-r10.json)
        Copy-Item -LiteralPath build/release/verification/runtime.png -Destination (Join-Path $preview 'production.png')
        $rejected = & ./build/release/bin/yalahoo.exe --verify-materials tests/fixtures/render-backend/visual.json 2>&1 | Out-String
        if ($LASTEXITCODE -ne 1) {
            throw 'Production accepted Review CLI'
        }
        Save-PublicText 'production-review-rejected.txt' $rejected
    } finally {
        $env:PATH = $savedPath
    }
    $summary = @{
        round = 'R10'
        configuration = 'Release'
        qtVersion = $chain.qtVersion
        baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2'
        fixtureSha256 = (Get-FileHash tests/fixtures/render-backend/visual.json).Hash.ToLowerInvariant()
        gpuPassed = $gpu.passed
        gpuCases = $gpu.cases
        parityPassed = $false
        selectedBackend = 'rhi'
        implementationReady = $false
        activeImplementation = 'quick3d-bootstrap'
        maxProjectionPixels = $gpu.maxProjectionPixels
        residualCount = $gpu.residuals.Count
        atlasGpuTransferBytes = $null
        qmllint = 'passed'
        productionPassed = $shading.passed
        productionReviewCliExitCode = 1
        operatingSystem = [Environment]::OSVersion.VersionString
        graphicsAdapters = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    }
    Save-PublicText 'summary.json' ($summary | ConvertTo-Json -Depth 10)
    Invoke-Recorded 'evidence-index.txt' { node tools/review/index-render-backend.mjs } | Out-Null
    & ./tools/audit/audit-r10.ps1
    if (-not $?) {
        throw 'Static audit failed'
    }
    Write-Output 'R10 Release witnesses passed; fixed RHI destination awaits R10A-D.'
} finally {
    Pop-Location
}
