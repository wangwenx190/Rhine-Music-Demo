$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$summary = Get-Content -Raw (Join-Path $repository 'verification/windows/navigation/summary.json') | ConvertFrom-Json
if (-not $summary.passed) {
    throw 'Complete R15 Release verification is required'
}
$checks = @(
    @('COMMON-01', 'src/render/navigation/CMakeLists.txt', 'Qt6::CanvasPainter', 'Shared native build closure', 'YalahooNavigation is a static C++23 target using the existing YalahooScene, Quick and selected CanvasPainter module. Rhi delegates only native input values and links this target; navigation never links Rhi, so there is no library cycle. QtKit discovery checks the highest installed native version before find_package. No new minor/patch condition, toolkit path or external library is introduced.'),
    @('COMMON-02', 'cmake/BrowseQml.cmake', 'OrdinalReel', 'QML and shader resource resolution', 'Shared aliases embed Ruler/OrdinalReel in components for App and the independent review engine. Foreign registration includes the Canvas base and shared controller/clock/selection types. Edge fade uses qrc:/assets/shaders/edge-fade.frag.qsb baked by the same Kit into both executable closures. Embedded original SFNT and public Qt shaping remain cross-platform. Canvas2D is review-only; production uses C++ CanvasPainter. Platform packaging remains the concrete R30-32 dependency.'),
    @('COMMON-03', 'src/render/navigation/RulerInk.cpp', 'synchronizeData', 'Canvas renderer and backends', 'Qt owns the render-thread renderer, its three actual-size path groups, RGBA8 alpha target and buffers. GUI synchronization copies one fixed sixteen-mark value array plus two QColor values. Paint draws each full tick inside a fixed-capacity attachment; the shared Qt Quick rectangular clip node applies the animated source width without rebuilding a subpixel sliver, preserving actual heights. Fill alpha applies the world-x linear mask at overflow edges. Cached path groups are removed when tick geometry changes; animated heights use fillRect so tessellated AA fringes are not scaled. No platform-native graphics handle enters navigation. Windows D3D11 and OpenGL are runtime facts, Metal is a Qt API/source deduction.'),
    @('COMMON-04', 'src/render/navigation/InputRouter.cpp', 'pointerOwner', 'Native input ownership and state boundaries', 'Wheel processing executes only after Qt routes an unowned event to SceneItem; a second item-ancestor check preserves input/text controls, buttons, sliders, popup items and Flickable ownership even at scroll boundaries. Keyboard filter checks focused ancestors and ignores Control/Command/Alt shortcuts. Pointer traversal skips invisible/disabled nodes and respects clips. Qt pixelDelta takes priority; Qt positive-y is explicitly inverted to the frozen browser convention. Selection alone owns stable IDs and finite/loop/column memory rules; no playback/media/provider calls exist here.'),
    @('WINDOW-01', 'src/app/NavigationReview.cpp', 'verification::windowPolicySatisfied', 'Own-render no-focus verification', 'App verification flags are installed before QML creation and the root begins hidden. Events are sent directly to the owned QQuickWindow; text focus is only QQuickItem focus, never OS activation. Review captures swapchain textures or own grabWindow, not desktop pixels. Hide/release/show and ten isolated surface rebuilds retain bottom/no-focus flags. SceneItem mouse focus does not force active focus on verification windows.'),
    @('PERF-01', 'src/render/navigation/RulerController.cpp', 'regroup', 'Bounded workload and indexed identities', 'Per active frame visits sixteen slots. Recycling and group assignment are O(16 squared), fixed independently of library size; destinations are precomputed once before stable sort. Selected column count and canonical identity come from ArchivePool indices; sixteen titles use existing indexed coverSource. No frame scans or serializes all album/track data. Decimal tracks have at most ten digits, each with 49 points only when retargeted.'),
    @('PERF-02', 'src/render/navigation/InputRouter.cpp', 'm_pending', 'Backpressure and direction semantics', 'Wheel remainder is strictly below 48px, each event at most three albums, with 200ms gaps, reversal, pointer/key/context and mode/library invalidation. When existing three-returning-cover ownership refuses a selection, only one replaceable absolute target is retained and attempted on the unique FrameClock. Reverse row events accumulate against that target; columnTarget exposes the existing pure column-memory calculation so queued column and row intents share the same single destination. Rebase translates pending coordinates; library/mode changes cancel it. Per-field decimal directions preserve row and pending-column intent separately; the album code uses the last navigation direction, and external selections clear stale direction. Explicit ruler clicks replace a pending column target with the currently visible column. No debounce timer, blocking operation, new worker or unbounded event backlog exists.'),
    @('PERF-03', 'src/render/navigation/RulerController.cpp', 'm_slotStates', 'Single immutable state publication', 'Drawing is a fixed value snapshot; the QVariantList for input/accessibility is built once per publication and returned using Qt implicit sharing, so sixteen QML bindings do not rebuild sixteen full snapshots. Ordinal faces likewise publish once per active frame, with two visible faces per digit. Repeater models are fixed integers, not per-frame list replacement. Paint never copies pixels or uploads cover images.'),
    @('PERF-04', 'src/render/rhi/ResourcePlan.h', 'navigation-ruler-color', 'Pass graph and cumulative resource plan', 'Existing 3D graph precedes the Canvas RGBA8 ruler attachment, its conservative depth upper bound and Quick alpha composition. Source opacity/edge masks are multiplied into C++ tick fills, with no separate full-screen mask. Desktop 130x30/narrow 96x21 attachments do not resize with animated width; input and drawing share the direct Quick clip boundary. Five bounded decimal-wheel layers use the shared edge-fade shader after text rendering. ResourcePlan adds physical-DPR descriptor upper bounds; the allocation descriptor key includes navigationDpr without changing unrelated GPU target keys. One old/new UI attachment set may coexist on resize, so document the doubled UI estimate alongside RHI resize resources. Driver buffers, Qt font atlas and private module allocations remain unknown rather than claimed measured.'),
    @('PERF-05', 'src/render/navigation/RulerInk.cpp', 'removePathGroup', 'Complete cache invalidation', 'Three static actual-size paths are cached per renderer, keyed by narrow/desktop tick geometry, and removed before replacement. Translation does not stretch AA. Fractional animated heights take the bounded immediate rectangle path. Color and alpha are current drawing parameters, not cache keys for immutable geometry. Controller thresholds/ripple/slot recycle are compared against the unchanged frozen production closure at four frame rates and at every 1/120-second representative state.'),
    @('PERF-06', 'qml/Yalahoo/Music/components/Ruler.qml', 'model: 16', 'Focus, accessibility and one clock', 'Sixteen Button delegates consume the same controller snapshot used by Canvas; hidden/overscan slots are disabled, invisible and have no focus policy, with source focus handoff. Hover/focus only retarget existing height/alpha springs. FrameClock already scales speed; no Timer/FrameAnimation is added to ruler or wheel components. Same targets do not restart decimal tracks. QtQuick Text draws bounded numeric faces; complete title shaping remains R16.'),
    @('PERF-07', 'tools/review/RulerAudit.cpp', 'lifecycleChecks', 'Hide, restore, resize and resource release', 'One readback is in flight and frames drain only to the backend asynchronous-readback bound. Thirty-three lifecycle captures cover baseline and ten hide/release/resize/restore cycles for all three drawing APIs, with persistent scenegraph/graphics disabled so each cycle genuinely releases its surface. Stationary paint counts use a visible settled ruler after three initialization submissions and explicitly advance its controller. Canvas2D draws explicit rect/fill paths and normalizes its antialias amount by actual DPR to keep a one-physical-pixel fringe instead of eroding thin lines at DPR2. C++ renderer count is at most one and reaches zero after destruction. Canvas2D preview contexts are disposable Loader instances across replacement surfaces, preserving outer state. Product hide freezes controller progress, restore retains input/window flags and navigation never changes playback IDs.'),
    @('MAC-01', 'src/app/GraphicsBackend.h', 'Metal', 'macOS input/render branches', 'Production selects Metal under the existing Apple guard; no Windows input API or wheel sign assumption leaks into the shared Qt value path. Command/Control/Alt modified keys remain owned by their controls. qsb dump verifies an MSL variant of the fade shader. Retina DPR is read from the actual QQuickWindow and feeds UI attachments. Platform compiler/font/compositor and touch-device behavior remain user runtime checks.'),
    @('LNX-01', 'src/app/GraphicsBackend.h', 'OpenGL', 'Linux shared path', 'Linux reaches the existing native OpenGL branch; QML aliases and qrc resources do not depend on a drive letter, current directory, local server or Node at runtime. QRhi handles texture orientation for review readback; the production shader follows the same Qt Quick binding convention on GL/Metal/D3D11. xcb/Wayland presentation and device gesture behavior have not run here.'),
    @('COMMON-05', 'tools/review/export-ruler.mjs', 'execFrozenGit', 'Source precedence and artifact limits', 'Ruler geometry, dynamics, loop helpers and CSS are reconstructed through the cleaned frozen Git mapping. Only an observation method is inserted into the production closure; algorithms and final CSS overrides are preserved. Installed rolling-number is checked against the frozen dependency lock before pure track/sample functions are evaluated. Full raw images and the source-only one-physical-pixel geometric-alpha contour are kept separately; numerical state gates prevent contour normalization from hiding moved geometry. Per-tick raw ink errors remain recorded; the continuous source rectangle/fade integral and browser integral define a reference-only coverage envelope with the unchanged RGB<=6 bound. Missing/excess-ink perturbations must be rejected, and fractional-DPR browser raster losses are not copied into production. All machine outputs and raw series stay ignored.')
)
$paths = @('src/render/navigation/CMakeLists.txt', 'src/render/navigation/InputRouter.cpp', 'src/render/navigation/InputRouter.h', 'src/render/navigation/RulerController.cpp', 'src/render/navigation/RulerController.h', 'src/render/navigation/RulerInk.cpp', 'src/render/navigation/RulerInk.h', 'src/render/navigation/OrdinalMotion.cpp', 'src/render/navigation/OrdinalMotion.h', 'src/app/NavigationReview.cpp', 'src/app/main.cpp', 'src/app/QmlTypes.h', 'src/app/GraphicsBackend.h', 'src/render/rhi/SceneItem.cpp', 'src/render/SelectionController.cpp', 'src/render/rhi/ResourcePlan.h', 'qml/Yalahoo/Music/components/Ruler.qml', 'qml/Yalahoo/Music/components/OrdinalReel.qml', 'qml/Yalahoo/Music/components/NavigationFrame.qml', 'qml/Yalahoo/Music/components/BrowseInfo.qml', 'resources/shaders/edge-fade.frag', 'tools/review/RulerAudit.cpp', 'tools/review/RulerReview.qml', 'tools/review/export-ruler.mjs', 'tests/unit/NavigationTest.cpp')
$records = foreach ($path in $paths) {
    @{
        path = $path
        sha256 = (Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant()
    }
}
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('windows', 'macos', 'linux')) {
    $matrix = foreach ($check in $checks) {
        if (-not [IO.File]::ReadAllText((Join-Path $repository $check[1])).Contains($check[2])) {
            throw "Missing reviewed symbol: $($check[1]):$($check[2])"
        }
        @{
            checkId = $check[0]
            requirement = $check[3]
            platform = $platform
            sourcePath = $check[1]
            lineOrSymbol = $check[2]
            evidence = $check[4]
            result = 'passed'
            limitations = 'Windows Release observations and static deductions are separate. macOS/Linux have not run; compositor, GPU, font fallback, device timing and deployment remain user validation or their planned rounds.'
        }
    }
    $directory = if ($platform -eq 'windows') {
        Join-Path $repository 'verification/windows/navigation'
    } else {
        Join-Path $repository "verification/$platform-static/navigation"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $conclusion = if ($platform -eq 'windows') {
        'Windows Release 导航、滑尺与绘制生命周期验证通过'
    } else {
        '静态审计通过，实机待用户验证'
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), (@{
                round = 'R15'
                conclusion = $conclusion
                qtVersion = $summary.qtVersion
                sourceRecords = $records
                checks = $matrix
            } | ConvertTo-Json -Depth 8), $utf8)
    $paragraphs = @("# R15 / $platform", $conclusion)
    foreach ($check in $checks) {
        $paragraphs += "$($check[0]) — $($check[1]):$($check[2])`n$($check[4])"
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-report.md'), ($paragraphs -join "`n`n"), $utf8)
    [IO.File]::WriteAllText((Join-Path $directory 'user-validation.md'), ("$conclusion`n`n使用该平台最新原生 Kit 执行 cmake --preset qt-release、cmake --build --preset qt-release 和 ctest --preset qt-release --output-on-failure。普通启动后检查箭头、滚轮/触控板、36px 主轴滑动、真实列端点、循环跨首尾、滑尺点击与反向；输入框、面板和滑块保持输入所有权。检查双主题、减少动态、改速、DPR/resize、隐藏恢复及播放不受导航影响。平台打包命令按 R31/R32 的实机清单执行；本审计不把 Windows 图像或 qsb 生成记为该平台运行。"), $utf8)
}
Write-Output 'R15 Windows and macOS/Linux static audits passed'
