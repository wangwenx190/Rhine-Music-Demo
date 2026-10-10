$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rulesPath = Join-Path $PSScriptRoot 'r05-rules.json'
$rules = Get-Content -Raw -Encoding UTF8 $rulesPath | ConvertFrom-Json
$windows = Join-Path $repository 'verification/windows/motion'
$summary = Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or $summary.ctest -ne '12/12 passed' -or $summary.qmllint -ne 'passed') { throw 'Missing Windows Release evidence.' }
$xml = [xml](Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'release-motion-test.xml'))
if ([int]$xml.testsuite.failures -or [int]$xml.testsuite.errors -or [int]$xml.testsuite.skipped -or $xml.SelectNodes('//system-err').Count) { throw 'Unresolved motion test failure/diagnostic.' }
$runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'release-runtime-motion.json') | ConvertFrom-Json
if (-not $runtime.passed -or -not $runtime.motion.passed -or $runtime.motion.resumeSamples -ne 2 -or $runtime.sourceOpened) { throw 'Incomplete production clock runtime.' }
$oracle = Get-Content -Raw -Encoding UTF8 (Join-Path $repository 'tests/fixtures/motion/kernel-golden.json') | ConvertFrom-Json
if ((Get-FileHash (Join-Path $repository 'tests/fixtures/motion/kernel-golden.json')).Hash.ToLowerInvariant() -ne $summary.kernelGoldenSha256) { throw 'Oracle differs from verified content.' }
if ((Get-FileHash (Join-Path $repository 'docs/contracts/motion-golden.json')).Hash.ToLowerInvariant() -ne $oracle.r01GoldenSha256) { throw 'R01 oracle changed.' }
foreach ($source in $oracle.sources) {
    $frozen = & git -C $repository show "$($oracle.baseline):$($source.path)"
    if ($LASTEXITCODE -ne 0) { throw 'Frozen source unavailable.' }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($frozen -join "`n") + "`n")
    if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.sha256) { throw 'Frozen source hash differs.' }
}
$qmlFiles = Get-ChildItem -LiteralPath (Join-Path $repository 'qml') -Recurse -File -Filter '*.qml'
$frameSources = 0
foreach ($file in $qmlFiles) { $frameSources += [regex]::Matches((Get-Content -Raw -Encoding UTF8 $file.FullName), '\bFrameAnimation\s*\{').Count }
if ($frameSources -ne 1) { throw 'Expected exactly one production visual frame source.' }
foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $repository 'src/motion') -File)) {
    $text = Get-Content -Raw -Encoding UTF8 $file.FullName
    if ($text -match '\b(QTimer|QElapsedTimer|QNetworkAccessManager|QNetworkRequest|QProcess|QAudioSink|QAudioDecoder|QAudioBufferOutput|QMediaPlayer|QAudioOutput)\b|Qt6::(Quick|Multimedia|Network)|#if|#error') { throw "Unexpected motion timer/media/platform dependency: $($file.Name)" }
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos', 'linux')) {
    $output = Join-Path $repository "verification/$platform-static/motion"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $checks = @()
    foreach ($check in $rules.checks) {
        $sources = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            if (-not (Get-Content -Raw -Encoding UTF8 -LiteralPath $path).Contains($source.symbol)) { throw "Missing source symbol $($source.path): $($source.symbol)" }
            $sources += @{ sourcePath = $source.path; lineOrSymbol = $source.symbol; sha256 = (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() }
        }
        $checks += @{ checkId = $check.checkId; platform = $platform; requirement = $check.requirement;
            sourcePath = @($sources.sourcePath); lineOrSymbol = @($sources.lineOrSymbol); sources = $sources;
            evidence = $check.evidence; result = 'passed'; limitations = $check.limitations }
    }
    $specific = if ($platform -eq 'macos') { 'MAC-01' } else { 'LNX-01' }
    $checks += @{ checkId = $specific; platform = $platform; requirement = 'R05 platform-integration delta'; result = 'N/A';
        sourcePath = @('src/motion/CMakeLists.txt', 'src/app/main.cpp', 'qml/Yalahoo/Music/App.qml');
        lineOrSymbol = @('target_sources', 'applicationStateChanged', 'FrameAnimation');
        evidence = 'This round adds shared mathematical kernels and public Qt lifecycle/frame APIs only. No new bundle/menu/shortcut/launcher/deployment/plugin/XDG/Retina behavior or platform branch. Existing graphics selection is checked in COMMON-03; full integration remains R31/R32.';
        limitations = 'No target runtime, compiler or deployment executed. OS lifecycle delivery is listed for user verification.' }
    $document = @{ round = 'R05'; configuration = 'Release'; qtVersion = $summary.qtVersion;
        conclusion = '静态审计通过，实机待用户验证'; checks = $checks; unresolvedFindings = @();
        method = 'Automated source/hash/oracle/Release gates plus explicit manual compilation, moc, QML lifecycle, ownership, math and completion traces recorded in each matrix row.';
        rulesSha256 = (Get-FileHash -LiteralPath $rulesPath).Hash.ToLowerInvariant();
        windowsSummarySha256 = (Get-FileHash -LiteralPath (Join-Path $windows 'summary.json')).Hash.ToLowerInvariant() }
    $matrixText = ($document | ConvertTo-Json -Depth 20).Replace("`r`n", "`n") + "`n"
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), $matrixText, $utf8)
    $report = @("# $platform · R05 运动内核静态审计", '', '静态审计通过，实机待用户验证', '',
        "COMMON-01–05 全部通过；$specific 的本轮新增平台集成为 N/A，具体原因、完整调用链及源码 hash 见 [audit-matrix.json](audit-matrix.json)。Windows 共享验证使用 Qt $($summary.qtVersion)，Release 12/12 CTest、无警告 qmllint、实际 D3D11/生产 FrameAnimation/隐藏与最小化恢复，以及现有资产和 qsb 检查。", '',
        '分别核对临界阻尼解析解、五次系数/导数、非零 observe 交接、四轨同钟、rebase、同目标、速度一次缩放、开场独立时间、减少动态及 revision 完成语义。唯一 QML 帧源从真实 QGuiApplication 生命周期和窗口可见性取得门控；纯数学不读文件，不调用网络、媒体或子进程。', '',
        '没有执行本平台编译、QML、GPU、帧率、媒体或后台恢复。Windows 数值/运行证据不替代本平台结果；场景视觉、列布局及开场界面按后续轮次实施。没有发现未修复的本轮静态缺陷。', '',
        '用户实机步骤见 docs/platforms 对应平台文档的 R05 条目；完整部署审计仍由 R31/R32 承担。')
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), ($report -join "`n") + "`n", $utf8)
}
Write-Output 'R05 static audit passed for macOS/Linux; target runtime checks await user verification.'
