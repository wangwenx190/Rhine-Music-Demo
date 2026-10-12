[CmdletBinding()]
param([switch]$ReuseReviewRegression)
$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/rhi-opaque'
$media=Join-Path $repository 'docs/media/rhi-opaque'
New-Item -ItemType Directory -Force -Path $evidence,$media | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Save([string]$Name,[string]$Text) {
    $Text=$Text.Replace($env:USERPROFILE.Replace('\','/'),'<USERPROFILE>').Replace($env:USERPROFILE,'<USERPROFILE>')
    $Text=$Text.Replace($repository.Replace('\','/'),'<REPOSITORY>').Replace($repository,'<REPOSITORY>')
    $Text=(($Text -split "`r?`n"|ForEach-Object {$_.TrimEnd()}) -join "`n").TrimEnd()+"`n"
    [IO.File]::WriteAllText((Join-Path $evidence $Name),$Text,$utf8)
}
function Run([string]$Name,[scriptblock]$Command) {
    $text=& $Command 2>&1;Save $Name ($text -join "`n")
    if($LASTEXITCODE -ne 0){throw "$Name failed"}
    return ($text -join "`n")
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    & ./tools/assets/prepare-rhi-shaders.ps1
    Run 'release-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=ON} | Out-Null
    Run 'release-build.txt' {cmake --build --preset qt-release} | Out-Null
    if($ReuseReviewRegression) {
        # An already completed full run may be retained; still re-run the changed RHI subsystem.
        $prior=Get-Content -Raw (Join-Path $evidence 'release-ctest.txt')
        if(([regex]::Matches($prior,'(?m)^Test Pass(?:ed\.| Reason:)')).Count -ne 30 -or $prior -match 'Test Failed\.'){throw 'Reuse requires preserved complete 30-test Review LastTest.log'}
        Run 'release-rhi-ctest.txt' {ctest --preset qt-release -R '^rhi-opaque-gpu$' --output-on-failure} | Out-Null
    } else {Run 'release-ctest.txt' {ctest --preset qt-release --output-on-failure} | Out-Null}
    $lint=Run 'release-qmllint.txt' {cmake --build --preset qt-release --target Yalahoo_qmllint}
    if($lint -match '(?m)^(Warning|Error):'){throw 'QML diagnostics remain'}
    $report=Get-Content -Raw build/release/verification/rhi/runtime.json|ConvertFrom-Json
    if(-not $report.passed -or $report.cases.Count -ne 36 -or $report.resize.Count -ne 12 -or $report.resizeBurst -ne 30 -or $report.shadows.Count -ne 6 -or $report.maxCornerProjectionPixels -gt 2){throw 'Incomplete RHI GPU witnesses'}
    Save 'release-runtime.json' (Get-Content -Raw build/release/verification/rhi/runtime.json)
    Save 'release-toolchain.json' (Get-Content -Raw build/release/toolchain.json)
    foreach($file in Get-ChildItem -LiteralPath build/release/verification/rhi -Filter '*.png'){Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $media $file.Name)}
    $chain=Get-Content -Raw build/release/toolchain.json|ConvertFrom-Json
    $shaders=@()
    foreach($file in Get-ChildItem -LiteralPath build/release/src/render/rhi/.qsb -Filter '*.qsb') {
        $dump=& "$($chain.qtKit)/bin/qsb.exe" --dump $file.FullName 2>&1 | Out-String
        if($LASTEXITCODE -ne 0 -or $dump -notmatch 'MSL' -or $dump -notmatch 'HLSL' -or $dump -notmatch 'GLSL' -or $dump -notmatch 'SPIR-V'){throw 'Incomplete cross-backend qsb variants'}
        Save ($file.Name+'.txt') $dump
        $shaders+=@{name=$file.Name;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant();targets=@('SPIR-V','GLSL330','GLSL ES300','HLSL50','MSL12');reflection=$file.Name+'.txt'}
    }
    if($shaders.Count -ne 6){throw 'Expected six real RHI shaders'}
    Save 'shader-variants.json' (@{qtVersion=$chain.qtVersion;shaders=$shaders}|ConvertTo-Json -Depth 8)
    # Changed source and copied source assets are hashed independently of binary evidence.
    $paths=@('resources/models/music-case-v040.glb','resources/environment/room-pmrem.rgba32f','resources/shaders/album-paper.frag','resources/shaders/album-paper.vert',
        'legacy-web/src/scene.ts','legacy-web/src/music-model.ts','legacy-web/package-lock.json',
        'legacy-web/node_modules/three/src/renderers/webgl/WebGLShadowMap.js','legacy-web/node_modules/three/src/renderers/shaders/ShaderChunk/shadowmap_pars_fragment.glsl.js')
    $entries=@();foreach($path in $paths){$entries+=@{source=$path;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}}
    $generated=@();foreach($file in Get-ChildItem -LiteralPath resources/shaders/rhi -File){$generated+=@{path='resources/shaders/rhi/'+$file.Name;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()}}
    $manifest=@{baseline='57f4f845c20cce646a15dbf4008c2a162aaaf4e2';sources=$entries;shaders=$generated;sharedMath='prepare-rhi-shaders.ps1 adapts R07 uniforms only';license='resources/licenses/three-LICENSE; resources/licenses/materials-NOTICE.md';geometry='Source GLB; identity transforms; no axis conversion';filter='Frozen PCFSoftShadowMap runtime alias to PCFShadowMap: 5 Vogel samples + bilinear depth comparison'}
    [IO.File]::WriteAllText((Join-Path $repository 'resources/manifests/rhi-opaque.json'),($manifest|ConvertTo-Json -Depth 10).Replace("`r`n","`n")+"`n",$utf8)
    Run 'production-configure.txt' {cmake --preset qt-release -DYALAHOO_REVIEW=OFF} | Out-Null
    Run 'production-build.txt' {cmake --build --preset qt-release} | Out-Null
    Run 'production-ctest.txt' {ctest --preset qt-release --output-on-failure} | Out-Null
    Save 'summary.json' (@{round='R10A';configuration='Release';passed=$true;qtVersion=$chain.qtVersion;reviewCases=36;resizeCases=12;resizeBurst=30;shadowPairs=6;maxCornerProjectionPixels=$report.maxCornerProjectionPixels;knownPlaneDepthError=$report.knownPlaneDepthError;worstChannelMAE=$report.worstChannelMAE;productImplementationReady=$false;operatingSystem=[Environment]::OSVersion.VersionString;graphicsDevice=$report.cases[0].metadata.device;graphicsDeviceSource='actual QRhi::driverInfo';graphicsDriverVersion=$null;graphicsDriverVersionStatus='Not exposed by QRhi; WMI unavailable in sandbox'}|ConvertTo-Json -Depth 8)
    & ./tools/audit/audit-r10a.ps1
    if(-not $?){throw 'Static audit failed'}
    Write-Output 'R10A Release verification passed; product replacement remains R10D.'
} finally {Pop-Location}
