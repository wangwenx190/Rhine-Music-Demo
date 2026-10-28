# Shared native asset-tool configuration, build, Kit environment and output paths.
function Initialize-NativeAssetTool {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][ValidateSet('prepare-assets', 'export-branding')][string]$Tool,
        [ValidateSet('qt-debug', 'qt-release')][string]$Preset = 'qt-release'
    )
    $repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
    . (Join-Path $repository 'tools/build/enter-msvc.ps1')
    Push-Location -LiteralPath $repository
    try {
        & cmake --preset $Preset | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw 'Asset tool configure failed.'
        }
        & cmake --build --preset $Preset --target $Tool | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw 'Asset tool build failed.'
        }
        $build = Join-Path $repository ('build/' + $Preset.Replace('qt-', ''))
        $chain = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
        $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
        $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
        return [pscustomobject]@{
            Repository = $repository
            Build = $build
            Chain = $chain
        }
    } finally {
        Pop-Location
    }
}
