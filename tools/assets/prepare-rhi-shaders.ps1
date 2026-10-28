# Preserve current per-entity hoisting when preparing shared paper mathematics.
$ErrorActionPreference = 'Stop'
& python (Join-Path $PSScriptRoot 'prepare-rhi-paper.py')
if ($LASTEXITCODE -ne 0) {
    throw 'RHI paper helper generation failed'
}
