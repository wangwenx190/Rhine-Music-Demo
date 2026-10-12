# Dot-source to enable the compiler in this PowerShell session only.
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere not found; install Visual Studio C++ tools.'
}
$vsInstall = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) {
    throw 'No Visual Studio native C++ toolchain found.'
}
$vsDev = Join-Path $vsInstall 'Common7/Tools/VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDev)) {
    throw 'VsDevCmd.bat not found.'
}
$nativeArch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
    'arm64'
} else {
    'x64'
}
if ($env:YALAHOO_MSVC_ENV_READY -eq "$vsInstall|$nativeArch" -and (Get-Command cl -ErrorAction SilentlyContinue)) {
    # Asset preparation and Release verification can call this helper more
    # than once in one process. VsDevCmd prepends its paths on every call.
    # Keep the initialized environment rather than growing beyond cmd's limit.
    return
}
$start = [System.Diagnostics.ProcessStartInfo]::new()
$start.FileName = $env:ComSpec
$start.Arguments = '/d /s /c ""' + $vsDev + '" -no_logo -arch=' + $nativeArch + ' -host_arch=' + $nativeArch + ' >nul && set"'
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$compilerProcess = [System.Diagnostics.Process]::Start($start)
$vsEnvironment = $compilerProcess.StandardOutput.ReadToEnd() -split "`r?`n"
$compilerErrors = $compilerProcess.StandardError.ReadToEnd()
$compilerProcess.WaitForExit()
if ($compilerProcess.ExitCode -ne 0) {
    throw "VsDevCmd environment initialization failed: $compilerErrors"
}
$compilerProcess.Dispose()
$compilerPath = $null
foreach ($entry in $vsEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') {
        # VsDevCmd writes PATH. Some hosts also pass an older Path entry; importing
        # that later would overwrite the compiler search path on Windows.
        if ($Matches[1] -ieq 'Path') {
            if ($Matches[1] -ceq 'PATH' -or -not $compilerPath) {
                $compilerPath = $Matches[2]
            }
        } else {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}
if ($compilerPath) {
    $env:PATH = $compilerPath
}
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    $cmakeBin = Join-Path $env:ProgramFiles 'CMake/bin'
    if (-not (Test-Path -LiteralPath (Join-Path $cmakeBin 'cmake.exe'))) {
        throw 'CMake not found.'
    }
    $env:PATH = "$cmakeBin;$env:PATH"
} else {
    $cmakeBin = Split-Path -Parent $cmakeCommand.Source
    $env:PATH = "$cmakeBin;$env:PATH"
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw 'Ninja not found; provide it in PATH or beside CMake.'
}
$env:YALAHOO_MSVC_ENV_READY = "$vsInstall|$nativeArch"
