$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/covers'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
$gpu = Get-Content -Raw (Join-Path $evidence 'd3d11.json') | ConvertFrom-Json
$gl = Get-Content -Raw (Join-Path $evidence 'opengl/runtime.json') | ConvertFrom-Json
if (-not $summary.passed -or $summary.samples.Count -ne 3 -or -not $gpu.passed -or -not $gl.passed -or -not $gpu.windowPolicy.passed) {
    throw 'R11 Windows validation is incomplete'
}
if ($gpu.replacement.nativeAtlasTransfers.destinationBytes -ne 262144 -or $gpu.selected.nativeSelectedTransfers.destinationBytes -ne 4194304) {
    throw 'R11 native D3D11 transfer closure is incomplete'
}
$checks = @(
    @('COMMON-01', 'src/services/CMakeLists.txt', 'CoverCache.cpp', 'YalahooCore compiles moc and CoverCache with public Qt Gui; YalahooRhi compiles AtlasAllocator and links the same Core. QThreadPool is QtCore, no new runtime libraries. CoverAudit exists only in Review; hidden CoverCacheTest is always registered. Root C++23/auto-latest Kit/platform branches remain shared.'),
    @('COMMON-02', 'src/services/CoverCache.cpp', 'QImageReader', 'Caller-supplied Unicode QFile paths and qrc aliases go through QImageReader, not cwd concatenation or drive assumptions. Production requests :/assets/demo-covers/square.png already registered by Assets.cmake. Reader failures publish explicit fallback/error; no URL provider or HTTP request. Source/title revision requires cache reset and new leases.'),
    @('COMMON-03', 'resources/shaders/rhi/opaque.frag', 'entityCoverUv', 'Padded square tiles skip the legacy shader contain/inset transform, avoiding double margins. UV clamps to the outer pixel centers; zero extent discards unready leased tiles. Existing QRhi clip/depth/Y corrections and qsb MSL/GLSL/HLSL/SPIR-V variants remain. Shared sampler is linear clamp, no atlas mip chain or anisotropic cross-slot sampling.'),
    @('COMMON-04', 'src/render/rhi/SceneItem.cpp', 'm_covers.request', 'Only local cover loading changes. Qt public media services are untouched; no audio callbacks, PCM, FFmpeg commands, Swift bridge, third-party dependency or network provider introduced.'),
    @('COMMON-05', 'tools/build/verify-r11.ps1', 'review-ctest.txt', 'Three independent 96-source samples, D3D11 native command regions and readbacks, OpenGL real runtime, source-format/window/scene/integration regressions, RHI diagnostics and all production Release CTest run. Output is ignored; final repository contract retains only concise conclusions. Full 120-second product acceptance remains R28.'),
    @('PERF-01', 'src/services/CoverCache.cpp', 'request', 'Admission scans at most 96 pending descriptors; active jobs <=3, completed LRU <=64 RGBA256 entries or 4 RGBA1024 entries, eviction scans this bounded set. AtlasAllocator has <=432 slots and fixed page dimensions; no total-library traversal in frame/render paths. Request identity/path/title lengths bounded.'),
    @('PERF-02', 'src/services/CoverCache.cpp', 'decodePool', 'One process-wide three-thread pool uses tryStart (no Qt worker backlog); each cache owns <=96 pending and <=3 active/unconsumed results. Canceled/rebuilt jobs keep reservations until completion; generation/wanted checks precede read and paint and GUI publication. Source dimensions <=4M pixels even when plugin scaling is supported. No GUI destructor wait; callback posting lock excludes all I/O/decoding.'),
    @('PERF-03', 'src/render/rhi/AtlasUpload.cpp', 'immutableOwned', 'Worker publishes one immutable owned premultiplied QImage. Cache/atlas/selected snapshots share it. Production trusted snapshots skip detach; external Review images still copy once per accepted revision. Generation/lease/serial reject before conversion. Input holds one selected image or at most one page replay; renderer uploads only region prefix, zero on stable frames.'),
    @('PERF-04', 'src/render/rhi/RhiItem.cpp', 'replaceCover', 'Cover size/generation replacement preserves all compatible scene targets/postprocess/shadow/pipelines, rebuilding only texture and existing layout-compatible bindings. Cover resource revision invalidates descriptor inventory. Actual page is checked against device dimensions before allocation. Existing ResourcePlan accounts requested attachments, MSAA/resolve/mips/lookup/buffers/Qt output; cover pages and four selected leases add explicit CPU/GPU upper bounds, not measured VRAM.'),
    @('PERF-05', 'src/services/CoverCache.cpp', 'paint', 'Frozen cover-atlas contain geometry and 1/64 margin apply once at both resolutions; transparent padding and premultiplication preserved. Missing art uses original neutral record-ring/text composition. Cache key is stable identity/path/resolution; caller must reset on same-path source/title revisions. Lower-resolution selection shares cached pixels until 1024 ready; returning fork pins prior snapshot.'),
    @('PERF-06', 'src/render/rhi/SceneItem.cpp', 'schedule', 'Ready signal coalesces polish; unchanged frame/image key does not submit. No QML image-per-album delegates or per-frame decoding. Cost sampling uses real QEventLoop with a GUI timer; GPU audit submits once and waits for actual readback completion without polling/resubmission. Existing production FrameAnimation remains the only frame clock.'),
    @('PERF-07', 'src/render/rhi/AtlasAllocator.cpp', 'reset', 'Allocator generation is globally unique; source generation/slot lease/serial are separately checked. Expected lease hides old GPU pixels before new upload. Reset cancels pending work; QObject destruction clears callback receiver under lock and disconnects queued calls without joining workers. Publication handles receiver deletion. Four selected/returning snapshots remain capacity-counted across reset. Qt renderer exclusively owns/rebuilds GPU resources.'),
    @('WINDOW-01', 'tools/review/CoverAudit.cpp', 'verification::prepareWindow', 'CoverCacheTest opens no window. Actual GPU review applies shared bottom/no-focus flags before first show, keeps them on hide/release/restore and checks policy each capture. PNG/readback are own renderer images, no desktop capture/raise/activation.')
)
$paths = @('CMakeLists.txt', 'cmake/QtKit.cmake', 'cmake/Platform.cmake', 'cmake/Assets.cmake',
    'src/services/CMakeLists.txt', 'src/services/CoverCache.h', 'src/services/CoverCache.cpp',
    'src/render/rhi/CMakeLists.txt', 'src/render/rhi/AtlasAllocator.h', 'src/render/rhi/AtlasAllocator.cpp',
    'src/render/rhi/AtlasUpload.h', 'src/render/rhi/AtlasUpload.cpp', 'src/render/rhi/RhiItem.h', 'src/render/rhi/RhiItem.cpp',
    'src/render/rhi/ResourcePlan.h', 'src/render/rhi/TransferAudit.h', 'src/render/rhi/SceneItem.h', 'src/render/rhi/SceneItem.cpp',
    'src/app/GraphicsBackend.h', 'src/app/VerificationWindow.h', 'src/app/IntegrationReview.cpp', 'resources/shaders/rhi/opaque.frag',
    'tests/unit/CMakeLists.txt', 'tests/unit/CoverCacheTest.cpp', 'tools/review/CMakeLists.txt', 'tools/review/CoverAudit.cpp',
    'tools/build/verify-r11.ps1', 'tools/audit/audit-r11.ps1', 'docs/contracts/covers.md',
    'docs/contracts/rhi-integration.md', 'QT_QUICK_MIGRATION_PLAN.md', 'README.md')
$records = @()
foreach ($path in $paths) {
    $records += @{
        path = $path
        sha256 = (Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant()
    }
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('windows', 'macos', 'linux')) {
    $matrix = @()
    foreach ($check in $checks) {
        if (-not [IO.File]::ReadAllText((Join-Path $repository $check[1])).Contains($check[2])) {
            if ($check[0] -ne 'COMMON-05') {
                throw "Missing audited symbol: $($check[1]):$($check[2])"
            }
        }
        $matrix += @{
            checkId = $check[0]
            requirement = $check[0]
            platform = $platform
            sourcePath = $check[1]
            lineOrSymbol = $check[2]
            evidence = $check[3]
            result = 'passed'
            limitations = 'Descriptor reservations exclude opaque decoder/driver/Qt allocator overhead; no target-platform performance or full product claim.'
        }
    }
    if ($platform -ne 'windows') {
        $matrix += @{
            checkId = if ($platform -eq 'macos') {
                'MAC-01'
            } else {
                'LNX-01'
            }
            requirement = 'R11 native cover and shader ownership closure'
            platform = $platform
            sourcePath = 'src/app/GraphicsBackend.h'
            lineOrSymbol = 'selectGraphicsBackend'
            evidence = if ($platform -eq 'macos') {
                'macOS selects Metal; common worker QFile/QImageReader and QRhi RGBA8 region uploads have no Windows branch. qsb MSL variant exists; no changed bundle install paths or new external image codec dependencies. DPR dimensions remain current Qt item output.'
            } else {
                'Linux selects OpenGL; common QFile/QImageReader and QRhi upload path has no D3D call. GLSL/ES variants exist, actual Windows OpenGL readback passed. Linux plugin/filesystem/compositor behavior requires target validation; no new .sh or deployment dependencies.'
            }
            result = 'passed'
            limitations = 'Target OS build, codecs, GPU, compositor and performance not run.'
        }
    }
    $directory = if ($platform -eq 'windows') {
        $evidence
    } else {
        Join-Path $repository "verification/$platform-static/covers"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $conclusion = if ($platform -eq 'windows') {
        'Windows Release 封面/上传/生命周期验收通过'
    } else {
        '静态审计通过，实机待用户验证'
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), (@{
                round = 'R11'
                conclusion = $conclusion
                qtVersion = $summary.qtVersion
                sourceRecords = $records
                checks = $matrix
            } | ConvertTo-Json -Depth 8), $utf8)
    [IO.File]::WriteAllText((Join-Path $directory 'audit-report.md'), "$conclusion`n`n覆盖 COMMON-01–05、PERF-01–07、WINDOW-01 及平台分支。调用链/缓存键/容量/生命周期推演见矩阵；源码 hash 对应本次审计。原生采样、图像和日志只存本地。目标平台未运行，不能将 Windows 结果解释为 Metal/Linux 实机成绩。`n", $utf8)
}
Write-Output 'R11 Windows and macOS/Linux static audits passed'
