$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rules = Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'r02-rules.json') | ConvertFrom-Json
$utf8 = [System.Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos', 'linux')) {
    $output = Join-Path $repository "verification/$platform-static"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $matrix = @()
    foreach ($check in $rules.checks | Where-Object { $platform -in $_.platforms }) {
        $sourceEvidence = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
            if (-not $text.Contains($source.symbol)) {
                throw "Missing $($source.path): $($source.symbol)"
            }
            $sourceEvidence += @{
                sourcePath = $source.path
                lineOrSymbol = $source.symbol

                sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
        $matrix += @{
            checkId = $check.checkId
            requirement = $check.requirement
            platform = $platform

            sourcePath = @($sourceEvidence.sourcePath)
            lineOrSymbol = @($sourceEvidence.lineOrSymbol)

            evidence = $check.evidence
            sources = $sourceEvidence
            result = 'passed'
            limitations = $check.limitations
        }
    }
    $cpp = Get-Content -Raw -Encoding UTF8 (Join-Path $repository 'src/app/main.cpp')
    if ($cpp -match 'QNetworkAccessManager|QProcess|QAudioSink|QAudioDecoder|QAudioBufferOutput') {
        throw 'Forbidden bootstrap dependency found.'
    }
    $shader = Get-Content -Raw -Encoding UTF8 (Join-Path $repository 'verification/windows/native-bootstrap/shader-variants.txt')
    foreach ($variant in @('MSL 12', 'GLSL 150', 'HLSL 50')) {
        if (-not $shader.Contains($variant)) {
            throw "Missing actual shader evidence: $variant"
        }
    }
    foreach ($configuration in @('release')) {
        $runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $repository "verification/windows/native-bootstrap/$configuration-runtime.json") | ConvertFrom-Json
        if (-not $runtime.passed -or $runtime.graphicsApi -ne 4 -or -not $runtime.mediaAvailable) {
            throw 'Windows shared regression is not passed.'
        }
    }
    $document = @{
        round = 'R02'
        qtVersion = $runtime.qtVersion
        conclusion = '静态审计通过，实机待用户验证'

        method = 'Automated source/hash/evidence coverage plus manually traced branch and resource chains in each evidence field'

        checks = $matrix
    }
    [System.IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), ($document | ConvertTo-Json -Depth 20), $utf8)
    $report = @("# $platform · R02 静态审计", '', '静态审计通过，实机待用户验证', '',
        '范围仅为原生最小窗口、Kit/构建/测试与模块探针。没有执行该平台的编译、图形或音频；本结论不覆盖后续轮次。', '',
        "共享证据所选 Qt：$($runtime.qtVersion)。源码版本以矩阵中每个文件的 SHA-256 锁定。", '',
        '逐项源码、符号、SHA-256、调用链推演和限制见 [audit-matrix.json](audit-matrix.json)。自动覆盖检查不能替代矩阵中的人工分支分析。', '')
    foreach ($check in $matrix) {
        $report += "- $($check.checkId): passed — $($check.requirement)"
    }
    $report += @('', '共享运行依据：Windows Release 全部 QtTest/Kit/runtime 检查、qmllint，以及实际 qsb 的 GLSL/HLSL/MSL 变体。Windows 截图仅是 Windows 证据。', '',
        "用户实机清单：[../../docs/platforms/$platform.md](../../docs/platforms/$platform.md)。后续 R31/R32 再补完整产品部署和平台验收。")
    [System.IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), ($report -join "`n"), $utf8)
}
Write-Output 'R02 static coverage and evidence checks passed for macOS and Linux.'
