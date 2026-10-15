[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'enter-msvc.ps1')
$runner = Join-Path $PSScriptRoot 'verify-material-review.py'
& python $runner lighting
if ($LASTEXITCODE -ne 0) {
    throw 'Source/state material verification failed'
}
