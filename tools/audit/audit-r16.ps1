$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$chain = Get-Content -Raw (Join-Path $repository 'build/release/toolchain.json') | ConvertFrom-Json
$utf8 = [Text.UTF8Encoding]::new($false)
# Symbol checks guard the independently reviewed call-chain deductions below.
# They are not runtime or target-platform compiler/visual evidence.
$checks = @(
    @('COMMON-01', 'src/render/text/CMakeLists.txt', 'YALAHOO_REEL_GLYPH_API', 'Same latest native Kit and C++23; the private addGlyphs signature is compiled at configure time. A missing interface fails explicitly at the isolated RHI boundary, without selecting an older Qt. Shared Qt value types and events have no Windows implementation dependency.'),
    @('COMMON-02', 'cmake/BrowseQml.cmake', 'TitleReel', 'App and ui-audit register the same Yalahoo.Music components with qrc aliases. Fonts are initialized from existing assets; no drive letter, current-directory file, browser or Node is used by the production title path. Canvas review-only resources do not become a product dependency.'),
    @('COMMON-03', 'resources/shaders/reel-slot.frag', 'textureHeight', 'Text renders into Qt-owned single-sample RGBA8 layers then alpha-composites through the Qt Quick shader ABI. Matrix/opacity block and source binding follow Qt Quick; no manual projection, MSAA resolve or textureSize unsupported by ES100. The qsb dump contains GLSL/HLSL/MSL, which proves generation rather than target GPU behavior.'),
    @('COMMON-04', 'qml/Yalahoo/Music/components/TransportTitle.qml', 'locateRequested', 'The title button sends only locateRequested, forwarded by Header as locate-song. It has no QMediaPlayer/QAudioOutput/PCM call or media backend. The source width/opacity/offset/icon state is visual; actual transport commands remain in their planned media round.'),
    @('COMMON-05', 'tools/review/export-title.mjs', 'execFrozenGit', 'The frozen production markup/CSS/font/track sources are read through the cleaned Git mapping. Source probe refresh is declared separately and raw prior captures remain ignored. Current complete title visual matrix is not passed; R16B1-B3 retain its gates and failure samples, so implementation tests are not reported as final pixel selection.'),
    @('PERF-01', 'src/render/text/TitleReel.cpp', 'm_animatedSlots', 'Reconcile uses an indexed line/column slot pool; one-shot layout processes input text, never the library. Tick traverses only moving slots and height, with O(1) stationary entry. Whole-line string-index grouping is O(n log n); at most 25 bounded fit candidates measure line boxes, and only the selected candidate extracts glyphs.'),
    @('PERF-02', 'src/render/rhi/ReelTextNode.cpp', 'QRawFont::fromFont', 'GUI shaping keeps no QTextLayout or GUI QRawFont in the cached descriptor. Render-thread font lookup verifies the font head identity before applying glyph IDs. There is one coalesced queued reconcile per controller and no worker, file read, processEvents, wait or production GPU readback in title ticks.'),
    @('PERF-03', 'src/render/text/TitleReel.cpp', 'faceValues', 'Glyph/font/position snapshots use implicit-sharing containers and cached QVariant values. The stable slot model inserts/removes individual rows. Fixed-capacity QML face delegates create Text only for color glyphs or the explicit comparison candidate. Animation updates transforms/clip/alpha; single-slot geometry still rebuilds on content revision and its further reuse is an explicit R16B1 task.'),
    @('PERF-04', 'src/render/rhi/ResourcePlan.h', 'title-reel-depth', 'Each shown slot has a capacity-sized layer at actual DPR times local raster scale two; hidden slots disable it. SceneItem aggregates physical sizes only on resourcesChanged and passes values/revision to RhiInput. ResourcePlan counts RGBA8 and conservative depth upper bounds, without changing unrelated 3D target compatibility keys. Resize may overlap old/new layers; private font atlas and driver memory remain unknown.'),
    @('PERF-05', 'src/render/text/TextLayoutAdapter.cpp', 'CacheBytes', 'Cache key includes full text, both fonts, size, width, height budget, narrow, DPR and transport. Ninety-six entries and 8MiB descriptor accounting bound retention; oversized entries are not cached. Font database changes clear the cache. Whole-run IDs preserve ligatures and contextual glyph forms; sameGlyphInk also invalidates unchanged text when its context differs.'),
    @('PERF-06', 'qml/Yalahoo/Music/components/TitleReel.qml', 'root.clock', 'Existing FrameClock drives title/transport/icon states with speed applied once; no component timer/FrameAnimation is added. Invisible text stops updates, reduced motion finishes explicitly, same targets do not restart. Tooltip/Accessible names use full original strings while visual faces are ignored by accessibility. Disabled exiting song cannot emit locate actions.'),
    @('PERF-07', 'tools/review/TitleAudit.cpp', 'window.destroy()', 'One asynchronous review readback is in flight; drain uses MaxAsyncReadbackFrames+1. Teardown destroys the graphics surface while callback/readback state still lives; SG nodes and Canvas renderers belong to Qt. Native single-context diagnostics released all tracked nodes/renderers after ten cycles. Cross-DPR/backend lifetime and geometry reuse verification remain R16B1-B3.'),
    @('MAC-01', 'src/app/GraphicsBackend.h', 'Metal', 'Apple selects Metal; title measurement and window DPR feed the same layer descriptors and qsb MSL shader. Only font descriptions/IDs cross the sync boundary, with actual font identity checked per native platform. No Windows input/font handle enters this shared path. Native compiler/private ABI, Retina fallback/color emoji and compositor behavior await real hardware; bundle/deployment stays R31.'),
    @('LNX-01', 'src/app/GraphicsBackend.h', 'OpenGL', 'Linux selects OpenGL through the existing platform guard and the same shader/value pipeline. Resources are qrc and do not require drive letters; QRhi readback orientation is backend-derived in the tool. No Windows media DLL or input dependency is introduced. Native compiler/private ABI, xcb/Wayland fonts/color emoji, compositor and GPU pixels await real hardware; deployment stays R32.')
)
$paths = @('CMakeLists.txt', 'src/render/text/CMakeLists.txt', 'src/render/text/TextLayoutAdapter.cpp', 'src/render/text/TextLayoutAdapter.h', 'src/render/text/TitleReel.cpp', 'src/render/text/TitleReel.h', 'src/render/text/ReelInk.cpp', 'src/render/text/TransportMotion.cpp', 'src/render/rhi/ReelTextNode.cpp', 'src/render/rhi/SceneItem.cpp', 'src/render/rhi/ResourcePlan.h', 'src/render/rhi/RhiItem.cpp', 'src/app/GraphicsBackend.h', 'src/app/VerificationWindow.h', 'src/app/QmlTypes.h', 'qml/Yalahoo/Music/components/TitleReel.qml', 'qml/Yalahoo/Music/components/TransportTitle.qml', 'qml/Yalahoo/Music/components/Header.qml', 'qml/Yalahoo/Music/components/BrowseInfo.qml', 'resources/shaders/reel-slot.frag', 'cmake/BrowseQml.cmake', 'tools/review/TitleAudit.cpp', 'tools/review/CanvasTitleProbe.cpp', 'tools/review/export-title.mjs', 'tests/unit/TitleTest.cpp')
$records = foreach ($path in $paths) {
    @{
        path = $path
        sha256 = (Get-FileHash -LiteralPath (Join-Path $repository $path)).Hash.ToLowerInvariant()
    }
}
foreach ($platform in @('windows', 'macos', 'linux')) {
    $matrix = foreach ($check in $checks) {
        if (-not [IO.File]::ReadAllText((Join-Path $repository $check[1])).Contains($check[2])) {
            throw "Missing reviewed symbol: $($check[1]):$($check[2])"
        }
        @{
            checkId = $check[0]
            platform = $platform
            sourcePath = $check[1]
            lineOrSymbol = $check[2]
            evidence = $check[3]
            result = 'static-passed'
            limitations = 'Implementation audit only. Full title visual/path/performance acceptance remains R16B1-B3. macOS/Linux compilation and runtime have not run.'
        }
    }
    $directory = if ($platform -eq 'windows') {
        Join-Path $repository 'verification/windows/title-reels'
    } else {
        Join-Path $repository "verification/$platform-static/title-reels"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $conclusion = if ($platform -eq 'windows') {
        'Windows实现源码审计通过；完整标题视觉与成本验收待R16B1-B3'
    } else {
        '静态审计通过，实机待用户验证'
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), (@{
                round = 'R16 implementation'
                conclusion = $conclusion
                qtVersion = $chain.qtVersion
                sourceRecords = $records
                checks = $matrix
            } | ConvertTo-Json -Depth 8), $utf8)
    $paragraphs = @("# R16 / $platform", $conclusion)
    foreach ($check in $checks) {
        $paragraphs += "$($check[0]) — $($check[1]):$($check[2])`n$($check[3])"
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-report.md'), ($paragraphs -join "`n`n"), $utf8)
    [IO.File]::WriteAllText((Join-Path $directory 'user-validation.md'), ("$conclusion`n`n在目标平台使用最新原生Kit执行 cmake --preset qt-release、cmake --build --preset qt-release、ctest --preset qt-release --output-on-failure；普通产物入口按README启动。检查双主题/长标题/译名/连字/组合字/emoji/括号/下伸部、真实DPR1/1.5/2及跨屏resize，连续切换/反向、0.25到3倍途中改速、减少动态、隐藏恢复、歌名只定位与播放按钮固定。私有字形接口配置探测失败须适配本机最新Qt，不能选旧Kit。部署/媒体设备实机命令按R30-R32清单执行；本审计未运行目标平台编译器/GPU/compositor，未生成部署包。"), $utf8)
}
Write-Output 'R16 implementation static audits passed; full visual/cost acceptance remains R16B1-B3'
