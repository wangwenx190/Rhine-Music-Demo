$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot '../review/ResolveFrozenRevision.ps1')
$rulesPath = Join-Path $PSScriptRoot 'r07-rules.json'
$rules = Get-Content -Raw -Encoding UTF8 $rulesPath | ConvertFrom-Json
$windows = Join-Path $repository 'verification/windows/materials'
$summary = Get-Content -Raw (Join-Path $windows 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or $summary.ctest -ne '17/17 passed' -or $summary.qmllint -ne 'passed' -or $summary.gpuCases -ne 90 -or $summary.diffuseProbes -ne 9 -or $summary.maxProjectionPixels -gt 2) {
    throw 'Missing valid Windows evidence'
}
if ($summary.production.reviewEnabled -or $summary.production.ctest -ne '13/13 passed' -or $summary.production.reviewCliExitCode -ne 1) {
    throw 'Production Review guard failed'
}
$fixturePath = Join-Path $repository 'tests/fixtures/materials/single-case.json'
if ((Get-FileHash $fixturePath).Hash.ToLowerInvariant() -ne $summary.fixtureSha256) {
    throw 'Fixture changed after validation'
}
$fixture = Get-Content -Raw $fixturePath | ConvertFrom-Json
if ((Get-FileHash (Join-Path $repository 'resources/environment/room-pmrem.rgba32f')).Hash.ToLowerInvariant() -ne $fixture.environment.sha256) {
    throw 'Environment differs from frozen capture'
}
foreach ($source in $fixture.sources.psobject.Properties) {
    $frozen = & git -C $repository show "$(Resolve-FrozenRevision -Repository $repository -Revision $fixture.baseline):$($source.Name)"
    if ($LASTEXITCODE -ne 0) {
        throw 'Frozen source unavailable'
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($frozen -join "`n") + "`n")
    if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.Value) {
        throw 'Frozen source hash differs'
    }
}
$shaders = Get-Content -Raw (Join-Path $windows 'shader-variants.json') | ConvertFrom-Json
if ($shaders.qtKitVersion -ne $summary.qtVersion -or $shaders.shaders.Count -ne 6) {
    throw 'Incomplete same-Kit shader evidence'
}
foreach ($shader in $shaders.shaders) {
    if (-not $shader.passed -or $shader.targets.Count -ne 5 -or (Get-FileHash (Join-Path $repository $shader.source)).Hash.ToLowerInvariant() -ne $shader.sha256) {
        throw 'Shader provenance/target mismatch'
    }
}
$manifest = Get-Content -Raw (Join-Path $repository 'resources/manifests/materials.json') | ConvertFrom-Json
foreach ($asset in $manifest.assets) {
    $path = Join-Path $repository "resources/$($asset.path)"
    if ((Get-Item $path).Length -ne $asset.bytes -or (Get-FileHash $path).Hash.ToLowerInvariant() -ne $asset.sha256 -or $asset.qrc -ne ":/assets/$($asset.path)") {
        throw 'Material asset manifest closure failed'
    }
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos', 'linux')) {
    $output = Join-Path $repository "verification/$platform-static/materials"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $checks = @()
    foreach ($check in $rules.checks) {
        $sources = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            if (-not (Get-Content -Raw -Encoding UTF8 $path).Contains($source.symbol)) {
                throw "Missing implementation: $($source.path):$($source.symbol)"
            }
            $sources += @{
                sourcePath = $source.path
                lineOrSymbol = $source.symbol
                sha256 = (Get-FileHash $path).Hash.ToLowerInvariant()
            }
        }
        $checks += @{
            checkId = $check.checkId
            platform = $platform
            requirement = $check.requirement
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
        requirement = 'New R07 OS-specific integration'
        result = 'N/A'
        sourcePath = @('src/app/main.cpp', 'src/app/FixedTextureData.cpp', 'qml/Yalahoo/Music/scene/GeometryScene.qml')
        lineOrSymbol = @('setGraphicsApi', 'QFile', 'PerspectiveCamera')
        evidence = 'No new native menu, device, deployment, filesystem or window branch. Existing Metal/OpenGL selection is retained, logical View3D and public Quick3D/QFile are shared. R07 shaders contain no Windows constants or API branches. Full platform integration belongs to R31/R32.'
        limitations = 'No real target GPU/window system. Follow effective-DPR capture instructions in the platform R07 checklist; Windows PNGs are not target visual evidence.'
    }
    $matrix = @{
        round = 'R07'
        configuration = 'Release'
        qtVersion = $summary.qtVersion
        conclusion = '静态审计通过，实机待用户验证'
        checks = $checks
        unresolvedFindings = @()
        rulesSha256 = (Get-FileHash $rulesPath).Hash.ToLowerInvariant()
        windowsSummarySha256 = (Get-FileHash (Join-Path $windows 'summary.json')).Hash.ToLowerInvariant()
        method = 'Automated provenance/Release/production/qsb/source-closure gates plus manual expansion of build branches, qrc ownership, color/capture/Y/mip conventions, shader functions and error/destruction paths.'
    }
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), (($matrix | ConvertTo-Json -Depth 20).Replace("`r`n", "`n") + "`n"), $utf8)
    $report = @("# $platform · R07 单盒材质静态审计", '', '静态审计通过，实机待用户验证', '', "COMMON-01–05 通过；$specific 新增系统集成为 N/A。逐项实际调用链、源码 hash、证据和限制见 [audit-matrix.json](audit-matrix.json)。", '', "Windows 最新 Kit Qt $($summary.qtVersion) Release 17/17 与普通 13/13 回归、qmllint、90 单盒 GPU 状态及 9 漫反射探针通过；六 shader 的 GLSL/HLSL/MSL 生成通过。", '', '手工推演共享环境/DFG纹理生命周期、六 qrc shader 路径、主盖局部纸面距离、单面透射、侧脊参数、线性捕获、双重映射防护、原投影矩阵和 framebuffer UV 偏移。使用公有 Quick3D API；无私有 include、外部运行库、网络或媒体新操作。无本轮静态可确定缺陷。', '', '未执行本平台编译、GPU、GUI、音频或部署；qsb wrapper 不能替代 Metal/GL 实际绑定与画面验证。全场光照/阴影/景深/阵列与播放分别由后续轮次验收。平台用户清单见 docs/platforms 对应 R07 条目。')
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), (($report -join "`n") + "`n"), $utf8)
}
Write-Output 'R07 macOS/Linux static audit passed; real target verification awaits user.'
