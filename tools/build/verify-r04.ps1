$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/library-data'
$preview = Join-Path $repository 'docs/media/library-data'
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
    # Standard verification is Release only; Debug is reserved for explicit stack capture.
    $preset = 'qt-release'
    $output = & (Join-Path $PSScriptRoot 'build.ps1') -Preset $preset -Action All 2>&1
    Save-PublicText (Join-Path $evidence 'release-build.txt') ($output -join "`n")
    if (($output -join "`n") -notmatch '100% tests passed(?:, 0 tests failed)? out of 10') { throw 'Expected ten passing tests.' }
    $lint = & cmake --build --preset $preset --target Yalahoo_qmllint 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'QML lint failed.' }
    Save-PublicText (Join-Path $evidence 'release-qmllint.txt') ($lint -join "`n")
    $build = Join-Path $repository 'build/release'
    $chain = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
    Save-PublicText (Join-Path $evidence 'release-toolchain.json') ($chain | ConvertTo-Json -Depth 10)
    foreach ($file in @('library-data-test.xml', 'library-qml-test.xml', 'runtime-library.json')) {
        Save-PublicText (Join-Path $evidence "release-$file") (Get-Content -Raw -Encoding UTF8 (Join-Path $build "verification/$file"))
    }
    foreach ($file in @('library-data-test.xml', 'library-qml-test.xml')) {
        $xml = [xml](Get-Content -Raw -Encoding UTF8 (Join-Path $build "verification/$file"))
        if ([int]$xml.testsuite.failures -or [int]$xml.testsuite.errors -or [int]$xml.testsuite.skipped) { throw 'Data/QML failures or skipped tests.' }
        if ($xml.SelectNodes('//system-err').Count) { throw 'Unresolved Qt diagnostic in data tests.' }
    }
    $runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'verification/runtime-library.json') | ConvertFrom-Json
    if (-not $runtime.passed -or $runtime.libraryAlbums -ne 21 -or $runtime.searchResults -ne 28 -or $runtime.metadataResults -ne 1) { throw 'Anonymous library GPU/QML runtime failed.' }
    Copy-Item -LiteralPath (Join-Path $build 'verification/runtime.png') -Destination (Join-Path $preview 'release-runtime.png')
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $cwd = Join-Path $repository 'build/library cwd 中文'
    New-Item -ItemType Directory -Force -Path $cwd | Out-Null
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = Join-Path $build 'bin/yalahoo.exe'
    $info.WorkingDirectory = $cwd
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    foreach ($argument in @('--verify-runtime', (Join-Path $cwd 'runtime.json'), '--verify-library',
        (Join-Path $repository 'tests/fixtures/web-baseline/anonymous.json'))) { $info.ArgumentList.Add($argument) }
    $native = [Diagnostics.Process]::Start($info)
    try {
        $childNames = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $($native.Id)" | Select-Object -ExpandProperty Name)
        $connections = @(Get-NetTCPConnection -ErrorAction Stop | Where-Object OwningProcess -EQ $native.Id)
        if (-not $native.WaitForExit(30000)) { $native.Kill(); throw 'Unicode cwd runtime timeout.' }
        if ($native.ExitCode -ne 0 -or $childNames.Count -or $connections.Count) { throw 'Runtime/child-process/network assertion failed.' }
        Save-PublicText (Join-Path $evidence 'process-observation.json') (@{
            configuration = 'Release'; exitCode = $native.ExitCode; childProcessNames = $childNames;
            tcpConnectionCount = $connections.Count; unicodeWorkingDirectory = 'passed';
            limitations = 'One process/TCP snapshot, supplemented by full core/provider source audit: no network or child-process APIs/dependencies.'
        } | ConvertTo-Json -Depth 10)
    } finally { $native.Dispose() }
    Save-PublicText (Join-Path $evidence 'unicode-cwd-runtime.json') (Get-Content -Raw -Encoding UTF8 (Join-Path $cwd 'runtime.json'))
    $oracle = Get-Content -Raw -Encoding UTF8 tests/fixtures/core/search-golden.json | ConvertFrom-Json
    $fixtureHash = (Get-FileHash tests/fixtures/web-baseline/anonymous.json -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($fixtureHash -ne $oracle.fixtureSha256) { throw 'Search oracle fixture hash differs.' }
    $summary = @{
        round = 'R04'; configuration = 'Release'; qtVersion = $chain.qtVersion;
        operatingSystem = [Environment]::OSVersion.VersionString; ctest = '10/10 passed'; qmllint = 'passed';
        frozenSearchCases = $oracle.cases.Count; baseline = $oracle.baseline; fixtureSha256 = $fixtureHash;
        sourceSha256 = $oracle.sourceSha256; networkRequestApis = 0;
        validation = 'All frozen library columns, 216 frozen search cases, Unicode/local URLs and unknown-field round-trip, model notifications, independent IDs, asynchronous cache/Deferred, cancellation/generation/lifecycle, QML roles and injected Matched/Uncertain/Error, actual Windows D3D11 window/mouse and Unicode cwd.';
        scope = 'R04 data/provider integration only. No scanning, production browsing UI or actual audio playback.'
    }
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 10)
} finally { Pop-Location }
