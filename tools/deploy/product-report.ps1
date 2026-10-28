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

# Public verifier scenarios share one mapping to the production CLI/report protocol.
function Get-ProductVerificationArguments {
    param([string]$Scenario, [string]$OutputDirectory)
    switch ($Scenario) {
        'help' {
            @('--help')
        }
        'demo' {
            @('--demo')
        }
        'normal' {
            @('--data-dir', (Join-Path $OutputDirectory 'data'))
        }
        { $_ -in @('deployment', 'browse') } {
            @('--verify-deployment', (Join-Path $OutputDirectory 'product/report.json'))
        }
        { $_ -in @('navigation', 'detail', 'overview') } {
            @(('--verify-' + $Scenario), (Join-Path $OutputDirectory 'product'))
        }
        default {
            throw "Unknown product verification scenario: $Scenario"
        }
    }
}

# Bounded subprocess protocol shared by staging validation and its failure regressions.
function Assert-ProductProbeProcess {
    param([Diagnostics.ProcessStartInfo]$StartInfo, [string]$OutputDirectory,
        [string]$ReportPath, [int]$TimeoutMilliseconds = 30000)
    if (Test-Path -LiteralPath $ReportPath) {
        throw 'Product probe requires a fresh report path.'
    }
    $StartInfo.UseShellExecute = $false
    $StartInfo.CreateNoWindow = $true
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($StartInfo)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $finished = $process.WaitForExit($TimeoutMilliseconds)
        if (-not $finished) {
            $process.Kill($true)
            $process.WaitForExit()
        }
        [IO.File]::WriteAllText((Join-Path $OutputDirectory 'stdout.txt'), $stdout.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $OutputDirectory 'stderr.txt'), $stderr.GetAwaiter().GetResult())
        if (-not $finished -or $process.ExitCode -ne 0) {
            throw "Product load probe failed; see $OutputDirectory"
        }
        Assert-ProductVerificationReport -Path $ReportPath
    } finally {
        $process.Dispose()
    }
}

function Invoke-DeploymentLoadProbe {
    param([string]$PackageDirectory, [string]$OutputDirectory)
    $null = New-Item -ItemType Directory -Path $OutputDirectory
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $PackageDirectory 'yalahoo.exe'
    $start.WorkingDirectory = $OutputDirectory
    foreach ($key in @($start.Environment.Keys)) {
        if ($key -match '^(QT_|QML|QSG_|QCPAINTER_|YALAHOO_)') {
            $null = $start.Environment.Remove($key)
        }
    }
    $start.Environment['PATH'] = (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
    $start.Environment['QSG_RHI_BACKEND'] = 'd3d11'
    $start.Environment['YALAHOO_VERIFY_FOREGROUND'] = '0'
    $start.Environment['QCPAINTER_DEBUG_COLLECT'] = '0'
    foreach ($argument in (Get-ProductVerificationArguments -Scenario deployment -OutputDirectory $OutputDirectory)) {
        $start.ArgumentList.Add([string]$argument)
    }
    Assert-ProductProbeProcess -StartInfo $start -OutputDirectory $OutputDirectory `
        -ReportPath (Join-Path $OutputDirectory 'product/report.json')
}

# A load failure occurs before the old package moves. Failed installation rolls back by rename.
function Publish-WindowsPackage {
    param([string]$Staging, [string]$Output, [scriptblock]$Verify)
    & $Verify
    $backup = $Output + '-previous-' + [guid]::NewGuid().ToString('N')
    if (Test-Path -LiteralPath $Output) {
        Move-Item -LiteralPath $Output -Destination $backup
    }
    try {
        Move-Item -LiteralPath $Staging -Destination $Output
    } catch {
        if (Test-Path -LiteralPath $backup) {
            Move-Item -LiteralPath $backup -Destination $Output
        }
        throw
    }
    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Recurse -Force
    }
}
