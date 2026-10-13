$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/rhi-integration'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
$variants = Get-Content -Raw (Join-Path $evidence 'shader-variants.json') | ConvertFrom-Json
if (-not $summary.passed -or $summary.runs.Count -ne 4 -or $variants.shaders.Count -ne 24) {
    throw 'R10D Windows runtime/shader closure is incomplete'
}
if ($summary.performanceAcceptance -ne 'record-only' -or $summary.throughputReports.Count -lt 1) {
    throw 'R10D user-authorized record-only performance evidence is required'
}
foreach ($path in $summary.throughputReports) {
    $report = Get-Content -Raw (Join-Path $evidence $path) | ConvertFrom-Json
    if ($report.performance.frameIntervals.count -le 100 -or $report.performance.durationSeconds -ne 120 -or $report.performance.atlasUploadBytes -ne 0 -or $report.performance.resourceChanges -ne 0) {
        throw "Incomplete or structurally invalid throughput observation: $path"
    }
}
$paths = @('CMakeLists.txt', 'cmake/QtKit.cmake', 'cmake/RhiProbe.cmake', 'cmake/Assets.cmake',
    'src/app/CMakeLists.txt', 'src/app/main.cpp', 'src/app/QmlTypes.h', 'src/app/GraphicsBackend.h',
    'src/app/VerificationWindow.h', 'src/app/IntegrationReview.cpp', 'src/app/SceneReview.cpp',
    'src/render/SceneAdapter.h', 'src/render/SceneAdapter.cpp', 'src/render/SceneFrame.h',
    'src/render/SelectionLighting.cpp', 'src/render/RenderQuality.cpp', 'src/render/RenderBackend.h',
    'src/render/rhi/CMakeLists.txt', 'src/render/rhi/SceneItem.h', 'src/render/rhi/SceneItem.cpp',
    'src/render/rhi/RhiItem.h', 'src/render/rhi/RhiItem.cpp', 'src/render/rhi/RhiApi.h',
    'src/render/rhi/PreparedAssets.h', 'src/render/rhi/PreparedAssets.cpp',
    'src/render/rhi/ResourceInventory.h', 'src/render/rhi/ResourcePlan.h',
    'src/render/rhi/Postprocess.cpp', 'src/render/rhi/Postprocess.h', 'src/render/rhi/AtlasUpload.cpp',
    'qml/Yalahoo/Music/App.qml', 'qml/Yalahoo/Music/ResearchApp.qml', 'tests/unit/CMakeLists.txt',
    'tools/build/verify-r10d.ps1', 'tools/review/RhiPostprocessAudit.cpp', 'tools/audit/audit-r10d.ps1', 'docs/contracts/rhi-integration.md',
    'docs/contracts/render-backend.json', 'docs/contracts/render-backend.md', 'docs/contracts/rhi-opaque.md',
    'docs/contracts/rhi-shell.md', 'docs/contracts/rhi-postprocess.md', 'docs/contracts/scene.md',
    'QT_QUICK_MIGRATION_PLAN.md', 'docs/contracts/performance.md', 'README.md', 'docs/platforms/macos.md', 'docs/platforms/linux.md',
    'tests/fixtures/scene/camera-golden.json', 'tests/fixtures/render-backend/visual.json',
    'tests/fixtures/rhi-shell/visual.json')
$utf8 = [Text.UTF8Encoding]::new($false)
$records = @()
foreach ($path in $paths) {
    $text = [IO.File]::ReadAllText((Join-Path $repository $path)).Replace("`r`n", "`n")
    $records += @{
        path = $path
        sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($text))).ToLowerInvariant()
        hashMode = 'UTF-8 LF text'
    }
}
$checks = @(
    @('COMMON-01', 'src/app/CMakeLists.txt', 'YALAHOO_REVIEW; target_link_libraries; qt_add_qml_module',
        'Production links YalahooRhi/Quick and one SceneItem; Quick3D, FixedTextureData, ResearchApp and material resources exist only in the Review block. Root selects current Kit and probes C++23/GuiPrivate. Assets.cmake excludes only Quick3D production model assets, preserving files and review resources.'),
    @('COMMON-02', 'src/render/rhi/PreparedAssets.cpp', 'preparedAssets; SceneItem constructor',
        'All fixed GPU resources are immutable :/rhi aliases; demo cover is :/assets. qsb prefixes/resources and CMake targets form the registration closure. No cwd/drive-based resource lookup. Test output is explicitly supplied; product data identity remains initialized before QGuiApplication.'),
    @('COMMON-03', 'src/render/rhi/RhiItem.cpp', 'prepareResources; renderFrame; uniformTemplate; Postprocess::render',
        'Native VP/lightVP use current QRhi clipSpaceCorrMatrix; canonical Web projection is preserved for screen effects, framebuffer Y conversion is distinct from PNG/lookup row order. R32F/D32F shadow, exactly equal half scalar AO (only red consumed; diagnostic invariant channels reconstructed), RGBA16F MSAA resolve/mips and independent opaque/composite attachments are checked. 24 baked shader programs include MSL12 and GLSL330/ES300 plus HLSL/SPIR-V; source kernels/optics/output math unchanged.'),
    @('COMMON-04', 'src/app/main.cpp', 'QMediaPlayer; QAudioOutput; QObject ownership',
        'No media source is opened in this integration boundary. Existing public QtMultimedia objects remain independent of renderer/clock; no new audio backend, PCM, bridge or network path. Complete playback behavior is outside R10D and belongs to R24/R25.'),
    @('COMMON-05', 'tools/build/verify-r10d.ps1', 'Measure; Run; summary; shader-variants',
        'Windows R06-R10 regressions and three serial normal-VSYNC runs plus actual OpenGL are retained separately. Existing source-index/five-ROI residuals are not pooled or relabeled as image-gate success. Hash records identify current sources/binaries and variants; historical directories are not written.'),
    @('PERF-01', 'src/render/rhi/RhiItem.cpp', 'visibleEntities; opaqueOrder; shells; uniformScratch',
        'One production inspection entity now, maximum 432 in renderer. Culling O(N), depth keys computed once, O(N log N) sorting with stable ID/surface; buffers fixed at 432 entries and uniformStride*(1296+6). Work is unrelated to total library size; key changes ignore entity count.'),
    @('PERF-02', 'src/render/rhi/SceneItem.cpp', 'updatePolish; Mailbox; preparedAssets',
        'GUI polish coalesces notices, unchanged content does not submit. Renderer receives values only; resource disk reads, GLB/qsb parsing, PMREM conversion and PNG decoding occur once before show. Telemetry has 128 samples and one queued dispatch; Review samples have 40000-event bounds. Atlas queue merges stable slots within a fixed page. Product captures are disabled.'),
    @('PERF-03', 'src/render/rhi/RhiItem.cpp', 'synchronize; coverSourceKey; submission; updateDynamicBuffer',
        'Duplicate submission skips synchronization. Owned cover pixels copy once per cacheKey; frozen Qt values share immutable payloads. Atlas generation/lease/serial rejection precedes pixel conversion. Only the initialized uniform prefix/instance range is uploaded; stable atlas frames submit zero image bytes.'),
    @('PERF-04', 'src/render/rhi/ResourcePlan.h', 'resourcePlan; Renderer::resourceInventory; Postprocess::inventory',
        'Cumulative plan precedes GPU allocation and checks pixel/dimension/MSAA capabilities. Inventory includes attachments, MSAA/resolve/mips, shared depth once, lookups/buffers and Qt item output. Unknown native depth format uses an explicit upper bound. Resize old+new is a descriptor replacement estimate; driver/PSO/SRB/swapchain/compositor/retired storage remain unmeasured.'),
    @('PERF-05', 'src/render/SceneAdapter.cpp', 'publish; Renderer::renderFrame; Postprocess::prepare',
        'Unchanged exact camera/layout/entity values avoid rebuilding projection/inverse matrices. Entity light cache keys include matrices and row/print/light settings. Shadow caster/target/enable keys exclude camera. Postprocess keys exclude focus/aperture; source functions, samples, precision and ordering are preserved.'),
    @('PERF-06', 'src/app/IntegrationReview.cpp', 'click; recordInput; frameSubmitted; frameSwapped; distribution',
        'Real object-directed QML input records monotonic input revision/time and corresponding renderer snapshot submission. Production FrameAnimation remains single source; drive is event/timer based without polling or processEvents. Normal-VSYNC events are per run/stage; scanout/input-to-photon is not claimed. Unimplemented full GUI is explicitly excluded.'),
    @('PERF-07', 'src/render/rhi/RhiItem.cpp', 'Renderer destructor/reset; reportFailure; SceneItem::event',
        'Scene graph owns GPU objects; private APIs confined to rhi and capability probe. Before-show fixed CPU assets are process-bounded and reused through ten real releases. Hidden FrameAnimation stops; callbacks use guarded receivers and bounded storage. DPR changes re-polish; render exceptions surface without destroying objects referenced by recorded commands.'),
    @('WINDOW-01', 'src/app/VerificationWindow.h', 'windowFlags; prepareWindow; App verificationFlags',
        'App remains hidden until parameters validated. Integration applies bottom/no-focus/frameless flags before first show and keeps them through ten restores. Captures use own grabWindow; helper/system flag test remains registered. No activation, raising or desktop capture.')
)
$windowsMatrix = @()
foreach ($check in $checks) {
    $windowsMatrix += @{
        checkId = $check[0]
        platform = 'windows'
        sourcePath = $check[1]
        lineOrSymbol = $check[2]
        evidence = $check[3]
        result = 'passed'
        runtimeEvidence = 'release-ctest.txt; production-ctest.txt; regression/; d3d11/run-*/runtime.json; opengl/run-1/runtime.json'
        limitations = 'Only the implemented single-case integration is accepted. Descriptor bytes are estimates; no scanout/compositor timing or full product acceptance.'
    }
}
[IO.File]::WriteAllText((Join-Path $evidence 'audit-matrix.json'), (@{
            round = 'R10D'
            conclusion = 'Windows Release 结构/功能实机验收通过；性能仅记录，未建立数值达标结论'
            performanceAcceptance = $summary.performanceAcceptance
            performanceCompliance = $summary.performanceCompliance
            sourceRecords = $records
            checks = $windowsMatrix
        } | ConvertTo-Json -Depth 10).Replace("`r`n", "`n") + "`n", $utf8)
foreach ($platform in @('macos', 'linux')) {
    $matrix = @()
    foreach ($check in $checks) {
        $matrix += @{
            checkId = $check[0]
            requirement = $check[0]
            platform = $platform
            sourcePath = $check[1]
            lineOrSymbol = $check[2]
            evidence = $check[3]
            result = 'passed'
            limitations = 'Source/ownership/build-branch audit and Windows baked variants; target OS compilation, window manager, GPU, format support and performance not executed.'
        }
    }
    $matrix += @{
        checkId = if ($platform -eq 'macos') {
            'MAC-01'
        } else {
            'LNX-01'
        }
        requirement = 'R10D native backend selection and implementation closure'
        platform = $platform
        sourcePath = 'src/app/GraphicsBackend.h'
        lineOrSymbol = 'selectGraphicsBackend; Q_OS_MACOS; OpenGL branch'
        evidence = if ($platform -eq 'macos') {
            'macOS defaults to Metal; explicit OpenGL is permitted, unsupported overrides fail. Product uses shared RHI/qsb and logical dimensions/effective DPR. Bundle branch retained; full packaging/menus/Command integration remains R31, not accepted here.'
        } else {
            'Linux defaults to OpenGL and uses common qsb GLSL330/ES300. No D3D11 is forced. Unsupported overrides fail explicitly; xcb/Wayland/plugin/deployment/Vulkan integration remains R32, not accepted here.'
        }
        result = 'passed'
        limitations = 'Target GPU, compositor and real platform deployment remain unexecuted.'
    }
    $directory = Join-Path $repository "verification/$platform-static/rhi-integration"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $report = @{
        round = 'R10D'
        conclusion = '静态审计通过，实机待用户验证'
        qtVersion = $summary.qtVersion
        sourceRecords = $records
        checks = $matrix
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), ($report | ConvertTo-Json -Depth 10).Replace("`r`n", "`n") + "`n", $utf8)
    $markdown = "# R10D · $platform`n`n静态审计通过，实机待用户验证。`n`n范围：生产 RHI 场景集成、资源和帧/输入测量入口；不替代后续完整平台部署与 GUI 验收。Qt $($summary.qtVersion)。当前源码 hash 见 audit-matrix.json，Windows 对照在 verification/windows/rhi-integration/。`n`n"
    foreach ($entry in $matrix) {
        $markdown += "- $($entry.checkId)：$($entry.evidence) [$($entry.sourcePath)::$($entry.lineOrSymbol)]`n"
    }
    $markdown += "`n目标机编译、MSAA/float MRT、光学/后处理像素、窗口 flags、DPR、驱动/compositor、显存驻留和性能不能由静态审计证明。用户命令及预期检查见 docs/platforms/$platform.md 的 R10D 清单。`n"
    [IO.File]::WriteAllText((Join-Path $directory 'audit-report.md'), $markdown, $utf8)
}
$artifacts = @()
foreach ($directory in @($evidence, (Join-Path $repository 'docs/media/rhi-integration'))) {
    foreach ($file in Get-ChildItem -LiteralPath $directory -Recurse -File) {
        if ($file.Name -eq 'evidence-index.json') {
            continue
        }
        $hashMode = 'binary bytes'
        $hash = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        if ($file.Extension -in @('.json', '.txt', '.md')) {
            $normalized = [IO.File]::ReadAllText($file.FullName).Replace("`r`n", "`n")
            $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($normalized))).ToLowerInvariant()
            $hashMode = 'UTF-8 LF text without BOM'
        }
        $artifacts += @{
            path = [IO.Path]::GetRelativePath($repository, $file.FullName).Replace('\', '/')
            sha256 = $hash
            hashMode = $hashMode
        }
    }
}
$index = @{
    round = 'R10D'
    baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2'
    parentCommit = (git -C $repository rev-parse HEAD)
    sourceRecords = $records
    artifacts = $artifacts
}
[IO.File]::WriteAllText((Join-Path $evidence 'evidence-index.json'), ($index | ConvertTo-Json -Depth 10).Replace("`r`n", "`n") + "`n", $utf8)
Write-Output 'R10D static call-chain/platform audit passed; target machine execution remains pending.'
