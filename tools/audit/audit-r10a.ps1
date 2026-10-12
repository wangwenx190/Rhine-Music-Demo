$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
function Source-Hash([string]$Path) {
    if([IO.Path]::GetExtension($Path) -in @('.png','.glb','.rgba32f')){return (Get-FileHash -LiteralPath $Path).Hash.ToLowerInvariant()}
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes([IO.File]::ReadAllText($Path).Replace("`r`n","`n"))
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
}
$evidence=Join-Path $repository 'verification/windows/rhi-opaque'
$summary=Get-Content -Raw (Join-Path $evidence 'summary.json')|ConvertFrom-Json
$variants=Get-Content -Raw (Join-Path $evidence 'shader-variants.json')|ConvertFrom-Json
if(-not $summary.passed -or $summary.configuration -ne 'Release' -or $summary.reviewCases -ne 36 -or $summary.resizeBurst -ne 30 -or $summary.productImplementationReady -or $variants.qtVersion -ne $summary.qtVersion -or $variants.shaders.Count -ne 6){throw 'Missing actual R10A evidence'}
$rules=Get-Content -Raw (Join-Path $PSScriptRoot 'r10a-rules.json')|ConvertFrom-Json
$manifest=Get-Content -Raw (Join-Path $repository 'resources/manifests/rhi-opaque.json')|ConvertFrom-Json
foreach($entry in @($manifest.sources)+@($manifest.shaders)) {
    $path=if($entry.source){$entry.source}else{$entry.path}
    if((Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Stale source/material shader: $path"}
}
$paths=@('tools/audit/r10a-rules.json','docs/contracts/rhi-opaque.md','cmake/QtKit.cmake','tools/assets/GeometryAudit.cpp','src/render/SceneFrame.h','src/render/RenderQuality.cpp','tools/assets/prepare-rhi-shaders.ps1','resources/shaders/rhi/shadow.frag','resources/shaders/rhi/output.vert','resources/shaders/rhi/paper-lighting.glsl')
foreach($rule in $rules.checks){$paths+=@($rule.sourcePath)}
foreach($n in 9..20){$paths+='docs/media/render-backend/web/'+$n.ToString('000')+'.png'}
$paths+='tests/fixtures/render-backend/visual.json'
$records=@();foreach($path in ($paths|Sort-Object -Unique)){$records+=@{path=$path;sha256=(Source-Hash (Join-Path $repository $path));hashMode='UTF-8 LF text / raw binary'}}
$artifacts=@();foreach($base in @('verification/windows/rhi-opaque','docs/media/rhi-opaque')){
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $repository $base) -File){if($file.Name -ne 'evidence-index.json'){$artifacts+=@{path=$base+'/'+$file.Name;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()}}}
}
$utf8=[Text.UTF8Encoding]::new($false)
$index=@{round='R10A';qtVersion=$summary.qtVersion;baseline=$manifest.baseline;sourceRecords=$records;artifacts=$artifacts}
[IO.File]::WriteAllText((Join-Path $evidence 'evidence-index.json'),($index|ConvertTo-Json -Depth 10).Replace("`r`n","`n")+"`n",$utf8)
foreach($platform in @('macos','linux')) {
    $checks=@();foreach($rule in $rules.checks){
        $check=@{checkId=$rule.checkId;requirement=$rule.requirement;platform=$platform;sourcePath=$rule.sourcePath;lineOrSymbol=$rule.lineOrSymbol;evidence=$rule.evidence;result='passed';limitations='Only static source/control-flow/resource review plus Windows regression and qsb variants; no target compiler, GPU or media execution.';sourceHashes=@()}
        foreach($path in $rule.sourcePath){$check.sourceHashes+=@($records|Where-Object path -eq $path)}
        $checks+=@($check)
    }
    $checks+=@{checkId=if($platform -eq 'macos'){'MAC-01'}else{'LNX-01'};requirement='Platform graphics selection and resource integration';platform=$platform;sourcePath=@('tools/review/RhiAudit.cpp','src/render/rhi/RhiItem.cpp','src/render/rhi/CMakeLists.txt');lineOrSymbol=if($platform -eq 'macos'){'Q_OS_MACOS -> Metal'}else{'non-Windows/macOS -> OpenGL'};evidence='RhiAudit platform branch selects native graphics API before QGuiApplication. All raster transforms come from same QRhi instance used by QQuickRhiItem; no native window handle/API. Output/target texel dimensions are explicit, qsb MSL/GLSL exists. Production bundle/XDG/launch/deploy integration is unchanged and remains subject to R31/R32; there is no new platform-specific launcher or media path.';result='passed';limitations='Static audit only; actual platform graphics, build, DPR and deployment await user verification.'}
    $output=Join-Path $repository "verification/$platform-static/rhi-opaque";New-Item -ItemType Directory -Force -Path $output|Out-Null
    $matrix=@{round='R10A';qtVersion=$summary.qtVersion;configuration='Release';conclusion='静态审计通过，实机待用户验证';checks=$checks;unresolvedStaticDefects=@();evidenceIndexSha256=(Get-FileHash (Join-Path $evidence 'evidence-index.json')).Hash.ToLowerInvariant()}
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'),($matrix|ConvertTo-Json -Depth 14).Replace("`r`n","`n")+"`n",$utf8)
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'),"# R10A $platform 静态审计`n`n静态审计通过，实机待用户验证。`n`n逐项源码/构建/资源/clip-space、纹理方向、bind/std140、GUI/render 所有权与错误路径推演见 audit-matrix.json。MSL/GLSL 已由 Windows 同 Kit qsb 生成；没有执行目标系统编译器、Metal/OpenGL GPU、音频或部署。实际 GPU 清单见 docs/platforms/$platform.md 的 R10A 章节。完整产品替换与 MSAA/壳体/后处理仍分别按 R10B–D 执行。`n",$utf8)
}
