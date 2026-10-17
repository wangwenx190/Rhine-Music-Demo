param(
    [ValidateSet('d3d11', 'opengl')][string[]]$Backends = @('d3d11'),
    [double[]]$Dprs = @(1, 1.5, 2)
)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/browse-ui'
foreach ($backend in $Backends) {
    foreach ($dpr in $Dprs) {
        $directory = Join-Path $evidence "$backend-dpr-$dpr"
        python3 (Join-Path $PSScriptRoot 'compare-browse-structure.py') (Join-Path $directory 'report.json') (Join-Path $evidence 'web/report.json') (Join-Path $directory 'structure.json')
        if ($LASTEXITCODE -ne 0) {
            throw 'Frozen CSS structure verification failed'
        }
    }
}
Write-Output 'R14 source structure passed'
