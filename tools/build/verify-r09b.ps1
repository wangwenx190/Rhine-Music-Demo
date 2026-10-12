[CmdletBinding()]
param([switch]$SkipWebCapture,[string]$Python='python')
$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/postprocess'
$preview=Join-Path $repository 'docs/media/postprocess/qt'
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
    Save-PublicText $Name ($output -join "`n")
    if($LASTEXITCODE -ne 0){throw "$Name failed"}
    return ($output -join "`n")
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Invoke-Recorded 'lookup-export.txt' {node tools/assets/export-postprocess.mjs} | Out-Null
    Invoke-Recorded 'kernel-prepare.txt' {& $Python tools/assets/prepare-postprocess-shaders.py} | Out-Null
    Invoke-Recorded 'capture-prepare.txt' {& $Python tools/review/prepare-postprocess-capture.py} | Out-Null
    if(-not $SkipWebCapture){Invoke-Recorded 'web-export.txt' {node tools/review/export-postprocess-visual.mjs} | Out-Null}
    Invoke-Recorded 'release-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=ON} | Out-Null
    Invoke-Recorded 'release-build.txt' {cmake --build --preset qt-release} | Out-Null
    $tests=Invoke-Recorded 'release-ctest.txt' {ctest --preset qt-release --output-on-failure}
    if($tests -notmatch '100% tests passed'){throw 'Review regression failed'}
    $lint=Invoke-Recorded 'release-qmllint.txt' {cmake --build --preset qt-release --target Yalahoo_qmllint}
    if($lint -match '(?m)^(Warning|Error):'){throw 'QML diagnostics remain'}
    Invoke-Recorded 'shader-bake.txt' {& $Python tools/audit/bake-postprocess-shaders.py} | Out-Null
    $chain=Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $gpu=Get-Content -Raw build/release/verification/postprocess/gpu-checks.json | ConvertFrom-Json
    if(-not $gpu.passed -or $gpu.cases -ne 105){throw 'GPU gates failed'}
    foreach($dpr in @('1','1.5','2')){
        $r=Get-Content -Raw "build/release/verification/postprocess/dpr-$dpr/runtime.json" | ConvertFrom-Json
        if(-not $r.passed -or $r.dpr -ne [double]$dpr -or $r.graphicsApi -ne 4){throw 'Actual DPR/backend gate failed'}
        Save-PublicText "release-dpr-$dpr.json" (Get-Content -Raw "build/release/verification/postprocess/dpr-$dpr/runtime.json")
        foreach($c in $r.cases){foreach($name in @($c.screenshot,"diff-$($c.screenshot)")){Copy-Item -LiteralPath "build/release/verification/postprocess/dpr-$dpr/$name" -Destination (Join-Path $preview $name)}}
    }
    foreach($name in @('edge-1.png','edge-28.png','edge-91.png','paper-boundary.png')){Copy-Item -LiteralPath "build/release/verification/postprocess/$name" -Destination (Join-Path $preview $name)}
    Save-PublicText 'release-gpu-checks.json' (Get-Content -Raw build/release/verification/postprocess/gpu-checks.json)
    Save-PublicText 'release-render-quality-test.xml' (Get-Content -Raw build/release/verification/render-quality-test.xml)
    Save-PublicText 'release-toolchain.json' (Get-Content -Raw build/release/toolchain.json)
    Invoke-Recorded 'production-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=OFF} | Out-Null
    Invoke-Recorded 'production-build.txt' {cmake --build --preset qt-release} | Out-Null
    $production=Invoke-Recorded 'production-ctest.txt' {ctest --preset qt-release --output-on-failure}
    if($production -notmatch '100% tests passed'){throw 'Production regression failed'}
    $savedPath=$env:PATH
    try {
        $env:PATH="$($chain.qtKit)/bin;$savedPath"
        $p=Start-Process -FilePath './build/release/bin/yalahoo.exe' -ArgumentList @('--verify-runtime','build/release/verification/production-postprocess.json','--verify-shading') -Wait -PassThru -WindowStyle Hidden -RedirectStandardError 'build/r09b-production-stderr.txt' -RedirectStandardOutput 'build/r09b-production-stdout.txt'
        Save-PublicText 'production-shading.txt' ((Get-Content -Raw build/r09b-production-stdout.txt)+(Get-Content -Raw build/r09b-production-stderr.txt))
        if($p.ExitCode -ne 0){throw 'Production shading failed'}
        $shading=Get-Content -Raw build/release/verification/production-postprocess.json | ConvertFrom-Json
        if(-not $shading.passed -or -not $shading.shadingProbe){throw 'Production graph failed'}
        Save-PublicText 'production-shading.json' (Get-Content -Raw build/release/verification/production-postprocess.json)
        Copy-Item -LiteralPath build/release/verification/runtime.png -Destination (Join-Path $preview 'production.png')
        $rejected=Start-Process -FilePath './build/release/bin/yalahoo.exe' -ArgumentList @('--verify-materials','tests/fixtures/postprocess/visual-dpr-1.json') -Wait -PassThru -WindowStyle Hidden -RedirectStandardError 'build/r09b-production-review-stderr.txt'
        if($rejected.ExitCode -ne 1){throw 'Production accepted Review CLI'}
    } finally {$env:PATH=$savedPath}
    $summary=@{round='R09B';configuration='Release';qtVersion=$chain.qtVersion;baseline='57f4f845c20cce646a15dbf4008c2a162aaaf4e2';fixtureSha256=(Get-FileHash tests/fixtures/postprocess/visual.json).Hash.ToLowerInvariant();gpuCases=$gpu.cases;gpuPassed=$gpu.passed;actualDpr=@(1,1.5,2);graphicsApi=4;reviewCtest=$tests;productionCtest=$production;qmllint='passed';productionShadingPassed=$shading.passed;productionReviewCliExitCode=1;maxBackgroundRoiMae=$gpu.maxBackgroundRoiMae;maxRawBackgroundRoiMae=$gpu.maxRawBackgroundRoiMae;maxCompositeRoiMae=$gpu.maxCompositeRoiMae;minCompositeRoiSsim=$gpu.minCompositeRoiSsim;residualCount=$gpu.residuals.Count;operatingSystem=[Environment]::OSVersion.VersionString;graphicsAdapters=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)}
    Save-PublicText 'summary.json' ($summary|ConvertTo-Json -Depth 12)
    & ./tools/audit/audit-r09b.ps1
    if(-not $?){throw 'Static audit failed'}
    Write-Output 'R09B Release verification passed; explicit remaining render limitations belong to R10.'
} finally {Pop-Location}
