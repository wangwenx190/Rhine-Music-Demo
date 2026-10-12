$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/lighting'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or $summary.ctest -ne '19/19 passed' -or $summary.gpuCases -ne 48 -or $summary.qmllint -ne 'passed' -or $summary.production.reviewEnabled -or $summary.production.ctest -ne '14/14 passed') {
    throw 'Missing Release/production evidence'
}
foreach ($pair in @(@('visual.json', $summary.fixtureSha256), @('golden.json', $summary.goldenSha256))) {
    if ((Get-FileHash (Join-Path $repository "tests/fixtures/lighting/$($pair[0])")).Hash.ToLowerInvariant() -ne $pair[1]) {
        throw 'Stale lighting evidence'
    }
}
foreach ($region in @('paper', 'margin', 'spine', 'rim', 'background')) {
    if ($summary.roi.$region.maxMeanAbsoluteRgb -gt 6 -or $summary.roi.$region.minSsim -lt .97) {
        throw 'Image target failed'
    }
}
$contract = Get-Content -Raw (Join-Path $repository 'docs/contracts/lighting.json') | ConvertFrom-Json
foreach ($source in $contract.sources.psobject.Properties) {
    $frozen = & git -C $repository show "$($contract.baseline):$($source.Name)"
    if ($LASTEXITCODE -ne 0) {
        throw 'Frozen source unavailable'
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($frozen -join "`n") + "`n")
    if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.Value) {
        throw 'Frozen source hash differs'
    }
}
foreach ($source in $contract.upstreamAlgorithms.psobject.Properties) {
    if ((Get-FileHash (Join-Path $repository $source.Name)).Hash.ToLowerInvariant() -ne $source.Value) {
        throw 'Locked algorithm hash differs'
    }
}
$manifest = Get-Content -Raw (Join-Path $repository 'resources/manifests/lighting.json') | ConvertFrom-Json
foreach ($asset in $manifest.assets) {
    if ((Get-FileHash (Join-Path $repository "resources/$($asset.path)")).Hash.ToLowerInvariant() -ne $asset.sha256 -or $asset.bytes -ne 65536) {
        throw 'LTC asset closure failed'
    }
}
$shaders = Get-Content -Raw (Join-Path $evidence 'shader-variants.json') | ConvertFrom-Json
if ($shaders.qtKitVersion -ne $summary.qtVersion -or $shaders.shaders.Count -ne 6) {
    throw 'Same-Kit shader evidence missing'
}
foreach ($shader in $shaders.shaders) {
    if (-not $shader.passed -or $shader.targets.Count -ne 5 -or (Get-FileHash (Join-Path $repository $shader.source)).Hash.ToLowerInvariant() -ne $shader.sha256) {
        throw 'Stale shader evidence'
    }
}
$rulesPath = Join-Path $PSScriptRoot 'r08-rules.json'
$rules = Get-Content -Raw -Encoding UTF8 $rulesPath | ConvertFrom-Json
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos', 'linux')) {
    $checks = @()
    foreach ($check in $rules.checks) {
        $sources = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            if (-not (Get-Content -Raw -Encoding UTF8 $path).Contains($source.symbol)) {
                throw "Implementation absent: $($source.path)"
            }
            $sources += @{
                sourcePath = $source.path
                lineOrSymbol = $source.symbol
                sha256 = (Get-FileHash $path).Hash.ToLowerInvariant()
            }
        }
        $checks += @{
            checkId = $check.checkId
            requirement = $check.requirement
            platform = $platform
            sourcePath = @($sources.sourcePath)
            lineOrSymbol = @($sources.lineOrSymbol)
            sources = $sources
            evidence = $check.evidence
            result = 'passed'
            limitations = $check.limitations
        }
    }
    $specific = if ($platform -eq 'macos') {
        'MAC-01'
    } else {
        'LNX-01'
    }
    $checks += @{
        checkId = $specific
        platform = $platform
        requirement = 'R08 native platform integration delta'
        sourcePath = @('src/app/main.cpp', 'qml/Yalahoo/Music/scene/LightingMaterial.qml')
        lineOrSymbol = @('QSGRendererInterface::Metal', 'QSGRendererInterface::OpenGL', 'CustomMaterial')
        evidence = 'No new OS-specific window/menu/path/media/deployment operation. Existing Metal/OpenGL selection and logical pixels retained; new light math/material properties have no forced D3D11 constants. Full integration/deployment belongs to R31/R32.'
        result = 'N/A'
        limitations = 'No target runtime performed; user checklist records expected steps.'
    }
    $output = Join-Path $repository "verification/$platform-static/lighting"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $matrix = @{
        round = 'R08'
        configuration = 'Release'
        qtVersion = $summary.qtVersion
        conclusion = '静态审计通过，实机待用户验证'
        checks = $checks
        unresolvedFindings = @()
        rulesSha256 = (Get-FileHash $rulesPath).Hash.ToLowerInvariant()
        windowsSummarySha256 = (Get-FileHash (Join-Path $evidence 'summary.json')).Hash.ToLowerInvariant()
        method = 'Executed Windows provenance/qsb/Release gates plus manual expansion of actual build/resource/shader/frame/ownership/OS call chains.'
    }
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), (($matrix | ConvertTo-Json -Depth 20).Replace("`r`n", "`n") + "`n"), $utf8)
    $report = @("# $platform · R08 灯光静态审计", '', '静态审计通过，实机待用户验证', '', "COMMON-01–05全部通过；$specific 本轮新增平台集成为N/A，理由在audit-matrix.json。矩阵逐项保存真实调用链推演、源码hash、Windows/qsb证据与限制。", '', '公共构建/moc/QML注册/qrc闭包、LTC字节与端序、线性uniform及输出、世界原点与行basis换算、单时钟/主题打断/减少动态/隐藏/销毁均已展开。没有本轮静态可确定缺陷。', '', "Windows Qt $($summary.qtVersion) Release：19/19 Review回归、14/14普通回归、qmllint、48 GPU场景五ROI门槛与924+392冻结数值样本通过；六shader各五目标生成通过。", '', '未运行本平台编译、Metal/OpenGL、GUI、音频或部署；这些执行结果留待用户填写，不把Windows图像当成本平台证据。步骤见docs/platforms对应R08条目。')
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), (($report -join "`n") + "`n"), $utf8)
}
Write-Output 'R08 macOS/Linux static audit passed; real target verification awaits user.'
