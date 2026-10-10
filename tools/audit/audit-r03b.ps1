$ErrorActionPreference='Stop'
$repository=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rules=Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'r03b-rules.json') | ConvertFrom-Json
$windows=Join-Path $repository 'verification/windows/branding'
$summary=Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'summary.json') | ConvertFrom-Json
if (-not $summary.reproducible) { throw 'Windows branding reproducibility evidence absent.' }
$export=Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'export-report.json') | ConvertFrom-Json
if (-not $export.passed -or -not $export.nativeWindow.exposed -or $export.svgInventory.Count -ne 13 -or $export.rasterCases.Count -ne 243) { throw 'Windows SVG/window evidence incomplete.' }
foreach ($configuration in @('release')) {
    $report=Get-Content -Raw -Encoding UTF8 (Join-Path $windows "$configuration-branding-report.json") | ConvertFrom-Json
    $runtime=Get-Content -Raw -Encoding UTF8 (Join-Path $windows "$configuration-runtime.json") | ConvertFrom-Json
    if (-not $report.passed -or -not $runtime.passed -or $report.qtVersion -ne $summary.qtVersion) { throw 'Shared Windows verification failed.' }
    $log=Get-Content -Raw -Encoding UTF8 (Join-Path $windows "$configuration-build.txt")
    if ($log -notmatch '100% tests passed(?:, 0 tests failed)? out of [0-9]+') { throw 'All registered Windows tests must actually pass.' }
}
$manifest=Get-Content -Raw -Encoding UTF8 (Join-Path $repository 'resources/manifests/branding.json') | ConvertFrom-Json
if ($manifest.entries.Count -ne 36) { throw 'Branding inventory differs.' }
foreach ($entry in $manifest.entries) {
    if ((Get-FileHash -LiteralPath (Join-Path $repository $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw "Brand hash differs: $($entry.path)" }
}
$shell=Join-Path $repository 'tools/assets/export-branding.sh'
$bytes=[IO.File]::ReadAllBytes($shell)
if ($bytes -contains 13 -or $bytes[0] -ne 35) { throw 'Shell must use LF and no BOM.' }
$mode=& git -C $repository ls-files --stage -- tools/assets/export-branding.sh
if ($LASTEXITCODE -ne 0 -or $mode -notmatch '^100755 ') { throw 'Shell needs Git executable mode.' }
$utf8=[Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos','linux')) {
    $output=Join-Path $repository "verification/$platform-static/branding"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $checks=@()
    foreach ($rule in $rules.checks) {
        $sources=@()
        foreach ($source in $rule.sources) {
            $path=Join-Path $repository $source.path
            if (-not (Get-Content -Raw -Encoding UTF8 -LiteralPath $path).Contains($source.symbol)) { throw "Missing symbol: $($source.path): $($source.symbol)" }
            $sources+=@{sourcePath=$source.path;lineOrSymbol=$source.symbol;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
        $checks+=@{checkId=$rule.checkId;requirement=$rule.requirement;platform=$platform;
            sourcePath=@($sources.sourcePath);lineOrSymbol=@($sources.lineOrSymbol);sources=$sources;
            evidence=$rule.evidence;result=$rule.result;limitations=$rule.limitations}
    }
    if ($platform -eq 'macos') {
        $specific=@{checkId='MAC-01';requirement='Portable ICNS / iconset generation';sourcePath=@('tools/assets/ExportBranding.cpp');lineOrSymbol=@('exportContainers','verifyContainers');
            evidence='Four explicit PNG chunks ic07/ic08/ic09/ic10 are serialized with qToBigEndian, block lengths include eight bytes and file length includes the file header. The decoder checks all lengths/order/pixels and EOF. Ten iconset PNGs cover five logical dimensions with @2x. No iconutil prerequisite or Apple-only API exists. Console utility disables MACOSX_BUNDLE explicitly; product bundle/Info.plist/macdeployqt integration is R31 and not introduced here.';
            result='passed';limitations='No macOS compile, iconutil or Finder run; product bundle integration is a later round.'}
    } else {
        $specific=@{checkId='LNX-01';requirement='Portable SVG/PNG and POSIX export utility';sourcePath=@('tools/assets/ExportBranding.cpp','tools/assets/export-branding.sh');lineOrSymbol=@('write(icons+"/yalahoo.svg"','exec "$repository/build/$configuration/tools/export-branding"');
            evidence='Linux SVG is an exact byte copy of production icon.svg, and nine RGBA PNG dimensions are generated and verified from the correct size-dependent source. Shell is UTF-8 without BOM, LF, Git mode 100755; root is derived from its own dirname, paths and all extra arguments are quoted. CMake/Ninja selects the latest installed native Kit and embeds its Qt runtime linkage. No system install, driver or audio mutation occurs. Desktop file/plugin/shared-library packaging is R32 and not introduced here.';
            result='passed';limitations='No Linux compile, desktop icon theme or native filesystem execution; product deployment is a later round.'}
    }
    $specific.platform=$platform
    $specific.sources=@($specific.sourcePath | ForEach-Object { @{sourcePath=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $repository $_) -Algorithm SHA256).Hash.ToLowerInvariant()} })
    $checks+=$specific
    $document=@{round='R03B';qtVersion=$summary.qtVersion;conclusion='静态审计通过，实机待用户验证';
        method='Automated file/symbol/hash/container/Windows evidence checks plus recorded manual branch/resource/error-path traces';
        manifestSha256=(Get-FileHash (Join-Path $repository 'resources/manifests/branding.json') -Algorithm SHA256).Hash.ToLowerInvariant();checks=$checks}
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'),($document | ConvertTo-Json -Depth 25)+"`n",$utf8)
    $text=@("# $platform · R03B 品牌资产静态审计",'', '静态审计通过，实机待用户验证','',
        '适用 COMMON-01/02/03/05 及平台图标交付项通过；COMMON-04 的媒体规则不适用于品牌工具，预览窗口生命周期已审阅。实际调用链、错误分支、hash 和限制见 [audit-matrix.json](audit-matrix.json)。','',
        "共享 Windows Qt $($summary.qtVersion)：Release 全部已注册测试、QML lint、13 SVG、243 渲染、原生 Qt 窗口、全部 ICO/ICNS PNG 块及资源可复现性通过。Windows 图像不作为本平台运行证据。",'',
        '本轮新增工具仅依赖公共 QtGui/QtSvg，不含平台私有分支；字体无依赖，衍生容器可在 Windows 生成。未执行本平台构建、桌面图标加载或 GUI；主界面和系统集成留给对应后续轮次。','',
        "实机命令与预期结果见 [平台清单](../../../docs/platforms/$platform.md) 的 R03B 小节。") -join "`n"
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'),$text+"`n",$utf8)
}
Write-Output 'R03B static audit passed for macOS and Linux; actual platform verification pending.'
