[CmdletBinding()]
param([switch]$SkipWebCapture)
$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/lighting'
$preview=Join-Path $repository 'docs/media/lighting/qt'
New-Item -ItemType Directory -Force -Path $evidence,$preview | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Name,[string]$Text) {
    $Text=$Text.Replace($env:USERPROFILE.Replace('\','/'),'<USERPROFILE>').Replace($env:USERPROFILE,'<USERPROFILE>')
    $Text=$Text.Replace($repository.Replace('\','/'),'<REPOSITORY>').Replace($repository,'<REPOSITORY>')
    $Text=(($Text -split "`r?`n" | ForEach-Object {$_.TrimEnd()}) -join "`n").TrimEnd()+"`n"
    [IO.File]::WriteAllText((Join-Path $evidence $Name),$Text,$utf8)
}
function Invoke-Recorded([string]$Name,[scriptblock]$Command) {
    $output=& $Command 2>&1
    if ($LASTEXITCODE -ne 0) {Save-PublicText $Name ($output -join "`n");throw "$Name failed"}
    Save-PublicText $Name ($output -join "`n")
    return ($output -join "`n")
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Invoke-Recorded 'golden-export.txt' {node tools/review/export-lighting-golden.mjs} | Out-Null
    if (-not $SkipWebCapture) {Invoke-Recorded 'web-export.txt' {node tools/review/export-lighting-visual.mjs} | Out-Null}
    Invoke-Recorded 'lighting-data.txt' {node tools/assets/export-lighting-data.mjs} | Out-Null
    Invoke-Recorded 'shared-shaders.txt' {python tools/assets/expand-lighting-shaders.py --check} | Out-Null
    Invoke-Recorded 'material-data.txt' {node tools/assets/export-material-data.mjs} | Out-Null
    Invoke-Recorded 'release-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=ON} | Out-Null
    Invoke-Recorded 'release-build.txt' {cmake --build --preset qt-release} | Out-Null
    $tests=Invoke-Recorded 'release-ctest.txt' {ctest --preset qt-release --output-on-failure}
    if ($tests -notmatch '100% tests passed') {throw 'Release regression failed'}
    $lint=Invoke-Recorded 'release-qmllint.txt' {cmake --build --preset qt-release --target Yalahoo_qmllint}
    if ($lint -match '(?m)^(Warning|Error):') {throw 'QML diagnostics remain'}
    Invoke-Recorded 'shader-bake.txt' {python tools/audit/bake-material-shaders.py --lighting} | Out-Null
    $run=Get-Content -Raw build/release/verification/lighting/runtime.json | ConvertFrom-Json
    $fixtureHash=(Get-FileHash tests/fixtures/lighting/visual.json).Hash.ToLowerInvariant()
    if (-not $run.passed -or $run.dpr -ne 1 -or $run.graphicsApi -ne 4 -or $run.cases.Count -ne 48 -or $run.fixtureSha256 -ne $fixtureHash -or $run.maxProjectionPixels -gt 2) {throw 'Lighting GPU gate failed'}
    foreach ($case in $run.cases) {
        if (-not $case.passed -or -not $case.sourceParameterMatch -or $case.webProjectionPixels -gt 2 -or $case.foregroundPixels -lt 100) {throw 'Invalid light sample'}
        foreach ($region in @('paper','margin','spine','rim','background')) {
            $metric=$case.roi.$region
            if ($metric.pixels -le 0 -or $metric.meanAbsoluteRgb -gt 6 -or ($metric.ssimWindows -gt 0 -and $metric.ssim -lt .97)) {throw "Lighting ROI gate: $($case.index)/$region"}
        }
        foreach ($name in @($case.screenshot,"diff-$($case.screenshot)")) {Copy-Item -LiteralPath "build/release/verification/lighting/$name" -Destination (Join-Path $preview $name)}
    }
    $errors=Get-Content -Raw build/release/verification/lighting-errors.json | ConvertFrom-Json
    if ($errors.samples -ne 924 -or $errors.fields -ne 392 -or $errors.trajectoryError -gt 1e-6 -or $errors.paletteError -gt 1e-6 -or $errors.fieldError -gt 1e-12) {throw 'Frozen lighting oracle gate failed'}
    foreach ($pair in @(@('release-runtime.json','build/release/verification/lighting/runtime.json'),@('release-errors.json','build/release/verification/lighting-errors.json'),@('release-lighting-test.xml','build/release/verification/lighting-test.xml'),@('release-toolchain.json','build/release/toolchain.json'))) {
        Save-PublicText $pair[0] (Get-Content -Raw $pair[1])
    }
    $roi=@{}
    foreach ($region in @('paper','margin','spine','rim','background')) {
        $metric=@($run.cases.roi.$region);$withSsim=@($metric | Where-Object {$_.ssimWindows -gt 0})
        $roi[$region]=@{maxMeanAbsoluteRgb=($metric.meanAbsoluteRgb | Measure-Object -Maximum).Maximum;minSsim=($withSsim.ssim | Measure-Object -Minimum).Minimum}
    }
    $chain=Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    Invoke-Recorded 'production-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=OFF} | Out-Null
    Invoke-Recorded 'production-build.txt' {cmake --build --preset qt-release} | Out-Null
    $production=Invoke-Recorded 'production-ctest.txt' {ctest --preset qt-release --output-on-failure}
    if ($production -notmatch '100% tests passed') {throw 'Production regression failed'}
    $savedPath=$env:PATH
    try {
        $env:PATH="$($chain.qtKit)/bin;$savedPath"
        & ./build/release/bin/yalahoo.exe --verify-materials tests/fixtures/lighting/visual.json 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 1) {throw 'Production accepted Review CLI'}
    } finally {$env:PATH=$savedPath}
    $reviewCount=[int]([regex]::Match($tests,'100% tests passed[^\r\n]*out of (\d+)').Groups[1].Value)
    $productionCount=[int]([regex]::Match($production,'100% tests passed[^\r\n]*out of (\d+)').Groups[1].Value)
    if ($reviewCount -lt 19 -or $productionCount -lt 14) {throw 'Required regression suite coverage missing'}
    $summary=@{round='R08' ;configuration='Release';qtVersion=$chain.qtVersion;baseline=$run.baseline;fixtureSha256=$fixtureHash;goldenSha256=(Get-FileHash tests/fixtures/lighting/golden.json).Hash.ToLowerInvariant();gpuCases=48;graphicsApi=$run.graphicsApi;actualDpr=$run.dpr;maxProjectionPixels=$run.maxProjectionPixels;roi=$roi;errors=$errors;ctest="$reviewCount/$reviewCount passed";qmllint='passed';production=@{reviewEnabled=$false;ctest="$productionCount/$productionCount passed";reviewCliExitCode=1};operatingSystem=[Environment]::OSVersion.VersionString;graphicsAdapters=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion);lightUnitFactors=@{key=1;hemisphere=1;spot=1;area=1;method='Original radiometric units in custom GLSL; no built-in Qt lamp brightness conversion'}}
    Save-PublicText 'summary.json' ($summary | ConvertTo-Json -Depth 16)
    & ./tools/audit/audit-r08.ps1
    if (-not $?) {throw 'Static platform audit failed'}
    Invoke-Recorded 'report.txt' {python tools/review/report-lighting.py} | Out-Null
    Write-Output 'R08 Windows Release lighting verification passed.'
} finally {Pop-Location}
