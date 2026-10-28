[CmdletBinding()]
param(
    [ValidateSet('qt-debug', 'qt-release')][string]$Preset = 'qt-release',
    [ValidateSet('Configure', 'Build', 'Test', 'All')][string]$Action = 'All',
    [switch]$Fresh,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'enter-msvc.ps1')
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location -LiteralPath $repository
try {
    if ($Action -in @('Configure', 'All')) {
        $configureArgs = @('--preset', $Preset)
        if ($Fresh) {
            $configureArgs += '--fresh'
        }
        & cmake @configureArgs
        if ($LASTEXITCODE -ne 0) {
            throw 'CMake configure failed.'
        }
    }
    if ($Action -in @('Build', 'All')) {
        $buildArgs = @('--build', '--preset', $Preset)
        if ($Clean) {
            $buildArgs += '--clean-first'
        }
        & cmake @buildArgs
        if ($LASTEXITCODE -ne 0) {
            throw 'CMake build failed.'
        }
    }
    if ($Action -in @('Test', 'All')) {
        & ctest --preset $Preset --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            throw 'CTest failed.'
        }
    }
} finally {
    Pop-Location
}
