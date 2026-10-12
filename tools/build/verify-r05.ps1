[CmdletBinding()]
param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/motion'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Path, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}
Push-Location -LiteralPath $repository
try {
    $buildArgs = @{
        Preset = 'qt-release'
        Action = 'All'
    }
    if ($Clean) {
        $buildArgs.Clean = $true
    }
    $output = & (Join-Path $PSScriptRoot 'build.ps1') @buildArgs 2>&1
    Save-PublicText (Join-Path $evidence 'release-build.txt') ($output -join "`n")
    if (($output -join "`n") -notmatch '100% tests passed(?:, 0 tests failed)? out of 12') {
        throw 'Expected twelve passing CTest suites.'
    }
    $lint = & cmake --build --preset qt-release --target Yalahoo_qmllint 2>&1
    if ($LASTEXITCODE -ne 0 -or ($lint -join "`n") -match '(?m)^(Warning|Error):') {
        throw 'QML lint failed or produced unresolved diagnostics.'
    }
    Save-PublicText (Join-Path $evidence 'release-qmllint.txt') ($lint -join "`n")
    $build = Join-Path $repository 'build/release'
    foreach ($file in @('motion-test.xml', 'motion-errors.json', 'runtime-motion.json')) {
        Save-PublicText (Join-Path $evidence "release-$file") (Get-Content -Raw -Encoding UTF8 (Join-Path $build "verification/$file"))
    }
    Save-PublicText (Join-Path $evidence 'release-toolchain.json') (Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json'))
    $xml = [xml](Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/motion-test.xml'))
    if ([int]$xml.testsuite.failures -or [int]$xml.testsuite.errors -or [int]$xml.testsuite.skipped -or $xml.SelectNodes('//system-err').Count) {
        throw 'Motion tests failed, skipped or produced diagnostics.'
    }
    $errors = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/motion-errors.json') | ConvertFrom-Json
    if ($errors.r01Samples -ne 694 -or $errors.extendedSamples -ne 694 -or
        $errors.maxPositionError -gt 1e-6 -or $errors.maxVelocityError -gt 1e-6 -or $errors.maxAccelerationError -gt 1e-6) {
        throw 'Mathematical gate failed.'
    }
    $runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/runtime-motion.json') | ConvertFrom-Json
    if (-not $runtime.passed -or -not $runtime.motion.passed -or $runtime.visualFrameCount -le 1 -or $runtime.sourceOpened) {
        throw 'Production frame source runtime failed.'
    }
    $preview = Join-Path $repository 'docs/media/motion'
    New-Item -ItemType Directory -Force -Path $preview | Out-Null
    Copy-Item -LiteralPath (Join-Path $build 'verification/runtime-motion.png') -Destination (Join-Path $preview 'release-runtime.png')
    $oracle = Get-Content -Raw -Encoding UTF8 'tests/fixtures/motion/kernel-golden.json' | ConvertFrom-Json
    if ((Get-FileHash 'docs/contracts/motion-golden.json').Hash.ToLowerInvariant() -ne $oracle.r01GoldenSha256) {
        throw 'R01 oracle changed.'
    }
    foreach ($source in $oracle.sources) {
        $frozen = & git show "$($oracle.baseline):$($source.path)"
        if ($LASTEXITCODE -ne 0) {
            throw 'Frozen source unavailable.'
        }
        $bytes = [Text.Encoding]::UTF8.GetBytes(($frozen -join "`n") + "`n")
        if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.sha256) {
            throw "Frozen source changed: $($source.path)"
        }
    }
    $chain = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
    $summary = @{
        round = 'R05'
        configuration = 'Release'
        qtVersion = $chain.qtVersion

        operatingSystem = [Environment]::OSVersion.VersionString
        ctest = '12/12 passed'
        qmllint = 'passed'

        r01Samples = $errors.r01Samples
        extendedSamples = $errors.extendedSamples
        fps = $errors.fps

        maxPositionError = $errors.maxPositionError
        maxVelocityError = $errors.maxVelocityError
        maxAccelerationError = $errors.maxAccelerationError

        baseline = $oracle.baseline
        sourceHashes = $oracle.sources

        kernelGoldenSha256 = (Get-FileHash 'tests/fixtures/motion/kernel-golden.json').Hash.ToLowerInvariant()

        visualTickSources = 1
        scope = 'Motion kernels and production FrameAnimation integration; no R06 geometry/camera, R13 column identity/layout or R20 opening UI implementation.'

        validation = 'Frozen formulas; x/v/a joins, reverse, speed, negative/long frames, rebase, spring-to-guided, same-target, reduced completion/revision and reentrant completion. Actual Windows D3D11/QML frame source, hide/minimize, resumed zero delta, mouse and empty media source.'
    }
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 20)
    Write-Output 'R05 Release motion verification passed.'
} finally {
    Pop-Location
}
