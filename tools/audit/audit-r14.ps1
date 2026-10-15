$ErrorActionPreference = 'Stop'
python3 (Join-Path $PSScriptRoot 'audit-browse-navigation.py')
if ($LASTEXITCODE -ne 0) {
    throw 'R19E source audit failed'
}
Write-Output 'Source audit passed; target runtime checks remain separate'
