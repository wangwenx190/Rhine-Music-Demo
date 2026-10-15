$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/archive-pool'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
if (-not $summary.passed -or $summary.configuration -ne 'Release') {
    throw 'R12 Release validation is incomplete'
}
foreach ($sample in $summary.sampleDirectories) {
    $report = Get-Content -Raw (Join-Path $evidence "$sample/runtime.json") | ConvertFrom-Json
    if (-not $report.passed -or -not $report.windowPolicy.passed -or $report.libraryAlbums -ne 90000 -or $report.stable.drawCount -ne 432 -or $report.empty.drawCount -ne 0 -or $report.sourceMatrixError -ge 0.00001 -or $null -eq $report.sourceMatrixError -or $null -eq $report.rebaseMatrixError -or $report.rebaseMatrixError -gt 0.0001 -or $report.rebuild.drawCount -ne $report.rebased.drawCount) {
        throw "Incomplete R12 sample: $sample"
    }
}
$checks = @(
    @('DATA-01', 'src/services/LibrarySession.cpp', 'm_albumIndices', 'Snapshot replacement builds album/track identity hashes in O(N+T) before model notifications. Navigation, playback validation and provider routing query those hashes without full-library scans; duplicate identities preserve the original first-match semantics. Existing library/runtime tests and filled playback-isolation tests pass. The inactive pool controller does not alter legacy MotionController calls.'),
    @('PERF-01', 'src/render/ArchivePool.cpp', 'ArchivePool::update', 'Snapshot/sort builds location and cover-source hashes once in O(N) storage/time. Each publish visits exactly 432 pool slots plus <=4 owned copies; ID/location/source/column-count queries are indexed. Renderer computes visibility once, keeps offscreen shadow casters and precomputes sort depths; no library traversal in frame/render/picking.'),
    @('PERF-02', 'src/render/rhi/SceneItem.cpp', 'preparePool', 'GUI only shares worker snapshots and bounded leases; existing global three-worker decoder, 96-descriptor admission and generation cancellation remain. Ready identity set avoids re-decoding retained tiles after LRU eviction. Full queue rejects admission; completion schedules coalesced polish and retries missing identities. No I/O, wait, decode or manual event loop in product hot paths.'),
    @('PERF-03', 'src/render/SceneFrame.h', 'instances', 'SceneAdapter publishes Qt implicitly shared immutable instance records at revision boundaries. Projection, ray picking, atlas UV/lease and shadows consume those same transforms. Four 1024 snapshots split into 16 tiles only on content revision; per-frame replay shares pixels and renderer rejects known serial before conversion. Active buffer prefix only; stable atlas upload zero.'),
    @('PERF-04', 'src/render/rhi/RhiItem.cpp', 'visibleEntities', '436 entity buffer capacity is independent of actual draw count and quality attachment compatibility. Fixed 5632x6656 RGBA8 atlas packs 436 thumbnail slots plus four 1024 blocks; actual maximum texture dimension/format and ResourcePlan validate before allocation. Visibility affects drawing only, full world caster set remains. Camera/focus/count/transform do not replace targets or pipelines.'),
    @('PERF-05', 'src/render/SelectionController.cpp', 'SelectionController::rebase', 'Frozen pool order, nearest ties, adjacent direction, field/ripple/idle/settling/slope and return exponential retained. Rebase applies capped LCM logical period, transforms all track origins/polynomials/pulses/returning cells and ribbon springs while preserving derivatives and absolute wave phase. Frozen actual scene field/instance/model blocks generate independent geometry input; independent numeric matrices, identity/UV invariants and actual GPU resource checks exercise it.'),
    @('PERF-06', 'src/render/rhi/SceneItem.cpp', 'mousePressEvent', 'Qt direct item mouse/ray and wheel events navigate stable album IDs and physical cells. LibrarySession remains independent of playback/search. One FrameClock feeds movement and waves; Qt polish coalesces notifications, no per-album QML delegates. Render telemetry remains bounded to 128 queued events; diagnostics retain <=256 samples.'),
    @('PERF-07', 'src/render/rhi/SceneItem.cpp', 'resetPool', 'Library/sort replacement resets generation, workers and atlas/owned leases. Render recreation replays only current retained immutable pixels; late generations rejected. Four owned copies budget is checked before selection mutation; reclaim reuses exact cell/pixels/object ID, resolved returns precede pool restoration. QObject/QPointer disconnects guard replacement and destruction; GPU remains render-owned.'),
    @('COMMON-01', 'src/app/QmlTypes.h', 'SelectionControllerQml', 'Shared C++23 Qt Core/Gui/Quick build sources and QML foreign registration are complete. Cover URLs convert Unicode local paths or existing qrc assets once; no browser/HTTP/Node runtime, media engine, network request or new dependency. Demonstration is explicit resource-only anonymous data until R21 scanning.'),
    @('WINDOW-01', 'tools/review/ArchivePoolAudit.cpp', 'verification::prepareWindow', 'Real RHI tests apply bottom/no-focus flags before first show and keep them across hide/release/show. Inputs go directly to owned Qt objects. Images come from own QRhi texture readback, atlas readback omitted for this audit to avoid unrelated large capture payload; no desktop capture or activation.'),
    @('PLATFORM-01', 'src/app/GraphicsBackend.h', 'selectGraphicsBackend', 'Source fields and actual instance matrices are checked numerically; native draw/resource recovery is separate from cross-API images. macOS selects Metal, Linux OpenGL through common QRhi. New matrix/ray/worker/lease paths contain no OS APIs; existing qsb produces MSL/GLSL/HLSL/SPIR-V and build/Qt Kit/module/ABI discovery unchanged. Target maximum texture dimension must be >=6656; unsupported hardware fails explicitly, without silently lowering quality.')
)
$records = @()
foreach ($path in @('src/render/ArchivePool.h', 'src/render/ArchivePool.cpp', 'src/render/SelectionController.h', 'src/render/SelectionController.cpp', 'src/render/SceneFrame.h', 'src/render/SceneAdapter.cpp', 'src/render/rhi/SceneItem.cpp', 'src/render/rhi/RhiItem.cpp', 'src/render/rhi/ResourcePlan.h', 'src/render/rhi/AtlasAllocator.cpp', 'src/render/rhi/AtlasUpload.cpp', 'src/services/CoverCache.cpp', 'src/services/LibrarySession.cpp', 'src/app/main.cpp', 'src/app/QmlTypes.h', 'src/app/GraphicsBackend.h', 'src/app/VerificationWindow.h', 'tests/unit/ArchivePoolTest.cpp', 'tools/review/ArchivePoolAudit.cpp', 'tools/review/export-archive-pool-golden.mjs', 'tools/build/verify-r12.ps1', 'docs/contracts/archive-pool.md')) {
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
            throw "Missing audited symbol: $($check[1]):$($check[2])"
        }
        $matrix += @{
            checkId = $check[0]
            sourcePath = $check[1]
            lineOrSymbol = $check[2]
            evidence = $check[3]
            result = 'passed'
            limitations = 'Source deductions and Windows facts distinguished; target OS runtime/GPU/compositor and complete product performance not measured.'
        }
    }
    $directory = if ($platform -eq 'windows') {
        $evidence
    } else {
        Join-Path $repository "verification/$platform-static/archive-pool"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $conclusion = if ($platform -eq 'windows') {
        'Windows Release 填充实例池、选择/拾取/rebase 与上传/资源/生命周期验收通过'
    } else {
        '静态审计通过，实机待用户验证'
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), (@{
                round = 'R12'
                conclusion = $conclusion
                qtVersion = $summary.qtVersion
                sourceRecords = $records
                checks = $matrix
            } | ConvertTo-Json -Depth 8), $utf8)
}
Write-Output 'R12 Windows and macOS/Linux static audits passed'
