# R07 shared diffuse/PMREM math only. Frozen-source provenance is retained in materials.json.
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source = Get-Content -Raw (Join-Path $repository 'resources/shaders/album-paper.frag')
$source = $source.Substring($source.IndexOf('// BEGIN SHARED LIGHTING'), $source.IndexOf('// END SHARED LIGHTING') - $source.IndexOf('// BEGIN SHARED LIGHTING'))
# Shell transport belongs to R10B; do not bake an unused second shell implementation.
$start = $source.IndexOf('vec3 musicShell(')
$end = $source.IndexOf('#define CUBEUV_MAX_MIP', $start)
$source = $source.Remove($start, $end - $start)
$map = @{
    rowColumn = 'f.rowColumn.xyz'
    rowSettings = 'f.rowSettings'
    lightColumn = 'f.lightColumn.xyz'
    printAmbient = 'f.settings.x'
    rowFill = 'f.settings.z'
    rowDetail = 'f.settings.y'
    spotPosition = 'f.spotPosition.xyz'
    spotTarget = 'f.spotTarget.xyz'
    spotColor = 'f.spotColor.xyz'
    spotIntensity = 'f.optics.x'
    groundColor = 'f.groundColor.xyz'
    skyColor = 'f.skyColor.xyz'
    lightIntensities = 'f.lightIntensities.xyz'
    keyColor = 'f.keyColor.xyz'
}
foreach ($key in $map.Keys) {
    $source = [regex]::Replace($source, '\b' + $key + '\b', $map[$key])
}
$source = $source.Replace('Shared field, diffuse and shell transport', 'Shared field and diffuse transport').Replace("`r`n", "`n").TrimEnd() + "`n"
[IO.File]::WriteAllText((Join-Path $repository 'resources/shaders/rhi/paper-lighting.glsl'), $source, [Text.UTF8Encoding]::new($false))
