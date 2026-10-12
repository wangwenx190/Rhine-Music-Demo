$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'enter-msvc.ps1')
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/native-bootstrap'
$preview = Join-Path $repository 'docs/media/native-bootstrap'
New-Item -ItemType Directory -Force -Path $evidence, $preview | Out-Null
$utf8 = [System.Text.UTF8Encoding]::new($false)
function Save-PublicText([string]$Path, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}
Push-Location -LiteralPath $repository
try {
    $results = @()
    $referenceKit = $null
    foreach ($preset in @('qt-release')) {
        $configuration = $preset.Replace('qt-', '')
        $buildOutput = & (Join-Path $PSScriptRoot 'build.ps1') -Preset $preset -Action All -Fresh -Clean 2>&1
        Save-PublicText (Join-Path $evidence "$configuration-build.txt") ($buildOutput -join "`n")
        $buildRoot = Join-Path $repository "build/$configuration"
        $toolchain = Get-Content -Raw -Encoding UTF8 (Join-Path $buildRoot 'toolchain.json') | ConvertFrom-Json
        if ($referenceKit -and $referenceKit -ne $toolchain.qtKit) {
            throw 'Verification selected different Qt Kits.'
        }
        $referenceKit = $toolchain.qtKit
        $runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $buildRoot 'verification/runtime.json') | ConvertFrom-Json
        if (-not $runtime.passed) {
            throw "$preset runtime verification failed."
        }
        Save-PublicText (Join-Path $evidence "$configuration-runtime.json") ($runtime | ConvertTo-Json -Depth 20)
        Save-PublicText (Join-Path $evidence "$configuration-toolchain.json") (Get-Content -Raw -Encoding UTF8 (Join-Path $buildRoot 'toolchain.json'))
        Copy-Item -LiteralPath (Join-Path $buildRoot 'verification/runtime.png') -Destination (Join-Path $preview "$configuration.png")
        $lint = & cmake --build --preset $preset --target Yalahoo_qmllint 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "$preset qmllint failed."
        }
        Save-PublicText (Join-Path $evidence "$configuration-qmllint.txt") ($lint -join "`n")
        $results += @{
            preset = $preset
            configure = 'passed'
            build = 'passed'
            tests = 'passed'
            qmllint = 'passed'
            runtime = 'passed'
        }
    }
    $cache = Get-Content -Encoding UTF8 build/release/CMakeCache.txt
    $kit = ($cache | Select-String '^YALAHOO_SELECTED_QT:PATH=(.*)$').Matches.Groups[1].Value
    $shader = & (Join-Path $kit 'bin/qsb.exe') --dump build/release/src/app/.qsb/probe.frag.qsb 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw 'qsb dump failed.'
    }
    foreach ($variant in @('GLSL 150', 'HLSL 50', 'MSL 12')) {
        if (($shader -join "`n") -notmatch $variant) {
            throw "Missing shader variant: $variant"
        }
    }
    Save-PublicText (Join-Path $evidence 'shader-variants.txt') ($shader -join "`n")
    $stalePrefix = Join-Path $repository 'build/synthetic-stale-kit'
    $staleOutput = & cmake --preset qt-release -B build/stale-kit-check "-DYALAHOO_SELECTED_QT=$stalePrefix" 2>&1
    $staleExit = $LASTEXITCODE
    if ($staleExit -eq 0 -or ($staleOutput -join "`n") -notmatch 'differs from latest Kit') {
        throw 'Real configure did not reject stale Kit cache.'
    }
    $staleResult = @{
        exitCode = $staleExit
        expectedFailure = $true
        diagnostic = ($staleOutput -join "`n")
    }
    Save-PublicText (Join-Path $evidence 'stale-cache-rejection.json') ($staleResult | ConvertTo-Json -Depth 10)
    # Sample the actual native process in a different Unicode/space cwd. Qt's
    # event loop is active; source inspection additionally covers the whole lifetime.
    $runtimeCwd = Join-Path $repository 'build/runtime cwd 中文'
    New-Item -ItemType Directory -Force -Path $runtimeCwd | Out-Null
    $env:PATH = "$kit/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$kit/plugins"
    $env:QSG_INFO = '1'
    $processInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $processInfo.FileName = Join-Path $repository 'build/release/bin/yalahoo.exe'
    $processInfo.WorkingDirectory = $runtimeCwd
    $processInfo.UseShellExecute = $false
    $processInfo.CreateNoWindow = $true
    $processInfo.ArgumentList.Add('--verify-runtime')
    $processInfo.ArgumentList.Add((Join-Path $runtimeCwd 'runtime.json'))
    $nativeProcess = [System.Diagnostics.Process]::Start($processInfo)
    try {
        $childNames = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $($nativeProcess.Id)" | Select-Object -ExpandProperty Name)
        $connections = @(Get-NetTCPConnection -ErrorAction Stop | Where-Object OwningProcess -EQ $nativeProcess.Id)
        if (-not $nativeProcess.WaitForExit(30000)) {
            $nativeProcess.Kill()
            throw 'Runtime observer timed out.'
        }
        if ($nativeProcess.ExitCode -ne 0) {
            throw 'Unicode cwd runtime failed.'
        }
        if ($childNames.Count -gt 0 -or $connections.Count -gt 0) {
            throw 'Unexpected native child process or TCP connection.'
        }
        $observed = @{
            childProcessNames = $childNames
            tcpConnectionCount = $connections.Count

            unicodeWorkingDirectory = 'passed'
            exitCode = $nativeProcess.ExitCode

            limitations = 'A startup snapshot; complete lifetime also audited in source (no process or network request APIs).'
        }
        Save-PublicText (Join-Path $evidence 'process-observation.json') ($observed | ConvertTo-Json -Depth 10)
    } finally {
        $nativeProcess.Dispose()
    }
    $dateInChina = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow, 'China Standard Time').ToString('yyyy-MM-dd')
    $summary = @{
        date = $dateInChina
        round = 'R02'
        configurations = $results

        cmake = (& cmake --version | Select-Object -First 1)
        ninja = (& ninja --version)

        operatingSystem = [Environment]::OSVersion.VersionString

        runtimeScope = 'Actual Windows GPU/window/mouse/QML/media construction. No audio source or playback; player functionality belongs to later rounds.'
    }
    Save-PublicText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 20)
} finally {
    Pop-Location
}
