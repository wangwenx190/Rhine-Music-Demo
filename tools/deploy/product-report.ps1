# Validate the current product report without PowerShell truthiness conversions.
function Assert-ProductVerificationReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "The current product verification report is missing: $Path"
    }
    $report = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -NoEnumerate
    if ($report -isnot [pscustomobject] -or $report.passed -isnot [bool] -or -not $report.passed) {
        throw "The current product verification report failed: $Path"
    }
}
