#requires -Version 7.2
[CmdletBinding()]
param(
    [ValidateSet('help', 'demo', 'normal', 'browse', 'navigation', 'detail', 'overview')]
    [string]$Scenario = 'help',
    [string]$PackageDirectory,
    [string]$OutputDirectory,
    [string[]]$ApplicationArguments = @()
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'product-report.ps1')
if (-not $IsWindows) {
    throw 'The deployed Windows product must be verified on Windows.'
}
if (-not $PackageDirectory) {
    $PackageDirectory = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'yalahoo.exe')) {
        $PSScriptRoot
    } else {
        Join-Path $PSScriptRoot '../../out/windows'
    }
}
$PackageDirectory = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($PackageDirectory))
$metadata = Get-Content -LiteralPath (Join-Path $PackageDirectory 'package.json') -Raw | ConvertFrom-Json
foreach ($file in $metadata.files) {
    $path = [IO.Path]::GetFullPath((Join-Path $PackageDirectory $file.path))
    if (-not $path.StartsWith($PackageDirectory + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Invalid package inventory path.'
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.sha256) {
        throw "Package content changed: $($file.path)"
    }
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path ([IO.Path]::GetTempPath()) ('yalahoo-deployment-' + [guid]::NewGuid().ToString('N'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if ($OutputDirectory.StartsWith($PackageDirectory + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Verification output must be outside the deployed package.'
}
if (Test-Path -LiteralPath $OutputDirectory) {
    throw 'Use a fresh output directory to preserve earlier evidence.'
}
$null = New-Item -ItemType Directory -Path $OutputDirectory
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = Join-Path $PackageDirectory 'yalahoo.exe'
$start.WorkingDirectory = $OutputDirectory
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
# Restrict the child environment, leaving the caller's shell untouched.
foreach ($key in @($start.Environment.Keys)) {
    if ($key -match '^(QT_|QML|QSG_|QCPAINTER_|YALAHOO_)') {
        $start.Environment.Remove($key) | Out-Null
    }
}
$start.Environment['PATH'] = (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
$start.Environment['QSG_RHI_BACKEND'] = 'd3d11'
$start.Environment['YALAHOO_VERIFY_FOREGROUND'] = if ($env:YALAHOO_VERIFY_FOREGROUND -eq '1') {
    '1'
} else {
    '0'
}
$start.Environment['QCPAINTER_DEBUG_COLLECT'] = '0'
$arguments = switch ($Scenario) {
    'help' {
        @('--help')
    }
    'demo' {
        @('--demo')
    }
    'normal' {
        @('--data-dir', (Join-Path $OutputDirectory 'data'))
    }
    default {
        @('--verify-' + $Scenario, (Join-Path $OutputDirectory 'product'))
    }
}
foreach ($argument in (@($arguments) + $ApplicationArguments)) {
    $start.ArgumentList.Add([string]$argument)
}
$process = [Diagnostics.Process]::Start($start)
try {
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $interactive = $Scenario -in @('demo', 'normal')
    $finished = if ($interactive) {
        $process.WaitForExit()
        $true
    } else {
        $process.WaitForExit(180000)
    }
    if (-not $finished) {
        $process.Kill($true)
        $process.WaitForExit()
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'stdout.txt'), $stdout.GetAwaiter().GetResult())
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'stderr.txt'), $stderr.GetAwaiter().GetResult())
    $report = [ordered]@{
        scenario = $Scenario
        exitCode = $process.ExitCode
        timedOut = -not $finished
        qtVersion = $metadata.qtVersion
        architecture = $metadata.architecture
        os = [Environment]::OSVersion.VersionString
        cleanPath = $start.Environment['PATH']
        workingDirectoryOutsidePackage = $true
        functionalReportRequired = -not $interactive -and $Scenario -ne 'help'
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'environment.json') -Encoding utf8
    if (-not $finished -or $process.ExitCode -ne 0) {
        throw "Deployed process failed; see $OutputDirectory"
    }
    if ($report.functionalReportRequired) {
        $productReport = Join-Path $OutputDirectory 'product/report.json'
        Assert-ProductVerificationReport -Path $productReport
    }
    # Interactive exit zero and help establish no GUI/audio/performance claim.
    Write-Output "Deployed $Scenario process exited successfully; evidence: $OutputDirectory"
} finally {
    $process.Dispose()
}
