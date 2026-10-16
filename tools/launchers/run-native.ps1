$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$program = Join-Path $repository 'build/release/bin/yalahoo.exe'
if (-not (Test-Path -LiteralPath $program -PathType Leaf)) {
    throw 'Build the native Release application first (cmake --preset qt-release).'
}
$cache = Join-Path $repository 'build/release/CMakeCache.txt'
$kitLine = Get-Content -LiteralPath $cache | Select-String '^YALAHOO_SELECTED_QT:PATH=(.+)$'
if (-not $kitLine) {
    throw 'The configured native Qt Kit is unavailable; reconfigure Release.'
}
$selectedKit = $kitLine.Matches[0].Groups[1].Value
$env:PATH = (Join-Path $selectedKit 'bin') + [IO.Path]::PathSeparator + $env:PATH
$env:QT_PLUGIN_PATH = Join-Path $selectedKit 'plugins'
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $program
$start.UseShellExecute = $false
foreach ($argument in $args) {
    $start.ArgumentList.Add([string]$argument)
}
$process = [Diagnostics.Process]::Start($start)
$process.WaitForExit()
exit $process.ExitCode
