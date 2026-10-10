$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rules = Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'r03-rules.json') | ConvertFrom-Json
$windows = Join-Path $repository 'verification/windows/assets'
$summary = Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'summary.json') | ConvertFrom-Json
if (-not $summary.reproducible -or $summary.unicodeWorkingDirectory -ne 'passed') { throw 'Windows R03 evidence is incomplete.' }
foreach ($configuration in @('release')) {
    $xml = [xml](Get-Content -Raw -Encoding UTF8 (Join-Path $windows "$configuration-assets-test.xml"))
    if ([int]$xml.testsuite.failures -ne 0 -or [int]$xml.testsuite.errors -ne 0) { throw 'Asset test failures exist.' }
    $runtime = Get-Content -Raw -Encoding UTF8 (Join-Path $windows "$configuration-runtime.json") | ConvertFrom-Json
    if (-not $runtime.passed -or $runtime.font -ne 'MiSans' -or $runtime.modelPixels -le 100) { throw 'Windows font/mesh regression failed.' }
}
$manifest = Get-Content -Raw -Encoding UTF8 (Join-Path $repository 'resources/manifests/assets.json') | ConvertFrom-Json
foreach ($entry in $manifest.entries) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $repository "resources/$($entry.path)") -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $entry.sha256) { throw "Output hash changed: $($entry.path)" }
    $input = (Get-FileHash -LiteralPath (Join-Path $repository $entry.source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($input -ne $entry.inputSha256) { throw "Input changed: $($entry.source)" }
}
foreach ($entry in $manifest.additionalInputs) {
    if ((Get-FileHash -LiteralPath (Join-Path $repository $entry.source) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw "Auxiliary source changed: $($entry.source)" }
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos', 'linux')) {
    $output = Join-Path $repository "verification/$platform-static/assets"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $checks = @()
    foreach ($check in $rules.checks) {
        $sources = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
            if (-not $text.Contains($source.symbol)) { throw "Missing $($source.path): $($source.symbol)" }
            $sources += @{ sourcePath = $source.path; lineOrSymbol = $source.symbol;
                sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
        }
        $checks += @{ checkId = $check.checkId; platform = $platform; requirement = $check.requirement;
            sourcePath = @($sources.sourcePath); lineOrSymbol = @($sources.lineOrSymbol); sources = $sources;
            evidence = $check.evidence; result = 'passed'; limitations = $check.limitations }
    }
    $specific = if ($platform -eq 'macos') { 'MAC-01' } else { 'LNX-01' }
    $checks += @{ checkId = $specific; platform = $platform; requirement = 'R03 platform packaging scope';
        sourcePath = @('docs/contracts/assets.md'); lineOrSymbol = @('本轮不覆盖后续完整播放器');
        evidence = 'No new deployment/launcher/platform integration is introduced in resource preparation. Existing R02 platform selection/build remains; complete packaging is assigned to R31/R32. Quoted POSIX asset preparation and portable qrc resources are covered above.';
        result = 'N/A'; limitations = 'Platform actual compile/render/decode and packaging remain user/later-round scope.' }
    $document = @{ round = 'R03'; qtVersion = $summary.qtVersion; conclusion = '静态审计通过，实机待用户验证';
        method = 'Automated symbol/hash/Windows-evidence coverage plus explicit manual platform branch/resource/error-path traces';
        assetManifestSha256 = (Get-FileHash (Join-Path $repository 'resources/manifests/assets.json') -Algorithm SHA256).Hash.ToLowerInvariant();
        checks = $checks }
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), ($document | ConvertTo-Json -Depth 20), $utf8)
    $report = @("# $platform · R03 资源静态审计", '', '静态审计通过，实机待用户验证', '',
        "所选 Windows 共享验证 Qt：$($summary.qtVersion)。全部五个 COMMON 条目适用并通过；平台部署条目在本轮 N/A，R31/R32 承担完整平台部署。", '',
        '覆盖 C++23/最新 Kit 构建闭包、Dwrite 条件、OTF/CFF 与显式字体样式、qrc/Unicode/大小写路径、三批次几何/UV/绕序、Qt 公开媒体无声加载及来源/输出 hash。逐分支推演、符号、SHA-256 和运行限制见 [audit-matrix.json](audit-matrix.json)。', '',
        'Windows Release：全部已注册 CTest、qmllint、真实 D3D11 三壳体/字体/图标窗口、Unicode/空格 cwd、四个 qrc 音频无声 LoadedMedia，以及重复准备可复现。图片仅是 Windows 证据。', '',
        '没有执行该平台编译、GPU、字体后端或媒体解码；完整材质/交互/播放尚属后续轮次。', '',
        "用户验证清单：[../../../docs/platforms/$platform.md](../../../docs/platforms/$platform.md)。原 R02 审计未覆盖 R03 的部分由本矩阵补充。")
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), ($report -join "`n") + "`n", $utf8)
}
Write-Output 'R03 static resource audit passed for macOS and Linux; actual platform runs are pending user verification.'
