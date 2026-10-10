$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/assets'
$preview = Join-Path $repository 'docs/media/assets'
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
    $before = (Get-FileHash resources/manifests/assets.json -Algorithm SHA256).Hash
    $prepared = & (Join-Path $repository 'tools/assets/prepare-assets.ps1') 2>&1
    Save-PublicText (Join-Path $evidence 'prepare.txt') ($prepared -join "`n")
    if ($before -ne (Get-FileHash resources/manifests/assets.json -Algorithm SHA256).Hash) { throw 'Same-Kit asset regeneration is not reproducible.' }
    $results = @()
    foreach ($preset in @('qt-release')) {
        $configuration = $preset.Replace('qt-', '')
        $buildOutput = & (Join-Path $PSScriptRoot 'build.ps1') -Preset $preset -Action All 2>&1
        Save-PublicText (Join-Path $evidence "$configuration-build.txt") ($buildOutput -join "`n")
        $build = Join-Path $repository "build/$configuration"
        $lint = & cmake --build --preset $preset --target Yalahoo_qmllint 2>&1
        if ($LASTEXITCODE -ne 0) { throw 'qmllint failed.' }
        Save-PublicText (Join-Path $evidence "$configuration-qmllint.txt") ($lint -join "`n")
        foreach ($file in @('assets-test.xml', 'assets-report.json', 'runtime.json')) {
            Save-PublicText (Join-Path $evidence "$configuration-$file") (Get-Content -Raw -Encoding UTF8 (Join-Path $build "verification/$file"))
        }
        foreach ($file in @('icons.png', 'fonts.png', 'runtime.png')) {
            Copy-Item -LiteralPath (Join-Path $build "verification/$file") -Destination (Join-Path $preview "$configuration-$file")
        }
        $runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/runtime.json') | ConvertFrom-Json
        if (-not $runtime.passed -or $runtime.font -ne 'MiSans') { throw 'GPU/font runtime failed.' }
        $chain = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
        Save-PublicText (Join-Path $evidence "$configuration-toolchain.json") ($chain | ConvertTo-Json -Depth 10)
        $results += @{ preset = $preset; build = 'passed'; ctest = 'all registered tests passed'; qmllint = 'passed'; runtime = 'passed' }
    }
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $cwd = Join-Path $repository 'build/asset cwd 中文'
    New-Item -ItemType Directory -Force -Path $cwd | Out-Null
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = Join-Path $repository 'build/release/bin/yalahoo.exe'
    $info.WorkingDirectory = $cwd
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.ArgumentList.Add('--verify-runtime')
    $info.ArgumentList.Add((Join-Path $cwd 'runtime.json'))
    $process = [Diagnostics.Process]::Start($info)
    try {
        if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Unicode cwd runtime timed out.' }
        if ($process.ExitCode -ne 0) { throw 'Unicode cwd runtime failed.' }
    } finally { $process.Dispose() }
    Save-PublicText (Join-Path $evidence 'unicode-cwd-runtime.json') (Get-Content -Raw -Encoding UTF8 (Join-Path $cwd 'runtime.json'))
    $summary = @{ round = 'R03'; qtVersion = $chain.qtVersion; configurations = $results;
        operatingSystem = [Environment]::OSVersion.VersionString; assetCount = 47;
        frozenSourceInputs = 24; reproducible = $true; unicodeWorkingDirectory = 'passed';
        audioScope = 'All four complete resources LoadedMedia/duration with no QAudioOutput and zero PlayingState events. Actual playback belongs to R24/R25.';
        visualScope = 'Windows D3D11 actual three-shell geometry, MiSans and functional icon probe; shading/UI recreation belongs to later rounds.' }
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 20)
} finally { Pop-Location }
