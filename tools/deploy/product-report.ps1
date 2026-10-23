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

# Require a complete immutable inventory before launching any deployed executable.
function Assert-PackageInventory {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$PackageDirectory)

    $root = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($PackageDirectory))
    $metadata = Get-Content -LiteralPath (Join-Path $root 'package.json') -Raw | ConvertFrom-Json -NoEnumerate
    if ($metadata -isnot [pscustomobject] -or $metadata.configuration -cne 'Release' -or
        $metadata.review -isnot [bool] -or $metadata.review -or
        $metadata.files -isnot [array] -or $metadata.files.Count -eq 0) {
        throw 'Invalid production package inventory.'
    }
    $declared = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($file in $metadata.files) {
        if ($file -isnot [pscustomobject] -or $file.path -isnot [string] -or
            $file.path -match '(^/|\\|:|(^|/)[.]{1,2}(/|$)|//|/$)' -or
            [string]::IsNullOrEmpty($file.path) -or $file.path -ieq 'package.json' -or
            -not $declared.Add($file.path) -or
            ($file.bytes -isnot [long] -and $file.bytes -isnot [int]) -or $file.bytes -lt 0 -or
            $file.sha256 -isnot [string] -or $file.sha256 -notmatch '^[a-fA-F0-9]{64}$') {
            throw 'Invalid package inventory entry.'
        }
        $path = Join-Path $root $file.path
        $item = Get-Item -LiteralPath $path -Force
        if ($item.PSIsContainer -or $item.Attributes -band [IO.FileAttributes]::ReparsePoint -or
            $item.Length -ne $file.bytes -or
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.sha256) {
            throw "Package content changed: $($file.path)"
        }
    }
    $actual = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($item in (Get-ChildItem -LiteralPath $root -Recurse -Force)) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Package links are unsupported.'
        }
        if (-not $item.PSIsContainer) {
            $relative = [IO.Path]::GetRelativePath($root, $item.FullName).Replace('\', '/')
            if ($relative -ine 'package.json') {
                $null = $actual.Add($relative)
            }
        }
    }
    if (-not $declared.SetEquals($actual)) {
        throw 'Package file inventory changed.'
    }
    return $metadata
}
