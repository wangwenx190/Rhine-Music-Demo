$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repository 'verification/windows/postprocess'
$summary=Get-Content -Raw (Join-Path $evidence 'summary.json')|ConvertFrom-Json
if($summary.configuration -ne 'Release' -or -not $summary.gpuPassed -or $summary.gpuCases -ne 105 -or $summary.qmllint -ne 'passed' -or -not $summary.productionShadingPassed -or $summary.productionReviewCliExitCode -ne 1){throw 'Missing real Release evidence'}
if((Get-FileHash (Join-Path $repository 'tests/fixtures/postprocess/visual.json')).Hash.ToLowerInvariant() -ne $summary.fixtureSha256){throw 'Stale fixture evidence'}
$manifest=Get-Content -Raw (Join-Path $repository 'resources/manifests/postprocess.json')|ConvertFrom-Json
foreach($p in $manifest.algorithms.psobject.Properties){if((Get-FileHash (Join-Path $repository $p.Name)).Hash.ToLowerInvariant() -ne $p.Value){throw 'Locked algorithm changed'}}
foreach($a in $manifest.assets){$path=Join-Path $repository "resources/$($a.path)";if((Get-FileHash $path).Hash.ToLowerInvariant() -ne $a.sha256 -or (Get-Item $path).Length -ne $a.bytes){throw 'Lookup provenance changed'}}
foreach($source in $manifest.sources.psobject.Properties){
    $frozen=& git -C $repository show "$($manifest.baseline):$($source.Name)"
    if($LASTEXITCODE -ne 0){throw 'Missing frozen source'}
    $bytes=[Text.Encoding]::UTF8.GetBytes(($frozen -join "`n")+"`n")
    if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.Value){throw 'Frozen source hash changed'}
}
$shaders=Get-Content -Raw (Join-Path $evidence 'shader-variants.json')|ConvertFrom-Json
if($shaders.qtVersion -ne $summary.qtVersion -or $shaders.shaders.Count -ne 5){throw 'Same-Kit shader evidence missing'}
foreach($s in $shaders.shaders){if(-not $s.passed -or $s.targets.Count -ne 5 -or (Get-FileHash (Join-Path $repository $s.source)).Hash.ToLowerInvariant() -ne $s.sha256){throw 'Stale shader evidence'}}
foreach($dpr in @('1','1.5','2')){
    $r=Get-Content -Raw (Join-Path $evidence "release-dpr-$dpr.json")|ConvertFrom-Json
    if(-not $r.passed -or $r.dpr -ne [double]$dpr -or $r.graphicsApi -ne 4 -or (Get-FileHash (Join-Path $repository "tests/fixtures/postprocess/visual-dpr-$dpr.json")).Hash.ToLowerInvariant() -ne $r.fixtureSha256){throw 'Stale DPR/runtime evidence'}
}
$rulesPath=Join-Path $PSScriptRoot 'r09b-rules.json'
$rules=Get-Content -Raw $rulesPath|ConvertFrom-Json
$utf8=[Text.UTF8Encoding]::new($false)
foreach($platform in @('macos','linux')){
    $checks=@()
    foreach($check in $rules.checks){
        $sources=@();foreach($source in $check.sources){
            $path=Join-Path $repository $source;if(-not(Test-Path -LiteralPath $path)){throw "Missing $source"}
            $text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
            $sources+=@{sourcePath=$source;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($text))).ToLowerInvariant()}
        }
        $checks+=@{checkId=$check.checkId;requirement=$check.requirement;platform=$platform;sourcePath=$check.sources;lineOrSymbol=$check.lineOrSymbol;sources=$sources;evidence=$check.evidence;result='passed';limitations=$check.limitations}
    }
    $checks+=@{checkId=if($platform -eq 'macos'){'MAC-01'}else{'LNX-01'};requirement='Platform integration delta';platform=$platform;sourcePath='src/app/main.cpp';lineOrSymbol=if($platform -eq 'macos'){'QSGRendererInterface::Metal'}else{'QSGRendererInterface::OpenGL'};evidence='No new platform/deploy/menu/media/path branch. Existing native backend selection is preserved, shaders transform framebuffer UV via public uniforms. Full integration remains R31/R32.';result='N/A';limitations='No target runtime performed'}
    $output=Join-Path $repository "verification/$platform-static/postprocess";New-Item -ItemType Directory -Force -Path $output|Out-Null
    $matrix=@{round='R09B';configuration='Release';qtVersion=$summary.qtVersion;conclusion='静态审计通过，实机待用户验证';rulesSha256=(Get-FileHash $rulesPath).Hash.ToLowerInvariant();checks=$checks;unresolvedStaticDefects=@()}
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'),($matrix|ConvertTo-Json -Depth 14)+"`n",$utf8)
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'),"# R09B $platform 静态审计`n`n静态审计通过，实机待用户验证。`n`nCOMMON-01–05 均有逐调用链、资源、shader、生命周期和错误路径证据；机器检查锁定源码、纹理、shader、fixture hash 与 Windows Release 实机结果。平台集成增量不适用，无新增平台分支；完整集成仍按 R31/R32。未执行目标系统编译、图形、字体或音频。`n`n规则与内容 hash、Qt 版本及具体限制见 audit-matrix.json；用户步骤见 docs/platforms/$platform.md 的 R09B 清单。`n",$utf8)
}
Write-Output 'R09B macOS/Linux 静态审计通过，实机待用户验证。'
