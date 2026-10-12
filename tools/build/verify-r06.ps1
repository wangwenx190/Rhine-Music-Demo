[CmdletBinding()]
param([switch]$CollectOnly)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/scene'
$preview = Join-Path $repository 'docs/media/scene'
New-Item -ItemType Directory -Force -Path $evidence, $preview | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Path, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    & node tools/review/export-scene-golden.mjs
    if ($LASTEXITCODE -ne 0) {
        throw 'Frozen oracle export failed.'
    }
    $oracleHash = (Get-FileHash tests/fixtures/scene/camera-golden.json).Hash.ToLowerInvariant()
    if (-not $CollectOnly) {
        $configure = & cmake --preset qt-release -DYALAHOO_REVIEW=ON 2>&1
        Save-PublicText (Join-Path $evidence 'release-configure.txt') ($configure -join "`n")
        if ($LASTEXITCODE -ne 0) {
            throw 'Configure failed.'
        }
        $build = & cmake --build --preset qt-release 2>&1
        Save-PublicText (Join-Path $evidence 'release-build.txt') ($build -join "`n")
        if ($LASTEXITCODE -ne 0) {
            throw 'Build failed.'
        }
        $tests = & ctest --preset qt-release --output-on-failure 2>&1
        Save-PublicText (Join-Path $evidence 'release-ctest.txt') ($tests -join "`n")
        if ($LASTEXITCODE -ne 0) {
            throw 'Release suites failed.'
        }
    }
    $tests = Get-Content -Raw (Join-Path $evidence 'release-ctest.txt')
    if ($tests -notmatch '100% tests passed(?:, 0 tests failed)? out of 16' -or $tests -match '\*\*\*Failed') {
        throw 'Release suites failed.'
    }
    $lint = & cmake --build --preset qt-release --target Yalahoo_qmllint 2>&1
    Save-PublicText (Join-Path $evidence 'release-qmllint.txt') ($lint -join "`n")
    if ($LASTEXITCODE -ne 0 -or ($lint -join "`n") -match '(?m)^(Warning|Error):') {
        throw 'QML diagnostics remain.'
    }
    foreach ($file in @('scene-test.xml', 'scene-errors.json', 'runtime.json', 'runtime-motion.json', 'runtime-library.json')) {
        Save-PublicText (Join-Path $evidence "release-$file") (Get-Content -Raw -Encoding UTF8 "build/release/verification/$file")
    }
    $xml = [xml](Get-Content -Raw build/release/verification/scene-test.xml)
    if ([int]$xml.testsuite.failures -or [int]$xml.testsuite.errors -or [int]$xml.testsuite.skipped -or $xml.SelectNodes('//system-err').Count) {
        throw 'Scene unit diagnostic/failure.'
    }
    $errors = Get-Content -Raw build/release/verification/scene-errors.json | ConvertFrom-Json
    if ($errors.maxProjectionPixels -gt 2 -or $errors.maxTargetError -gt 1e-6 -or $errors.maxCameraPositionAimError -gt 1e-6 -or $errors.maxFovError -gt 1e-6) {
        throw 'Numerical gate failed.'
    }
    $runtime = @()
    foreach ($dpr in @('1', '1.5', '2')) {
        $source = "build/release/verification/scene-dpr-$dpr"
        $report = Get-Content -Raw "$source/runtime.json" | ConvertFrom-Json
        if (-not $report.passed -or $report.fixtureSha256 -ne $oracleHash -or $report.graphicsApi -ne 4 -or $report.cases.Count -ne 409 -or $report.maxWebPixels -gt 2) {
            throw "DPR $dpr runtime failed."
        }
        $ratios = @($report.cases | Select-Object -ExpandProperty dpr -Unique)
        if ($ratios.Count -ne 1 -or [Math]::Abs($ratios[0] - [double]$dpr) -gt 1e-6) {
            throw "Requested DPR $dpr differs from actual $ratios"
        }
        Save-PublicText (Join-Path $evidence "release-dpr-$dpr.json") (Get-Content -Raw "$source/runtime.json")
        $destination = Join-Path $preview "dpr-$dpr"
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        foreach ($case in $report.cases) {
            if ($case.screenshot) {
                if ($case.imageWidth -ne [Math]::Round($case.width * $case.dpr, [MidpointRounding]::AwayFromZero) -or $case.imageHeight -ne [Math]::Round($case.height * $case.dpr, [MidpointRounding]::AwayFromZero)) {
                    throw 'PNG/client/DPR mismatch.'
                }
                Copy-Item -LiteralPath (Join-Path $source $case.screenshot) -Destination (Join-Path $destination $case.screenshot)
            }
        }
        $runtime += @{
            dpr = $ratios[0]
            staticTargets = 48
            dynamicSamples = 361
            maxWebPixels = $report.maxWebPixels
            maxQtCpuPixels = $report.maxQtCpuPixels
        }
    }
    $oracle = Get-Content -Raw tests/fixtures/scene/camera-golden.json | ConvertFrom-Json
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    Save-PublicText (Join-Path $evidence 'release-toolchain.json') (Get-Content -Raw build/release/toolchain.json)
    $summary = @{
        round = 'R06'
        configuration = 'Release'
        qtVersion = $chain.qtVersion
        ctest = '16/16 passed'
        qmllint = 'passed'

        operatingSystem = [Environment]::OSVersion.VersionString
        baseline = $oracle.baseline
        sourceHashes = $oracle.sources

        fixtureSha256 = (Get-FileHash tests/fixtures/scene/camera-golden.json).Hash.ToLowerInvariant()

        errors = $errors
        runtime = $runtime
        scope = 'Geometry/layout/projection and interrupted camera only; unlit mesh, no material/light/array/presentation implementation.'
    }
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 20)
    # Leave the ordinary local executable without deterministic Review entry points.
    $production = & cmake --preset qt-release -DYALAHOO_REVIEW=OFF 2>&1
    Save-PublicText (Join-Path $evidence 'production-configure.txt') ($production -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw 'Production configure failed.'
    }
    $production = & cmake --build --preset qt-release 2>&1
    Save-PublicText (Join-Path $evidence 'production-build.txt') ($production -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw 'Production build failed.'
    }
    $production = & ctest --preset qt-release --output-on-failure 2>&1
    Save-PublicText (Join-Path $evidence 'production-ctest.txt') ($production -join "`n")
    if ($LASTEXITCODE -ne 0 -or ($production -join "`n") -notmatch '100% tests passed(?:, 0 tests failed)? out of 13') {
        throw 'Production suites failed.'
    }
    $savedPath = $env:PATH
    try {
        $env:PATH = "$($chain.qtKit)/bin;$savedPath"
        $process = Start-Process -FilePath (Join-Path $repository 'build/release/bin/yalahoo.exe') -ArgumentList '--verify-scene' -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 1) {
            throw 'Review CLI was not rejected by ordinary executable.'
        }
    } finally {
        $env:PATH = $savedPath
    }
    $summary.production = @{
        reviewEnabled = $false
        ctest = '13/13 passed'
        reviewCliExitCode = $process.ExitCode
    }
    $summary.graphicsAdapters = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 20)
    Write-Output 'R06 Windows Release geometry/camera verification passed.'
} finally {
    Pop-Location
}
