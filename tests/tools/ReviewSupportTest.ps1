#requires -Version 7.2
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repository 'tools/build/ReviewSupport.ps1')
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('yalahoo review space-' + [guid]::NewGuid().ToString('N'))
$names = @('PATH', 'QT_PLUGIN_PATH', 'QSG_RENDER_LOOP', 'QSG_NO_VSYNC', 'QT_SCALE_FACTOR', 'YALAHOO_REVIEW_FIXTURE')
$saved = Save-ProcessEnvironment -Names $names
$utf8 = [Text.UTF8Encoding]::new($false)
$null = New-Item -ItemType Directory -Path $temporary
Push-Location -LiteralPath $temporary
try {
    # Native output and nonzero status must survive the common logging wrapper.
    $result = @(Invoke-ReviewCommand 'success.txt' { cmake -E echo 'fixture output' } $temporary)
    if ($result.Count -ne 0 -or (Get-Content -Raw 'success.txt') -notmatch 'fixture output') {
        throw 'Command logging must not pollute the toolchain return pipeline.'
    }
    $failed = $false
    try {
        Invoke-ReviewCommand 'failure.txt' { cmake -E false } $temporary
    } catch {
        $failed = $true
    }
    if (-not $failed -or -not (Test-Path -LiteralPath 'failure.txt')) {
        throw 'A failing native command must fail and retain its log.'
    }
    [Environment]::SetEnvironmentVariable('YALAHOO_REVIEW_FIXTURE', $null, 'Process')
    $absent = Save-ProcessEnvironment -Names @('YALAHOO_REVIEW_FIXTURE')
    $env:YALAHOO_REVIEW_FIXTURE = 'temporary'
    Restore-ProcessEnvironment -Saved $absent
    if ($null -ne [Environment]::GetEnvironmentVariable('YALAHOO_REVIEW_FIXTURE', 'Process')) {
        throw 'Environment restoration did not remove a newly introduced variable.'
    }
    $env:YALAHOO_REVIEW_FIXTURE = 'original'
    $existing = Save-ProcessEnvironment -Names @('YALAHOO_REVIEW_FIXTURE')
    $env:YALAHOO_REVIEW_FIXTURE = 'replacement'
    Restore-ProcessEnvironment -Saved $existing
    if ($env:YALAHOO_REVIEW_FIXTURE -ne 'original') {
        throw 'Environment restoration lost the original value.'
    }

    # Run the unchanged initializer with a stub compiler setup and native build transport.
    # This isolates its returned metadata and environment from MSVC/Qt availability.
    Copy-Item -LiteralPath (Join-Path $repository 'tools/build/ReviewSupport.ps1') -Destination $temporary
    [IO.File]::WriteAllText((Join-Path $temporary 'enter-msvc.ps1'), '# Isolated compiler fixture.', $utf8)
    $null = New-Item -ItemType Directory -Path 'build/release'
    $kit = Join-Path $temporary 'selected kit'
    $expected = @{
        qtKit = $kit
        qtVersion = 'fixture-version'
        configuration = 'Release'
    }
    [IO.File]::WriteAllText((Join-Path $temporary 'build/release/toolchain.json'), ($expected | ConvertTo-Json), $utf8)
    . (Join-Path $temporary 'ReviewSupport.ps1')
    $script:commands = @()
    function Invoke-ReviewCommand {
        param([string]$Name, [scriptblock]$Command, [string]$OutputDirectory)
        $script:commands += $Name
    }
    $chain = @(Initialize-ReviewBuild -OutputDirectory $temporary)
    if ($chain.Count -ne 1 -or $chain[0].qtVersion -ne 'fixture-version' -or
        $chain[0].qtKit -ne $kit -or $script:commands.Count -ne 5) {
        throw 'Review initialization must return exactly the selected toolchain metadata.'
    }
    if ($env:QT_PLUGIN_PATH -ne "$kit/plugins" -or $env:QT_SCALE_FACTOR -ne '1') {
        throw 'Review environment did not use the returned Kit.'
    }
} finally {
    Restore-ProcessEnvironment -Saved $saved
    Pop-Location
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
Write-Host 'Review command failures, metadata pipeline and environment restoration passed.'
