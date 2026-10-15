[CmdletBinding()]
param()

# Historical image acceptance is retired. The current audit records source,
# call-path and platform limitations; actual component runs use audit-title.mjs.
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& python (Join-Path $repository 'tools/audit/audit-title-source.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Title source audit failed'
}
