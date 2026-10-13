function Resolve-FrozenRevision {
    param([string]$Repository, [string]$Revision)
    $manifest = Get-Content -Raw -LiteralPath (Join-Path $Repository 'docs/contracts/history-rewrite.json') | ConvertFrom-Json
    $entry = $manifest.revisions.PSObject.Properties[$Revision]
    if ($entry) {
        if ($entry.Value -notmatch '^[a-f0-9]{40}$') {
            throw 'Invalid frozen revision mapping'
        }
        return $entry.Value
    }
    return $Revision
}
