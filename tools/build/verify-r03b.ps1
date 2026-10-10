$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/branding'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Path,[string]$Text) {
    $Text=$Text.Replace($env:USERPROFILE.Replace('\','/'),'<USERPROFILE>').Replace($env:USERPROFILE,'<USERPROFILE>')
    $Text=$Text.Replace($repository.Replace('\','/'),'<REPOSITORY>').Replace($repository,'<REPOSITORY>')
    $Text=(($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd()+"`n"
    [IO.File]::WriteAllText($Path,$Text,$utf8)
}
Push-Location -LiteralPath $repository
try {
    $before=(Get-FileHash resources/manifests/branding.json -Algorithm SHA256).Hash
    $export=& ./tools/assets/export-branding.ps1 -Preset qt-release -WindowPreview 2>&1
    Save-PublicText (Join-Path $evidence 'export.txt') ($export -join "`n")
    if ($before -ne (Get-FileHash resources/manifests/branding.json -Algorithm SHA256).Hash) { throw 'Branding resource regeneration is not reproducible.' }
    $results=@()
    foreach ($preset in @('qt-release')) {
        $configuration=$preset.Replace('qt-','')
        $output=& ./tools/build/build.ps1 -Preset $preset -Action All 2>&1
        Save-PublicText (Join-Path $evidence "$configuration-build.txt") ($output -join "`n")
        if (($output -join "`n") -notmatch '100% tests passed(?:, 0 tests failed)? out of [0-9]+') { throw 'Expected all registered tests to pass.' }
        $lint=& cmake --build --preset $preset --target Yalahoo_qmllint 2>&1
        if ($LASTEXITCODE -ne 0) { throw 'QML lint failed.' }
        Save-PublicText (Join-Path $evidence "$configuration-qmllint.txt") ($lint -join "`n")
        $build=Join-Path $repository "build/$configuration"
        foreach ($file in @('branding-report.json','runtime.json')) {
            Save-PublicText (Join-Path $evidence "$configuration-$file") (Get-Content -Raw -Encoding UTF8 (Join-Path $build "verification/$file"))
        }
        $chain=Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
        Save-PublicText (Join-Path $evidence "$configuration-toolchain.json") ($chain | ConvertTo-Json -Depth 10)
        $result=Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/branding-report.json') | ConvertFrom-Json
        $runtime=Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/runtime.json') | ConvertFrom-Json
        if (-not $result.passed -or -not $runtime.passed -or $result.rasterCases.Count -ne 243 -or $result.logoCases.Count -ne 54) { throw 'Branding / shared runtime failed.' }
        $results+=@{preset=$preset;ctest='all registered tests passed';qmllint='passed';branding='passed';sharedRuntime='passed'}
    }
    $summary=@{round='R03B';qtVersion=$chain.qtVersion;operatingSystem=[Environment]::OSVersion.VersionString;
        configurations=$results;reproducible=$true;svgCount=13;rasterCases=243;logoCases=54;icoSizes=@(16,24,32,48,64,128,256);
        icnsChunks=@('ic07','ic08','ic09','ic10');revealSegments=17;visualReview='B selected; 16px holes corrected to pixel centers; native-size/day/night/transparent/mono and reveal proofs inspected';
        scope='Original branding assets and Qt export proof. Header/opening/system integration remains R14/R20/R30-R32.'}
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 20)
} finally { Pop-Location }
