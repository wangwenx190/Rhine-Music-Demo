[CmdletBinding()]
param([switch]$SkipCalibration,[switch]$SkipWebCapture)
$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/materials'
$preview=Join-Path $repository 'docs/media/materials/qt'
New-Item -ItemType Directory -Force -Path $evidence,$preview | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Path,[string]$Text) {
    $Text=$Text.Replace($env:USERPROFILE.Replace('\','/'),'<USERPROFILE>').Replace($env:USERPROFILE,'<USERPROFILE>')
    $Text=$Text.Replace($repository.Replace('\','/'),'<REPOSITORY>').Replace($repository,'<REPOSITORY>')
    $Text=(($Text -split "`r?`n" | ForEach-Object {$_.TrimEnd()}) -join "`n").TrimEnd()+"`n"
    [IO.File]::WriteAllText($Path,$Text,$utf8)
}
function Invoke-Recorded([string]$Name,[scriptblock]$Command) {
    $output=& $Command 2>&1
    Save-PublicText (Join-Path $evidence $Name) ($output -join "`n")
    if ($LASTEXITCODE -ne 0) {throw "$Name failed"}
    return ($output -join "`n")
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    if (-not $SkipWebCapture) {Invoke-Recorded 'web-export.txt' {node tools/review/export-material-golden.mjs} | Out-Null}
    Invoke-Recorded 'material-data.txt' {node tools/assets/export-material-data.mjs} | Out-Null
    Invoke-Recorded 'release-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=ON} | Out-Null
    Invoke-Recorded 'release-build.txt' {cmake --build --preset qt-release} | Out-Null
    $chain=Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    if (-not $SkipCalibration) {
        $savedPath=$env:PATH
        $savedScale=$env:QT_SCALE_FACTOR
        $savedDpi=$env:QT_ENABLE_HIGHDPI_SCALING
        try {
            $env:PATH="$($chain.qtKit)/bin;$savedPath"
            $env:QT_ENABLE_HIGHDPI_SCALING='0'
            $env:QT_SCALE_FACTOR='1'
            Invoke-Recorded 'calibration.txt' {python tools/review/calibrate-materials.py} | Out-Null
        } finally {$env:PATH=$savedPath;$env:QT_SCALE_FACTOR=$savedScale;$env:QT_ENABLE_HIGHDPI_SCALING=$savedDpi}
        Invoke-Recorded 'release-build.txt' {cmake --build --preset qt-release} | Out-Null
    }
    $tests=Invoke-Recorded 'release-ctest.txt' {ctest --preset qt-release --output-on-failure}
    if ($tests -notmatch '100% tests passed(?:, 0 tests failed)? out of 17') {throw 'Expected seventeen Release suites'}
    $lint=Invoke-Recorded 'release-qmllint.txt' {cmake --build --preset qt-release --target Yalahoo_qmllint}
    if ($lint -match '(?m)^(Warning|Error):') {throw 'QML diagnostics remain'}
    Invoke-Recorded 'shader-bake.txt' {python tools/audit/bake-material-shaders.py} | Out-Null
    $run=Get-Content -Raw build/release/verification/materials/runtime.json | ConvertFrom-Json
    $fixtureHash=(Get-FileHash tests/fixtures/materials/single-case.json).Hash.ToLowerInvariant()
    if (-not $run.passed -or $run.dpr -ne 1 -or $run.graphicsApi -ne 4 -or $run.cases.Count -ne 99 -or $run.fixtureSha256 -ne $fixtureHash -or $run.maxProjectionPixels -gt 2) {throw 'Material GPU acceptance failed'}
    foreach ($case in $run.cases) {
        if (-not $case.passed -or -not $case.sourceParameterMatch -or $case.webProjectionPixels -gt 2 -or $case.foregroundPixels -lt 100) {throw 'Invalid material sample'}
        foreach ($region in @('paper','margin','spine','rim','background')) {
            $metric=$case.roi.$region
            if ($metric.meanAbsoluteRgb -gt 6 -or ($metric.ssimWindows -gt 0 -and $metric.ssim -lt 0.97)) {throw "Material ROI image gate failed: $($case.index)/$region"}
        }
        foreach ($name in @($case.screenshot,"diff-$($case.screenshot)")) {
            Copy-Item -LiteralPath "build/release/verification/materials/$name" -Destination (Join-Path $preview $name)
        }
    }
    $calibration=Get-Content -Raw (Join-Path $evidence 'calibration.json') | ConvertFrom-Json
    if ($calibration.fixtureSha256 -ne $fixtureHash -or $calibration.scans.Count -gt 18) {throw 'Calibration provenance mismatch'}
    Save-PublicText (Join-Path $evidence 'release-materials.json') (Get-Content -Raw build/release/verification/materials/runtime.json)
    Save-PublicText (Join-Path $evidence 'release-toolchain.json') (Get-Content -Raw build/release/toolchain.json)
    $roi=@{}
    foreach ($region in @('paper','margin','spine','rim','background')) {
        $samples=@($run.cases | Where-Object {$_.layer -eq 'all'})
        $ssimSamples=@($samples | Where-Object {$_.roi.$region.ssimWindows -gt 0})
        $meanSsim=if ($ssimSamples.Count) {($ssimSamples.roi.$region.ssim | Measure-Object -Average).Average} else {$null}
        $roi[$region]=@{meanAbsoluteRgb=($samples.roi.$region.meanAbsoluteRgb | Measure-Object -Average).Average;maxAbsoluteRgb=($samples.roi.$region.meanAbsoluteRgb | Measure-Object -Maximum).Maximum;meanSsim=$meanSsim}
    }
    $summary=@{round='R07';configuration='Release';qtVersion=$chain.qtVersion;ctest='17/17 passed';qmllint='passed';fixtureSha256=$fixtureHash;baseline=$run.baseline;gpuCases=90;diffuseProbes=9;graphicsApi=$run.graphicsApi;actualDpr=$run.dpr;maxProjectionPixels=$run.maxProjectionPixels;roi=$roi;calibrationFactors=$calibration.factors;operatingSystem=[Environment]::OSVersion.VersionString;graphicsAdapters=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)}
    Invoke-Recorded 'production-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=OFF} | Out-Null
    Invoke-Recorded 'production-build.txt' {cmake --build --preset qt-release} | Out-Null
    $tests=Invoke-Recorded 'production-ctest.txt' {ctest --preset qt-release --output-on-failure}
    if ($tests -notmatch '100% tests passed(?:, 0 tests failed)? out of 13') {throw 'Production regression failed'}
    $savedPath=$env:PATH
    try {
        $env:PATH="$($chain.qtKit)/bin;$savedPath"
        & ./build/release/bin/yalahoo.exe --verify-materials tests/fixtures/materials/single-case.json 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 1) {throw 'Production did not reject Review CLI'}
    } finally {$env:PATH=$savedPath}
    $summary.production=@{reviewEnabled=$false;ctest='13/13 passed';reviewCliExitCode=1}
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 16)
    Invoke-Recorded 'report.txt' {python tools/review/report-materials.py} | Out-Null
    Invoke-Recorded 'platform-audit.txt' {& ./tools/audit/audit-r07.ps1} | Out-Null
    Write-Output 'R07 Windows Release material verification passed.'
} finally {Pop-Location}
