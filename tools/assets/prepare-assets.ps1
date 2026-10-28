[CmdletBinding()]
param([ValidateSet('qt-debug', 'qt-release')][string]$Preset = 'qt-release')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../build/asset-tool.ps1')
$asset = Initialize-NativeAssetTool -Tool 'prepare-assets' -Preset $Preset
$repository = $asset.Repository
$build = $asset.Build
$chain = $asset.Chain
Push-Location -LiteralPath $repository
try {
    $kit = $chain.qtKit
    # balsam does not expose a --version switch. Check the executable's actual
    # Windows version resource, as well as qmake in the C++ preparation tool.
    $version = (Get-Item -LiteralPath (Join-Path $kit 'bin/balsam.exe')).VersionInfo.ProductVersion
    if ($version -ne "$($chain.qtVersion).0" -and $version -ne $chain.qtVersion) {
        throw "balsam version differs from selected Qt: $version"
    }
    & (Join-Path $build 'tools/prepare-assets.exe') $repository $kit
    if ($LASTEXITCODE -ne 0) {
        throw 'Asset preparation or geometry comparison failed.'
    }
} finally {
    Pop-Location
}
