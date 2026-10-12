[CmdletBinding()]
param([ValidateSet('qt-debug', 'qt-release')][string]$Preset = 'qt-release')
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repository 'tools/build/enter-msvc.ps1')
Push-Location -LiteralPath $repository
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw 'Configure failed.'
    }
    & cmake --build --preset $Preset --target prepare-assets
    if ($LASTEXITCODE -ne 0) {
        throw 'Asset tool build failed.'
    }
    $build = Join-Path $repository ('build/' + $Preset.Replace('qt-', ''))
    $chain = Get-Content -Raw -Encoding UTF8 (Join-Path $build 'toolchain.json') | ConvertFrom-Json
    $kit = $chain.qtKit
    # balsam does not expose a --version switch. Check the executable's actual
    # Windows version resource, as well as qmake in the C++ preparation tool.
    $version = (Get-Item -LiteralPath (Join-Path $kit 'bin/balsam.exe')).VersionInfo.ProductVersion
    if ($version -ne "$($chain.qtVersion).0" -and $version -ne $chain.qtVersion) {
        throw "balsam version differs from selected Qt: $version"
    }
    $env:PATH = "$kit/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$kit/plugins"
    & (Join-Path $build 'tools/prepare-assets.exe') $repository $kit
    if ($LASTEXITCODE -ne 0) {
        throw 'Asset preparation or geometry comparison failed.'
    }
    $manifest = Get-Content -Raw -Encoding UTF8 resources/manifests/assets.json | ConvertFrom-Json
    $inputs = @($manifest.entries.source) + @($manifest.additionalInputs.source) | Sort-Object -Unique
    foreach ($source in $inputs) {
        $oldPath = $source -replace '^legacy-web/', ''
        $expected = & git rev-parse "$($manifest.baselineCommit):$oldPath"
        if ($LASTEXITCODE -ne 0) {
            throw "Frozen source not found: $source"
        }
        $actual = & git hash-object -- $source
        if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {
            throw "Frozen source changed: $source"
        }
    }
    Write-Output "Frozen baseline verified for $($inputs.Count) distinct source inputs."
} finally {
    Pop-Location
}
