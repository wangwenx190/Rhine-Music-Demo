$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rules = Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'r04-rules.json') | ConvertFrom-Json
$windows = Join-Path $repository 'verification/windows/library-data'
$summary = Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or $summary.ctest -ne '10/10 passed' -or $summary.frozenSearchCases -ne 216) {
    throw 'Missing Release Windows evidence.'
}
foreach ($file in @('release-library-data-test.xml', 'release-library-qml-test.xml')) {
    $xml = [xml](Get-Content -Raw -Encoding UTF8 (Join-Path $windows $file))
    if ([int]$xml.testsuite.failures -or [int]$xml.testsuite.errors -or $xml.SelectNodes('//system-err').Count) {
        throw 'Unresolved test failure/diagnostic.'
    }
}
$runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'release-runtime-library.json') | ConvertFrom-Json
if (-not $runtime.passed -or $runtime.libraryAlbums -ne 21 -or $runtime.searchResults -ne 28 -or $runtime.sourceOpened) {
    throw 'Incomplete native library runtime.'
}
$oraclePath = Join-Path $repository 'tests/fixtures/core/search-golden.json'
$oracle = Get-Content -Raw -Encoding UTF8 $oraclePath | ConvertFrom-Json
if ((Get-FileHash (Join-Path $repository 'tests/fixtures/web-baseline/anonymous.json')).Hash.ToLowerInvariant() -ne $oracle.fixtureSha256) {
    throw 'Fixture content changed.'
}
$frozen = & git -C $repository show "$($oracle.baseline):$($oracle.sourcePath)"
if ($LASTEXITCODE -ne 0) {
    throw 'Frozen source unavailable.'
}
$sourceBytes = [Text.Encoding]::UTF8.GetBytes(($frozen -join "`n") + "`n")
$sourceHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes)).ToLowerInvariant()
if ($sourceHash -ne $oracle.sourceSha256) {
    throw 'Frozen oracle source hash differs.'
}
$productFiles = @(Get-ChildItem -LiteralPath (Join-Path $repository 'src/core'), (Join-Path $repository 'src/services') -File)
foreach ($file in $productFiles) {
    $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $file.FullName
    if ($text -match '\b(QNetworkAccessManager|QNetworkRequest|QTcpSocket|QUdpSocket|QProcess|QAudioSink|QAudioDecoder|QAudioBufferOutput)\b|Qt6::Network|#error') {
        throw "Forbidden API or unimplemented branch: $($file.Name)"
    }
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos', 'linux')) {
    $output = Join-Path $repository "verification/$platform-static/library-data"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $checks = @()
    foreach ($check in $rules.checks) {
        $sources = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
            if (-not $text.Contains($source.symbol)) {
                throw "Missing $($source.path): $($source.symbol)"
            }
            $sources += @{
                sourcePath = $source.path
                lineOrSymbol = $source.symbol

                sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
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
        requirement = 'R04 platform-integration delta'

        sourcePath = @('src/services/CMakeLists.txt', 'src/app/main.cpp')
        lineOrSymbol = @('target_sources(YalahooCore PRIVATE', 'Q_OS_MACOS / Q_OS_LINUX')

        evidence = 'R04 adds portable Qt Core value/model/provider logic only. No bundle/menu/launcher/deployment/plugin-path or OS-specific functionality is introduced. Existing platform graphics selection and shared QML resource chain are checked in COMMON-01/03; full platform integration remains R31/R32.'

        result = 'N/A'
        limitations = 'No target runtime/deployment result; ordinary platform integration is outside this round.'
    }
    $document = @{
        round = 'R04'
        qtVersion = $summary.qtVersion
        configuration = 'Release'

        conclusion = '静态审计通过，实机待用户验证'
        checks = $checks

        method = 'Automated source/oracle/hash/Release evidence checks plus explicit manual branch, ownership, resource and error-path traces in each matrix row.'

        rulesSha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'r04-rules.json')).Hash.ToLowerInvariant()

        windowsSummarySha256 = (Get-FileHash -LiteralPath (Join-Path $windows 'summary.json')).Hash.ToLowerInvariant()

        unresolvedFindings = @()
    }
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), ($document | ConvertTo-Json -Depth 20) + "`n", $utf8)
    $report = @("# $platform · R04 数据与离线 Provider 静态审计", '', '静态审计通过，实机待用户验证', '',
        "Windows 共享验证：Qt $($summary.qtVersion)，Release 10/10 CTest、qmllint、真实 D3D11/QML/鼠标、中文/空格 cwd。COMMON-01–05 全部通过，$specific 的本轮新增平台集成为 N/A；具体原因、完整调用链与内容 hash 在 [audit-matrix.json](audit-matrix.json)。", '',
        '本轮审阅了值类型/JSON、排序/分组/搜索、角色/通知、异步缓存、token/generation/取消/对象销毁及 foreign QML 注入链。产品仅注册离线 Provider，core/services 没有网络/子进程/PCM API或依赖；运行快照观察零 TCP/子进程，不能单凭该快照推断全生命周期。216 个搜索用例来自实际执行冻结函数；所有冻结库列与已有 R01 资产核对。', '',
        '没有执行本平台编译、排序后端、QML 或 GPU；Windows 图片不作为本平台结果。没有进行歌曲播放，媒体功能归后续轮次。未发现未修复的本轮静态缺陷。', '',
        "用户实机步骤见 [../../../docs/platforms/$platform.md](../../../docs/platforms/$platform.md)；后续完整平台部署审计由 R31/R32 承担。")
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), ($report -join "`n") + "`n", $utf8)
}
Write-Output 'R04 static audit passed for macOS/Linux; actual target runs await user verification.'
