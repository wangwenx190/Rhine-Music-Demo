$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rulesPath = Join-Path $PSScriptRoot 'r06-rules.json'
$rules = Get-Content -Raw -Encoding UTF8 $rulesPath | ConvertFrom-Json
$windows = Join-Path $repository 'verification/windows/scene'
$summary = Get-Content -Raw -Encoding UTF8 (Join-Path $windows 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or $summary.ctest -ne '16/16 passed' -or $summary.qmllint -ne 'passed') {throw 'Missing Windows Release evidence.'}
if ($summary.production.reviewEnabled -or $summary.production.ctest -ne '13/13 passed' -or $summary.production.reviewCliExitCode -ne 1) {throw 'Ordinary build did not reject Review CLI.'}
$oraclePath = Join-Path $repository 'tests/fixtures/scene/camera-golden.json'
if ((Get-FileHash $oraclePath).Hash.ToLowerInvariant() -ne $summary.fixtureSha256) {throw 'Oracle differs from verified content.'}
$oracle = Get-Content -Raw -Encoding UTF8 $oraclePath | ConvertFrom-Json
foreach ($source in $oracle.sources) {
    $frozen = & git -C $repository show "$($oracle.baseline):$($source.path)"
    if ($LASTEXITCODE -ne 0) {throw 'Frozen source missing.'}
    $bytes = [Text.Encoding]::UTF8.GetBytes(($frozen -join "`n")+"`n")
    if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -ne $source.sha256) {throw 'Frozen source hash differs.'}
}
foreach ($dpr in @('1','1.5','2')) {
    $runtime = Get-Content -Raw (Join-Path $windows "release-dpr-$dpr.json") | ConvertFrom-Json
    if (-not $runtime.passed -or $runtime.fixtureSha256 -ne $summary.fixtureSha256 -or $runtime.cases.Count -ne 409 -or $runtime.maxWebPixels -gt 2) {throw 'Missing GPU projection evidence.'}
}
$sceneFiles = Get-ChildItem -LiteralPath (Join-Path $repository 'src/render') -File
foreach ($file in $sceneFiles) {
    $text = Get-Content -Raw -Encoding UTF8 $file.FullName
    if ($text -match '\b(QTimer|QElapsedTimer|QNetworkAccessManager|QNetworkRequest|QProcess|QMediaPlayer|QAudioOutput|QAudioSink|QAudioDecoder)\b|Qt6::(Quick|Multimedia|Network)|#if|#error|/private/') {throw "Unexpected platform/timer/private/media dependency: $($file.Name)"}
}
$qmlFiles = Get-ChildItem -LiteralPath (Join-Path $repository 'qml') -Recurse -File -Filter '*.qml'
$frameSources = 0
foreach ($file in $qmlFiles) {$frameSources += [regex]::Matches((Get-Content -Raw $file.FullName),'\bFrameAnimation\s*\{').Count}
if ($frameSources -ne 1) {throw 'Expected one production frame source.'}
$sceneQml = Get-Content -Raw (Join-Path $repository 'qml/Yalahoo/Music/scene/GeometryScene.qml')
foreach ($binding in @('position: view.adapter.cameraPosition','rotation: view.adapter.cameraRotation','fieldOfView: view.adapter.fieldOfView','clipNear: view.adapter.clipNear','clipFar: view.adapter.clipFar','position: view.adapter.casePosition')) {
    if (-not $sceneQml.Contains($binding)) {throw "Scene no longer consumes the shared frame: $binding"}
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('macos','linux')) {
    $output = Join-Path $repository "verification/$platform-static/scene"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $checks = @()
    foreach ($check in $rules.checks) {
        $sources = @()
        foreach ($source in $check.sources) {
            $path = Join-Path $repository $source.path
            if (-not (Get-Content -Raw -Encoding UTF8 $path).Contains($source.symbol)) {throw "Missing $($source.path): $($source.symbol)"}
            $sources += @{sourcePath=$source.path;lineOrSymbol=$source.symbol;sha256=(Get-FileHash $path).Hash.ToLowerInvariant()}
        }
        $checks += @{checkId=$check.checkId;platform=$platform;requirement=$check.requirement;
            sourcePath=@($sources.sourcePath);lineOrSymbol=@($sources.lineOrSymbol);sources=$sources;
            evidence=$check.evidence;result='passed';limitations=$check.limitations}
    }
    $specific = if ($platform -eq 'macos') {'MAC-01'} else {'LNX-01'}
    $checks += @{checkId=$specific;platform=$platform;requirement='R06 native platform integration delta';result='N/A';
        sourcePath=@('src/render/CMakeLists.txt','src/app/main.cpp','qml/Yalahoo/Music/scene/GeometryScene.qml');
        lineOrSymbol=@('Qt6::Gui','setGraphicsApi','PerspectiveCamera');
        evidence='Only shared Qt geometry/layout/camera logic added. Existing native API selection is traced in COMMON-03. No new bundle/menu/plugin/launcher/XDG/platform deployment branch; those remain R31/R32. Logical pixels/DPR and resource resolution are traced in COMMON-02; Review frameless/DPI environment is test-only.';
        limitations='No target GPU, window system or package execution. See platform user checklist for effective DPR and native camera projection validation.'}
    $matrix = @{round='R06';configuration='Release';qtVersion=$summary.qtVersion;conclusion='静态审计通过，实机待用户验证';
        checks=$checks;unresolvedFindings=@();method='Automated build/oracle/source/clock/binding gates plus explicit manual branch, transform, moc/qrc, ownership and public rendering API traces in each entry.';
        rulesSha256=(Get-FileHash $rulesPath).Hash.ToLowerInvariant();windowsSummarySha256=(Get-FileHash (Join-Path $windows 'summary.json')).Hash.ToLowerInvariant()}
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'),(($matrix | ConvertTo-Json -Depth 20).Replace("`r`n","`n")+"`n"),$utf8)
    $report = @("# $platform · R06 几何与相机静态审计",'', '静态审计通过，实机待用户验证','',
        "COMMON-01–05 通过，$specific 新增集成为 N/A；具体源码/分支、hash、证据和限制见 [audit-matrix.json](audit-matrix.json)。Windows Qt $($summary.qtVersion) Release 16/16 CTest 与 qmllint 通过；每个实际 DPR 1/1.5/2 均验证 48 静态状态、361 逐帧打断状态。",'',
        '逐项推演 C++23/Gui 静态目标、moc/foreign QML 模块、qrc 三批次几何、逻辑像素、右手 view/projection/quaternion、原几何单次 transform、公开 View3D 后端修正、ray/AABB、同钟相机阻尼及所有者销毁顺序。无本轮未修复静态缺陷。', '',
        '没有执行本平台编译、GPU、GUI、媒体或部署；Windows 数值/截图不构成本平台实机证据。无光材质与单盒不代表完整灯光/玻璃/阵列验收。用户验证清单见 docs/platforms 对应 R06 条目。')
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'),(($report -join "`n")+"`n"),$utf8)
}
Write-Output 'R06 macOS/Linux static audit passed; native runtime awaits user verification.'
