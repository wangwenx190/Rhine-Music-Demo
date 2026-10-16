#requires -Version 7.2
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $IsWindows) {
    throw 'Use the native launcher for this operating system.'
}
$package = Join-Path $repository 'out/windows'
$program = Join-Path $package 'yalahoo.exe'
$deployed = Test-Path -LiteralPath $program -PathType Leaf
if ($deployed) {
    $manifest = Get-Content -LiteralPath (Join-Path $package 'package.json') -Raw | ConvertFrom-Json
    if ($manifest.configuration -ne 'Release' -or $manifest.review -or
        $manifest.displayName -ne 'yalahOO!!!' -or
        -not (Test-Path -LiteralPath (Join-Path $package 'qt.conf') -PathType Leaf)) {
        throw 'The production package is incomplete; run tools/deploy/windows.ps1.'
    }
} else {
    $program = Join-Path $repository 'build/release/bin/yalahoo.exe'
    if (-not (Test-Path -LiteralPath $program -PathType Leaf)) {
        throw 'Build Release or run tools/deploy/windows.ps1 first.'
    }
    $cache = Join-Path $repository 'build/release/CMakeCache.txt'
    $kitLine = Get-Content -LiteralPath $cache | Select-String '^YALAHOO_SELECTED_QT:PATH=(.+)$'
    $releaseLine = Get-Content -LiteralPath $cache | Select-String '^CMAKE_BUILD_TYPE:STRING=Release$'
    if (-not $kitLine -or -not $releaseLine) {
        throw 'The configured native Release Qt Kit is unavailable; reconfigure Release.'
    }
    $selectedKit = $kitLine.Matches[0].Groups[1].Value
    if (-not (Test-Path -LiteralPath (Join-Path $selectedKit 'bin/Qt6Core.dll'))) {
        throw 'The configured Windows Qt Kit is unavailable; reconfigure Release.'
    }
}
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $program
$start.UseShellExecute = $false
foreach ($key in @('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_CONF_PATH',
        'QML_IMPORT_PATH', 'QML2_IMPORT_PATH')) {
    $start.Environment.Remove($key) | Out-Null
}
if (-not $deployed) {
    $start.Environment['PATH'] = (Join-Path $selectedKit 'bin') + [IO.Path]::PathSeparator + $env:PATH
    $start.Environment['QT_PLUGIN_PATH'] = Join-Path $selectedKit 'plugins'
    $start.Environment['QML_IMPORT_PATH'] = Join-Path $selectedKit 'qml'
}
foreach ($argument in $args) {
    $start.ArgumentList.Add([string]$argument)
}
$process = [Diagnostics.Process]::Start($start)
$process.WaitForExit()
$exitCode = $process.ExitCode
$process.Dispose()
exit $exitCode
