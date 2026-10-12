[CmdletBinding()]
param([switch]$Check)

$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path

# Reuse an installed development formatter; never download or add runtime libraries.
if (-not (Get-Module -ListAvailable PSScriptAnalyzer)) {
    $extensionRoots = @(
        (Join-Path $env:USERPROFILE '.vscode/extensions'),
        (Join-Path $env:USERPROFILE '.cursor/extensions')
    )
    $modules = foreach ($extensionRoot in $extensionRoots) {
        if (Test-Path -LiteralPath $extensionRoot) {
            Get-ChildItem -LiteralPath $extensionRoot -Filter 'ms-vscode.powershell-*' -Directory |
                ForEach-Object {
                    Get-ChildItem -LiteralPath (Join-Path $_.FullName 'modules/PSScriptAnalyzer') `
                        -Filter PSScriptAnalyzer.psd1 -Recurse -ErrorAction SilentlyContinue
                }
        }
    }
    $module = $modules | Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $module) {
        throw 'Install/locate PSScriptAnalyzer to format maintenance PowerShell sources.'
    }
    Import-Module $module.FullName
} else {
    Import-Module PSScriptAnalyzer
}

$settings = Join-Path $PSScriptRoot 'PowerShellStyle.psd1'
$encoding = [Text.UTF8Encoding]::new($false)
$failures = @()
$sources = Get-ChildItem -LiteralPath (Join-Path $repository 'tools') -Recurse -File |
    Where-Object { $_.Extension -in @('.ps1', '.psd1') }
foreach ($source in $sources) {
    $original = [IO.File]::ReadAllText($source.FullName)
    $tokens = $null
    $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseInput($original, [ref]$tokens, [ref]$errors)
    if ($errors) {
        throw "Invalid PowerShell source: $($source.FullName): $errors"
    }
    # Semicolons in for headers are syntax. Other separators can become newlines.
    $loops = $ast.FindAll({
            param($node)
            $node -is [Management.Automation.Language.ForStatementAst]
        }, $true)
    $expanded = $original
    $newline = if ($original.Contains("`r`n")) {
        "`r`n"
    } else {
        "`n"
    }
    foreach ($token in ($tokens | Sort-Object { $_.Extent.StartOffset } -Descending)) {
        if ($token.Kind -eq 'AtCurly') {
            $offset = $token.Extent.EndOffset
            $end = $offset
            while ($end -lt $original.Length -and $original[$end] -eq ' ') {
                ++$end
            }
            if ($end -lt $original.Length -and $original[$end] -notin @("`r", "`n", '}')) {
                $expanded = $expanded.Remove($offset, $end - $offset).Insert($offset, $newline)
            }
            continue
        }
        if ($token.Kind -ne 'Semi') {
            continue
        }
        $offset = $token.Extent.StartOffset
        $inHeader = $loops | Where-Object {
            $offset -ge $_.Extent.StartOffset -and $offset -lt $_.Body.Extent.StartOffset
        }
        if (-not $inHeader) {
            $expanded = $expanded.Remove($offset, 1).Insert($offset, $newline)
        }
    }
    $formatted = Invoke-Formatter -ScriptDefinition $expanded -Settings $settings
    $formatted = (($formatted -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n")
    $formatted = $formatted.TrimEnd() + "`n"
    $formatted = $formatted.Replace("`r`n", "`n").Replace("`n", "`r`n")

    # A layout pass must preserve every non-separator token, including strings.
    $afterTokens = $null
    $afterErrors = $null
    $null = [Management.Automation.Language.Parser]::ParseInput(
        $formatted, [ref]$afterTokens, [ref]$afterErrors
    )
    $before = ($tokens | Where-Object { $_.Kind -notin @('NewLine', 'LineContinuation', 'Semi', 'EndOfInput') } |
            ForEach-Object { "$($_.Kind):$($_.Text)" }) -join "`n"
    $after = ($afterTokens | Where-Object { $_.Kind -notin @('NewLine', 'LineContinuation', 'Semi', 'EndOfInput') } |
            ForEach-Object { "$($_.Kind):$($_.Text)" }) -join "`n"
    if ($afterErrors -or $before -cne $after) {
        throw "PowerShell formatting changed tokens: $($source.FullName)"
    }
    if ($original -cne $formatted) {
        if ($Check) {
            $failures += $source.FullName
        } else {
            [IO.File]::WriteAllText($source.FullName, $formatted, $encoding)
        }
    }
}
if ($failures) {
    throw "Run tools/format/FormatTools.ps1 to format:`n$($failures -join "`n")"
}
Write-Output "Qt tool style: $($sources.Count) PowerShell sources (check=$Check)"
