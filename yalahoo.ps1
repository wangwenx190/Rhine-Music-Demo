#requires -Version 7.2
& (Join-Path $PSScriptRoot 'tools/launchers/run-native.ps1') @args
exit $LASTEXITCODE
