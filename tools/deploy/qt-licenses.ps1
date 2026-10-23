# Copies the complete license bundle from the selected Qt installation.
function Copy-QtLicenses([string]$Kit, [string]$Destination) {
    $directory = [IO.DirectoryInfo]::new([IO.Path]::GetFullPath($Kit))
    while ($null -ne $directory) {
        $candidate = Join-Path $directory.FullName 'Licenses'
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            if (-not (Get-ChildItem -LiteralPath $candidate -Recurse -File)) {
                throw "Selected Qt installation license bundle is incomplete: $candidate"
            }
            Copy-Item -LiteralPath $candidate -Destination $Destination -Recurse -ErrorAction Stop
            return
        }
        $directory = $directory.Parent
    }
    throw 'Selected Qt installation license directory is missing.'
}
