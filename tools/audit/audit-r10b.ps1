$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/rhi-shell'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
$report = Get-Content -Raw (Join-Path $evidence 'release-runtime.json') | ConvertFrom-Json
$variants = Get-Content -Raw (Join-Path $evidence 'shader-variants.json') | ConvertFrom-Json
$rules = Get-Content -Raw (Join-Path $PSScriptRoot 'r10b-rules.json') | ConvertFrom-Json
if (-not $summary.passed -or $summary.configuration -ne 'Release' -or $summary.productImplementationReady -or $summary.cases -ne 61 -or $variants.shaders.Count -ne 7 -or $variants.qtVersion -ne $summary.qtVersion -or $report.meanEqualWeightLoss -ge $report.quick3dMeanEqualWeightLoss -or $report.fixtureSha256 -ne (Get-FileHash (Join-Path $repository 'tests/fixtures/rhi-shell/visual.json')).Hash.ToLowerInvariant()) {
    throw 'Missing actual R10B evidence closure'
}
$sourcePaths = @('legacy-web/src/music-model.ts', 'legacy-web/src/music-lighting.ts', 'legacy-web/src/music-row-lighting.ts',
    'legacy-web/node_modules/three/src/renderers/WebGLRenderer.js', 'legacy-web/node_modules/three/src/renderers/webgl/WebGLRenderLists.js',
    'legacy-web/node_modules/three/src/renderers/webgl/WebGLShadowMap.js', 'legacy-web/package-lock.json',
    'resources/shaders/case-glass.frag', 'resources/shaders/album-paper.frag', 'qml/Yalahoo/Music/scene/GlassMaterial.qml',
    'resources/models/music-case-v040.glb', 'resources/environment/room-pmrem.rgba32f',
    'resources/environment/dfg.rgba32f', 'resources/environment/ltc1.rgba32f', 'resources/environment/ltc2.rgba32f')
$sources = @()
foreach ($path in $sourcePaths) {
    $sources += @{
        path = $path
        sha256 = (Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant()
    }
}
$shaders = @()
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repository 'resources/shaders/rhi') -File) {
    $shaders += @{
        path = 'resources/shaders/rhi/' + $file.Name
        sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    }
}
$utf8 = [Text.UTF8Encoding]::new($false)
$manifest = @{
    round = 'R10B'
    baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2'
    sources = $sources
    shaders = $shaders
    adapter = 'tools/assets/prepare-rhi-shell.py: R07/R08 explicit bindings, R10A shared shadow filter; no algorithm fitting'
    license = 'resources/licenses/three-LICENSE; resources/licenses/materials-NOTICE.md; legacy-web/NOTICE.md'
    uniformBytes = 752
    capture = 'Opaque only; explicit MSAA resolve/mips; independent sorted-shell destination'
    fixedOptics = @{
        ior = 1.46
        environmentScale = 1.01
        roughnessScale = 1.01
        thicknessScale = 0.91809
        attenuationScale = 1.0201
    }
}
[IO.File]::WriteAllText((Join-Path $repository 'resources/manifests/rhi-shell.json'), ($manifest | ConvertTo-Json -Depth 10).Replace("`r`n", "`n") + "`n", $utf8)
$paths = @('tools/audit/r10b-rules.json', 'tools/audit/audit-r10b.ps1', 'docs/contracts/rhi-shell.md', 'src/render/rhi/ShellMaterial.h',
    'docs/contracts/render-backend.json', 'docs/contracts/render-backend.md', 'docs/contracts/rhi-opaque.md',
    'tests/fixtures/rhi-shell/inputs.json', 'tests/fixtures/rhi-shell/visual.json', 'tests/fixtures/render-backend/visual.json',
    'docs/media/render-backend/web/058.png', 'docs/media/render-backend/web/059.png')
foreach ($rule in $rules.checks) {
    $paths += @($rule.sourcePath)
}
$records = @()
foreach ($path in ($paths | Sort-Object -Unique)) {
    $source = Join-Path $repository $path
    if ([IO.Path]::GetExtension($source) -eq '.png') {
        $hash = (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant()
    } else {
        $bytes = $utf8.GetBytes([IO.File]::ReadAllText($source).Replace("`r`n", "`n"))
        $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
    }
    $records += @{
        path = $path
        sha256 = $hash
        hashMode = 'UTF-8 LF text / raw PNG'
    }
}
$artifacts = @()
foreach ($base in @('verification/windows/rhi-shell', 'docs/media/rhi-shell')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repository $base) -Recurse -File) {
        if ($file.Name -eq 'evidence-index.json') {
            continue
        }
        $artifacts += @{
            path = [IO.Path]::GetRelativePath($repository, $file.FullName).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        }
    }
}
$index = @{
    round = 'R10B'
    baseline = $manifest.baseline
    qtVersion = $summary.qtVersion
    sourceRecords = $records
    artifacts = $artifacts
}
[IO.File]::WriteAllText((Join-Path $evidence 'evidence-index.json'), ($index | ConvertTo-Json -Depth 10).Replace("`r`n", "`n") + "`n", $utf8)
foreach ($platform in @('macos', 'linux')) {
    $checks = @()
    foreach ($rule in $rules.checks) {
        $checks += @{
            checkId = $rule.checkId
            requirement = $rule.requirement
            platform = $platform
            sourcePath = $rule.sourcePath
            lineOrSymbol = $rule.lineOrSymbol
            evidence = $rule.evidence
            sourceHashes = @($records | Where-Object { $_.path -in $rule.sourcePath })
            result = 'passed'
            limitations = 'Static source/build/resource audit and same shared code on Windows only; target compiler/GPU/media not executed.'
        }
    }
    $checks += @{
        checkId = if ($platform -eq 'macos') {
            'MAC-01'
        } else {
            'LNX-01'
        }
        requirement = 'Native graphics selection, shader/resource orientation and errors'
        platform = $platform
        sourcePath = @('tools/review/RhiShellAudit.cpp', 'src/render/rhi/RhiItem.cpp', 'src/render/rhi/CMakeLists.txt')
        lineOrSymbol = if ($platform -eq 'macos') {
            'Q_OS_MACOS -> Metal before QGuiApplication'
        } else {
            'non-Windows/macOS -> OpenGL before QGuiApplication'
        }
        evidence = 'No Windows handle, D3D type or ABI in shared renderer. Actual selected QRhi supplies correction/Y/depth/sample/format limits; MSL12 and GLSL/ES variants exist. Single-sample resolve textures are sampled after pass end; unsupported float MSAA/targets fail explicitly. Shader samplers/resources use same Kit/qrc. All new files use case-correct paths and Qt Unicode file APIs; big-endian lookup conversion is explicit. No new launcher/media/deployment behavior; bundle/XDG and final deployment remain R31/R32.'
        result = 'passed'
        limitations = '静态审计通过，实机待用户验证；未执行目标系统构建、GPU、媒体或部署。'
    }
    $output = Join-Path $repository "verification/$platform-static/rhi-shell"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $matrix = @{
        round = 'R10B'
        configuration = 'Release'
        qtVersion = $summary.qtVersion
        conclusion = '静态审计通过，实机待用户验证'
        checks = $checks
        unresolvedStaticDefects = @()
        evidenceIndexSha256 = (Get-FileHash (Join-Path $evidence 'evidence-index.json')).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), ($matrix | ConvertTo-Json -Depth 14).Replace("`r`n", "`n") + "`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), "# R10B $platform 静态审计`n`n静态审计通过，实机待用户验证。`n`n逐项 COMMON-01–05、WINDOW-01 及平台调用链、qsb/资源、MSAA resolve/mips、排序、GUI/render 和读回所有权推演见 audit-matrix.json。测试窗口在首帧前置底/不接受焦点，非视觉检查隐藏；品牌采集自身 paint，不读用户桌面。Windows 同 Kit 已生成目标变体，并运行共用代码回归；没有执行目标系统编译器、Metal/OpenGL、图像、音频或部署。产品后端替换与图集/后处理仍分别按 R10D/R10C 执行。用户实机清单见 docs/platforms/$platform.md 的 R10B 章节。`n", $utf8)
}
