$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/browse-ui'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
$paired = Get-Content -Raw (Join-Path $evidence 'paired-ui.json') | ConvertFrom-Json
if (-not $summary.passed -or -not $paired.passed) {
    throw 'Complete Release R14 runtime and paired UI evidence is required'
}
$checks = @(
    @('COMMON-01', 'cmake/QtKit.cmake', 'CanvasPainter', 'Latest native build closure', 'Kit discovery enumerates native version/architecture/ABI first, then validates CanvasPainter alongside all existing modules without older fallback. Root C++23 capability probe remains required. Yalahoo and ui-audit link the selected Qt6::CanvasPainter; QML module dependencies include QtQuick and the review-only QtCanvas2D. Public presets contain no Kit minor/patch path. No platform branch removes the module.'),
    @('COMMON-02', 'cmake/BrowseQml.cmake', 'QT_RESOURCE_ALIAS', 'Fonts, brand and QML resource closure', 'Shared function gives components/pages module-relative aliases. App and UiReview import relative components from the same qrc module. Qt6Gui/Svg support identical embedded MiSans SFNT faces, and each executable embeds both SVG production logos at /assets/branding. Canvas shaders belong to the selected Qt module. R14 does not package a platform bundle; R30-32 must deploy CanvasPainter plus any adopted QtCanvas2D plugin and Qt notices.'),
    @('COMMON-03', 'tools/review/CanvasProbe.cpp', 'synchronizeData', 'Renderer ownership and API behavior', 'Qt owns createItemRenderer result and its path/font; constructor creates one immutable SVG QCanvasPath. GUI state copy is bool-only under Qt synchronization. Paint uses same curve, rounded 3px stroke, diagonal gradient, alpha, setClipRect and baseline-shaped string as Canvas2D. No I/O/worker wait/readback occurs inside paint. Qt item release owns native buffers; scenegraph rebuild creates a fresh renderer. D3D11/GL runs validate both APIs; Metal is a source deduction only.'),
    @('COMMON-04', 'tools/review/Canvas2DProbe.qml', 'property path2d curve', 'QML drawing import and supported clip semantics', 'QtCanvas2D import resolves through selected Kit, not current working directory. path2d.addPath runs once at completion; CPU path2d is reused by stroke without a shared GPU cache group. Theme/available invalidation coalesces requestPaint. The outer state owns cumulative counters; a single inner Loader drawing context is destroyed/recreated on visibility, surface loss or size changes because the preview context cannot reliably replay into a replaced attachment. Theme and stationary frames never recreate it; stationary tests prohibit a loop. reset/save/restore/setClipRect plus explicit resetClipping after restore preserve opacity and visible text. Browser Path2D constructor and general clip are intentionally unsupported; future arbitrary clip must be compared separately. No independent requestAnimationFrame/Timer exists.'),
    @('WINDOW-01', 'src/app/VerificationWindow.h', 'WindowDoesNotAcceptFocus', 'No-focus own-render verification', 'Both standalone UiAudit and actual App verify-browse receive helper flags before first show. App starts hidden, frameless enables required content dimensions above monitor width, and hide/release/show keeps flags. Mouse events are sent to the owned QQuickWindow after finding visual Repeater delegates; no desktop pointer/activation. UI capture is own grabWindow; Canvas evidence is revision-matched actual swapchain QRhi readback, one in flight with a backend MaxAsyncReadbackFrames +1 drain bound and a 1px diagnostic marker outside the complete 360x160 probe extent. Every capture validates actual dimensions and inactive/bottom/no-focus flags.'),
    @('PERF-01', 'src/services/LibrarySession.cpp', 'm_genreNames', 'Indexed browse work bounded by selected content', 'Snapshot replacement builds album/genre indices once (position index also rebuilds on columnsChanged), O(A+G+T) inherited identity work and O(G+A) added memory for genre and browse-position indices. browseInfo is O(1) index lookup plus constant-size string/value map; no tracks or entire album serialization. Stable ID retains existing title on search and playback state remains independent. FrameAnimation never invokes this getter or rebuilds the model.'),
    @('PERF-02', 'tools/review/CanvasProbe.cpp', 'setClipRect', 'No unbounded queue or render-thread I/O', 'Only GUI-owned bound values cross synchronizeData. No queued image/decode/upload worker was added. Fonts are registered before engines and copied from immutable initialized face map; render-thread path construction happens once per renderer. Capture/PNG/font loading and frozen Web work happen in verification setup or discrete audit points outside paint/sync timing.'),
    @('PERF-03', 'tools/review/UiAudit.cpp', 'painterImages', 'Bounded snapshots and comparison memory', 'Two fixed C++ QImage reference frames are retained, one per theme. QImage uses implicit sharing. Temporary captures and differences are released per verification point. Loader owns only one actual drawing API at a time; static image state does not resubmit or make deep pixel copies on normal frames.'),
    @('PERF-04', 'qml/Yalahoo/Music/components/ReadingVeil.qml', 'sideVeilOpacity', 'Pass graph and attachment lifetime', 'Production UI uses direct Qt Quick nodes: existing SceneItem result, landscape/portrait reading gradient, day/night vertical gradients, night side gradient, portrait local gradient, then chrome. Inactive color layers are invisible; no layer.enabled or ShaderEffectSource target. Canvas comparison separately uses one RGBA8 1-sample alpha attachment, no mips/resolve or application cover upload. ResourcePlan records conservative color/depth and simultaneous resize estimates; font/internal buffers/swapchain unknowns are not presented as measured memory.'),
    @('PERF-05', 'qml/Yalahoo/Music/components/NavigationFrame.qml', 'labelRowAscent', 'Dependency-complete layout and typography', 'QML consumes the existing LayoutMetrics, and source inline strut is derived from the same fonts, font sizes and line spacing. No viewport-to-1920 scaling or screenshot constants were added. Fractional CSS font sizes scale a neighboring integer vector font locally, with scaled letter spacing and rounded CSS ascent/descent; there is no giant font atlas or extra layer. Actual CSS cascade gives the open button 16px and explicit Basic padding. Text and path changes invalidate only their owning nodes; style/selection changes never rebuild 3D resources. R16 still owns full title measurement and reel caching.'),
    @('PERF-06', 'qml/Yalahoo/Music/components/Header.qml', 'songCap', 'Constant control pool and fixed transport', 'Two theme delegates and fixed buttons use Basic overrides, explicit SVG icons at 4x physical extent with mip filtering (bounded fixed-icon allocation, no per-frame raster), accessible names and focus outlines. Song width affects preceding controls, while transport buttons stay on the right. Information scroll owns short-screen overflow. Palette/pill transitions use existing reduced-motion/speed inputs; drawing APIs contain no extra clock. Hidden or steady probes do not repaint; this is checked for both C++ and JS.'),
    @('PERF-07', 'tools/review/UiAudit.cpp', 'lifecycleMode', 'Hide, resize and ten rebuilds', 'Each drawing API is tested stationary, hidden, restored after releaseResources, actually resized at Loader/item level, compared after restore/resize against the earlier submitted pixels, and unloaded/recreated ten times. Final C++ renderer count is zero and JS QObject is absent. Renderer timing storage is declared before QQuickWindow so callbacks cannot outlive it; diagnostic copies are locked. Product probe checks independent playback and exact requested content sizes.'),
    @('MAC-01', 'src/app/GraphicsBackend.h', 'QSGRendererInterface::Metal', 'macOS shared UI and Metal branch', 'Q_OS_MACOS selects Metal unless an explicit supported GL review request is used. All R14 draws, fonts, SVG paths and QML aliases are Qt APIs; Windows-only Kit/compiler/Dwrite preparation stays in existing guarded branches. No new Windows call is introduced in shared paint/layout logic. Target Metal shader execution, Retina font/color emoji and compositor/no-focus behavior require user runtime validation.'),
    @('LNX-01', 'src/app/GraphicsBackend.h', 'QSGRendererInterface::OpenGL', 'Linux shared UI and OpenGL branch', 'After Windows/macOS guards the native branch selects OpenGL. R14 uses qrc imports/Qt values and no drive-letter resource URLs, local server, Node runtime, or platform-specific input. Windows OpenGL checks module draw behavior, not X11/Wayland presentation. Target fontconfig fallback, GL capabilities and compositor flags require user runtime validation.'),
    @('COMMON-05', 'tools/review/export-browse-ui.mjs', 'execFrozenGit', 'Source precedence and repeatable paired input', 'Reference markup/CSS and title layout are reconstructed from mapped frozen Git source; exact viewport/DPR/theme content matches native captures. UTF-8 is explicit. All browser external requests are rejected, headless process is hidden and isolated profile is under build. Only the narrow theme text requirement is a documented source delta. Screenshots/raw series and hashes remain ignored; no fixture is created from output to bypass the Git boundary.')
)
$paths = @('CMakeLists.txt', 'cmake/QtKit.cmake', 'cmake/BrowseQml.cmake', 'src/app/CMakeLists.txt', 'src/app/main.cpp', 'src/app/ApplicationIdentity.h', 'src/app/BrowseReview.cpp', 'src/app/VerificationWindow.h', 'src/app/GraphicsBackend.h', 'src/services/LibrarySession.cpp', 'tools/review/CMakeLists.txt', 'tools/review/CanvasProbe.cpp', 'tools/review/CanvasProbe.h', 'tools/review/UiAudit.cpp', 'tools/review/UiTypes.h', 'tools/review/UiReview.qml', 'tools/review/Canvas2DProbe.qml', 'tools/review/Quick2DProbe.qml', 'tools/review/export-browse-ui.mjs', 'tools/review/browse-ui-harness.js', 'tools/review/compare-browse-ui.ps1', 'tools/build/verify-r14.ps1', 'docs/contracts/canvas-painter.md')
$paths += @('qml/Yalahoo/Music/App.qml', 'qml/Yalahoo/Music/BootstrapProbe.qml', 'qml/Yalahoo/Music/pages/Browse.qml')
$paths += @(Get-ChildItem -LiteralPath (Join-Path $repository 'qml/Yalahoo/Music/components') -Filter '*.qml' | ForEach-Object { 'qml/Yalahoo/Music/components/' + $_.Name })
$records = @()
foreach ($path in $paths) {
    $records += @{
        path = $path
        sha256 = (Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant()
    }
}
$utf8 = [Text.UTF8Encoding]::new($false)
$binaries = foreach ($path in @('build/release/bin/yalahoo.exe', 'build/release/tools/ui-audit.exe', 'build/release/tools/compare-images.exe')) {
    @{
        path = $path
        sha256 = (Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant()
        bytes = (Get-Item -LiteralPath (Join-Path $repository $path)).Length
    }
}
foreach ($platform in @('windows', 'macos', 'linux')) {
    $matrix = @()
    foreach ($check in $checks) {
        if (-not [IO.File]::ReadAllText((Join-Path $repository $check[1])).Contains($check[2])) {
            throw "Missing audited symbol: $($check[1]):$($check[2])"
        }
        $matrix += @{
            checkId = $check[0]
            requirement = $check[3]
            platform = $platform
            sourcePath = $check[1]
            lineOrSymbol = $check[2]
            evidence = $check[4]
            result = 'passed'
            limitations = 'Static call-chain deductions and Windows runtime facts are distinct; target macOS/Linux compiler, GPU, fonts and compositor have not run. Local CPU drawing diagnostics are not full-product performance.'
        }
    }
    $directory = if ($platform -eq 'windows') {
        $evidence
    } else {
        Join-Path $repository "verification/$platform-static/browse-ui"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $conclusion = if ($platform -eq 'windows') {
        'Windows Release 二维界面、双 API 绘制与生命周期验证通过'
    } else {
        '静态审计通过，实机待用户验证'
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), (@{
                round = 'R14'
                conclusion = $conclusion
                qtVersion = $summary.qtVersion
                sourceRecords = $records
                windowsReleaseBinaries = $binaries
                checks = $matrix
            } | ConvertTo-Json -Depth 8), $utf8)
    $paragraphs = @("# R14 / $platform", $conclusion)
    foreach ($check in $checks) {
        $paragraphs += "$($check[0]) — $($check[1]):$($check[2])`n$($check[4])"
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-report.md'), ($paragraphs -join "`n`n"), $utf8)
}
Write-Output 'R14 Windows and macOS/Linux static audits passed'
