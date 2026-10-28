[CmdletBinding()]
param([ValidateSet('qt-debug', 'qt-release')][string]$Preset = 'qt-release', [switch]$WindowPreview)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../build/asset-tool.ps1')
$asset = Initialize-NativeAssetTool -Tool 'export-branding' -Preset $Preset
$repository = $asset.Repository
$build = $asset.Build
Push-Location -LiteralPath $repository
try {
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
