[CmdletBinding()]
param([ValidateSet('qt-debug', 'qt-release')][string]$Preset = 'qt-release', [switch]$WindowPreview)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repository 'tools/build/enter-msvc.ps1')
Push-Location -LiteralPath $repository
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw 'Branding configure failed.'
    }
    & cmake --build --preset $Preset --target export-branding
    if ($LASTEXITCODE -ne 0) {
        throw 'Branding build failed.'
    }
    $build = Join-Path $repository ('build/' + $Preset.Replace('qt-', ''))
    $chain = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $arguments = @($repository)
    if ($WindowPreview) {
        $arguments += '--window-preview'
    }
    & (Join-Path $build 'tools/export-branding.exe') @arguments
    if ($LASTEXITCODE -ne 0) {
        throw 'Branding export/verification failed.'
    }
} finally {
    Pop-Location
}
