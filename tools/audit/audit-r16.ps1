[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& python (Join-Path $repository 'tools/audit/audit-title-source.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Title source audit failed'
}
