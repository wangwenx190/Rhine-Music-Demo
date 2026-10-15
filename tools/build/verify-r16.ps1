[CmdletBinding()]
param(
    [switch]$FullMatrix,
    [switch]$BackendMatrix,
    [switch]$TitleContext,
    [switch]$UseExistingWebSource,
    [ValidateSet('d3d11', 'opengl')][string]$Backend = 'd3d11',
    [ValidateSet(1, 1.5, 2)][double]$Dpr = 1,
    [string]$EvidenceDirectory = 'verification/windows/title-reels'
)

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if ([int][bool]$FullMatrix + [int][bool]$BackendMatrix + [int][bool]$TitleContext -gt 1) {
    throw 'Choose FullMatrix, BackendMatrix or TitleContext'
}
function Run([scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Title verification command failed with exit $LASTEXITCODE"
    }
}
Push-Location -LiteralPath $repository
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Run { cmake --preset qt-release -DYALAHOO_REVIEW=ON }
    Run { cmake --build --preset qt-release --target format }
    Run { cmake --build --preset qt-release --target check-format }
    Run { cmake --build --preset qt-release }
    Run { cmake --build --preset qt-release --target Yalahoo_qmllint ui-audit_qmllint }
    $evidence = Join-Path $repository $EvidenceDirectory
    $source = Join-Path $evidence 'web'
    $backends = if ($FullMatrix) {
        @('d3d11', 'opengl')
    } else {
        @($Backend)
    }
    $dprs = if ($FullMatrix -or $BackendMatrix) {
        @(1, 1.5, 2)
    } else {
        @($Dpr)
    }
    if (-not $UseExistingWebSource) {
        if ($dprs.Count -eq 1) {
            Run { node tools/review/export-title.mjs --dpr $Dpr --output $source }
        } else {
            Run { node tools/review/export-title.mjs --output $source }
        }
    }
    foreach ($backend in $backends) {
        foreach ($dpr in $dprs) {
            $directory = Join-Path $evidence "$backend-dpr-$dpr"
            Run { node tools/build/verify-title.mjs --backend $backend --dpr $dpr --source $source --output $directory }
            Run { node tools/audit/audit-title.mjs $directory }
        }
    }
    Run { python tools/audit/audit-title-source.py }
    Run { ctest --preset qt-release --output-on-failure }
} finally {
    Pop-Location
}
