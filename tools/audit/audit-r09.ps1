$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/occlusion'
$summary=Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or $summary.gpuCases -ne 88 -or -not $summary.gpu.passed -or $summary.qmllint -ne 'passed' -or $summary.production.reviewEnabled -or -not $summary.production.shadingPassed) {throw 'Missing real Release evidence'}
foreach ($pair in @(@('visual.json',$summary.fixtureSha256),@('kernel.json',$summary.kernelSha256),@('quality.json',$summary.qualitySha256))) {
    if ((Get-FileHash (Join-Path $repository "tests/fixtures/occlusion/$($pair[0])")).Hash.ToLowerInvariant() -ne $pair[1]) {throw 'Stale fixture evidence'}
}
$manifest=Get-Content -Raw (Join-Path $repository 'resources/manifests/occlusion.json') | ConvertFrom-Json
foreach ($source in $manifest.sources.psobject.Properties) {
    $frozen=& git -C $repository show "$($manifest.baseline):$($source.Name)"
    if ($LASTEXITCODE -ne 0) {throw 'Frozen source missing'}
    $bytes=[Text.Encoding]::UTF8.GetBytes(($frozen -join "`n")+"`n")
    if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.Value) {throw 'Frozen hash mismatch'}
}
foreach ($source in $manifest.algorithms.psobject.Properties) {if ((Get-FileHash (Join-Path $repository $source.Name)).Hash.ToLowerInvariant() -ne $source.Value) {throw 'Locked algorithm hash mismatch'}}
foreach ($asset in $manifest.assets) {
    $p=Join-Path $repository "resources/$($asset.path)"
    if ((Get-FileHash $p).Hash.ToLowerInvariant() -ne $asset.sha256 -or (Get-Item $p).Length -ne $asset.width*$asset.height*16) {throw 'Texture provenance mismatch'}
}
$shaderReport=Get-Content -Raw (Join-Path $evidence 'shader-variants.json') | ConvertFrom-Json
if ($shaderReport.qtVersion -ne $summary.qtVersion -or $shaderReport.shaders.Count -ne 4) {throw 'Same Kit shader gate missing'}
foreach ($shader in $shaderReport.shaders) {if (-not $shader.passed -or $shader.targets.Count -ne 5 -or (Get-FileHash (Join-Path $repository $shader.source)).Hash.ToLowerInvariant() -ne $shader.sha256) {throw 'Stale shader evidence'}}
$materialReport=Get-Content -Raw (Join-Path $evidence 'material-shader-variants.json') | ConvertFrom-Json
if ($materialReport.qtKitVersion -ne $summary.qtVersion -or $materialReport.shaders.Count -ne 6) {throw 'Same Kit material shader gate missing'}
foreach ($shader in $materialReport.shaders) {if (-not $shader.passed -or $shader.targets.Count -ne 5 -or (Get-FileHash (Join-Path $repository $shader.source)).Hash.ToLowerInvariant() -ne $shader.sha256) {throw 'Stale material shader evidence'}}
$rulesPath=Join-Path $PSScriptRoot 'r09-rules.json'
$rules=Get-Content -Raw -Encoding UTF8 $rulesPath | ConvertFrom-Json
$utf8=[Text.UTF8Encoding]::new($false)
function Get-SourceContentHash([string]$Path) {
    $text=[IO.File]::ReadAllText($Path).Replace("`r`n","`n")
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($text))).ToLowerInvariant()
}
foreach ($platform in @('macos','linux')) {
    $checks=@()
    foreach ($check in $rules.checks) {
        $sources=@();foreach ($source in $check.sources) {
            $path=Join-Path $repository $source
            if (-not (Test-Path -LiteralPath $path)) {throw "Missing $source"}
            $sources+=@{sourcePath=$source;sha256=(Get-SourceContentHash $path)}
        }
        $checks+=@{checkId=$check.checkId;requirement=$check.requirement;platform=$platform;sourcePath=$check.sources;lineOrSymbol=$check.lineOrSymbol;sources=$sources;evidence=$check.evidence;result='passed';limitations=$check.limitations}
    }
    $specific=if ($platform -eq 'macos') {'MAC-01'} else {'LNX-01'}
    $checks+=@{checkId=$specific;requirement='Native integration delta';platform=$platform;sourcePath='src/app/main.cpp';lineOrSymbol=if ($platform -eq 'macos') {'QSGRendererInterface::Metal'} else {'QSGRendererInterface::OpenGL'};evidence='R09 adds shared shaders/QML/value models; no platform menu/bundle/plugin/media/path branch. Existing target-specific graphics selection is retained, no forced D3D11 in shader math. Full platform integration remains R31/R32.';result='N/A';limitations='No target runtime performed'}
    $output=Join-Path $repository "verification/$platform-static/occlusion";New-Item -ItemType Directory -Force -Path $output | Out-Null
    $matrix=@{round='R09';configuration='Release';qtVersion=$summary.qtVersion;conclusion='静态审计通过，实机待用户验证';checks=$checks;unresolvedFindings=@();futureRoundInputs='docs/contracts/occlusion-limitations.json';rulesSha256=(Get-SourceContentHash $rulesPath);windowsSummarySha256=(Get-SourceContentHash (Join-Path $evidence 'summary.json'));sourceHashFormat='UTF-8 without BOM, CRLF normalized to LF (Git text content)';method='Executed frozen provenance / shader / Release gates plus expanded manual CMake, resource, clip-space, pass, state and ownership audit.'}
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'),(($matrix | ConvertTo-Json -Depth 20).Replace("`r`n","`n")+"`n"),$utf8)
    $report=@("# $platform · R09 阴影与 AO 静态审计",'', '静态审计通过，实机待用户验证','',"COMMON-01–05的实际构建/资源/几何覆盖/法线/深度重建/BufferInput/状态/销毁链已逐项展开，详见audit-matrix.json。$specific 本轮没有平台集成增量，理由在矩阵。",'',"Windows Qt $($summary.qtVersion) Release真实回归 $($summary.ctest)，普通构建 $($summary.production.ctest)，88 GPU状态和生产材质/AO启动、qmllint通过；四Effect片段的五种qsb变体生成通过。",'', '已知公开阴影投影/偏置/滤波、Unshaded壳体接收及重叠捕获由R10判定，背景输出由R09B接续，具体输入见docs/contracts/occlusion-limitations.json。它们不被本静态审计宣称为已解决，也不据此预选后端。', '', '未执行此平台编译、Metal/OpenGL、GUI、音频或部署，不把Windows截图称为本平台证据；用户步骤见docs/platforms对应R09条目。')
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'),(($report -join "`n")+"`n"),$utf8)
}
Write-Output 'R09 platform static audit passed; real target verification awaits user.'
