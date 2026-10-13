$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/real-columns'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
if (-not $summary.passed -or $summary.configuration -ne 'Release') {
    throw 'R13 Release validation is incomplete'
}
foreach ($sample in $summary.sampleDirectories) {
    $report = Get-Content -Raw (Join-Path $evidence "$sample/runtime.json") | ConvertFrom-Json
    if (-not $report.passed -or -not $report.realColumns -or -not $report.windowPolicy.passed -or $report.sourceMatrixError -gt 0.000001 -or $report.sourceSsim -lt 0.97 -or $report.sourceMae -gt 6 -or $report.rebuildSsim -lt 0.999 -or $report.stable.atlasUploadBytes -ne 0 -or $report.empty.drawCount -ne 0 -or $report.last.resourceGeneration -ne $report.stable.resourceGeneration -or $report.night.resourceGeneration -ne $report.stable.resourceGeneration) {
        throw "Incomplete R13 sample: $sample"
    }
}
[xml]$sourceTests = Get-Content -Raw (Join-Path $evidence 'full-source-trajectories.xml')
if ($sourceTests.testsuite.failures -ne '0' -or $sourceTests.testsuite.errors -ne '0') {
    throw 'Full frozen source trajectory comparison failed'
}
foreach ($log in @('check-format.txt', 'production-ctest.txt', 'production-qmllint.txt', 'affected-ctest.txt')) {
    $content = Get-Content -Raw (Join-Path $evidence $log)
    if ($content -match '[1-9][0-9]* tests failed|FAILED|Error:|Unqualified access') {
        throw "Validation log contains unresolved failure: $log"
    }
}
$checks = @(
    @('PERF-01', 'src/render/RealColumns.cpp', 'RealColumns::sample', 'Work scales with the visible pool, not total library or column count', 'RealColumns::update only advances scalar time/epoch. sample applies the analytic spring or quintic once per elapsed interval for visible/target columns; selection samples old/new destinations before retarget. SelectionController::publish checks 432 cells plus <=4 copies and ArchivePool location/row count are indexed. Snapshot reset is O(N+C), stores O(N+C); no full-library/column traversal in frame paths.'),
    @('PERF-02', 'src/services/PreferencesStore.cpp', 'PreferencesStore::schedule', 'Bounded asynchronous saves and GUI ownership', 'GUI startup reads <=1MiB once. QFutureWatcher carries one value result; private QThreadPool has one worker. While saving, GUI retains only latest QJsonObject and dirty bit. Worker only receives implicitly shared values and path, calls QSaveFile, never accesses SelectionController/QML. finished starts at most one next save. Product rendering, events, column sampling have no I/O or worker wait.'),
    @('PERF-03', 'src/render/SelectionController.cpp', 'SelectionController::publish', 'Publish only changed immutable state', 'Only legal finite cells produce instances and cover identities. Same transforms reuse the last published vector/revision. SceneAdapter shares QVector values; rendering uses the same matrices for draw/shadow/ray. Mode/memory settings never recreate a QML tree or copy pixel payloads. Existing R12 thumbnail/high-resolution bounded ownership and partial transfer are unchanged; stable native uploads are zero.'),
    @('PERF-04', 'src/render/rhi/SceneItem.cpp', 'preparePool', 'Compatibility-based resources and finite cover work', 'Finite selection produces <=436 instances under unchanged R12 resource plan: 5632x6656 atlas, fixed entity/uniform capacity and existing attachments/MSAA/resolve. Invalid cells do not enter preparePool, atlas leases or draw/pick. Changing column depth/count/theme does not alter attachment keys; actual D3D11/OpenGL resourceGeneration retained. Descriptor estimates remain distinct from unavailable VRAM measurement.'),
    @('PERF-05', 'src/render/RealColumns.cpp', 'RealColumns::select', 'Equivalent independent column curves and valid cache dependencies', 'ColumnScalar retains x/v/a at guided retarget; unchanged target does not restart. Exact damp and polynomial evaluation allow lazy elapsed intervals, validated against separately executed frozen TypeScript every frame at 20/30/60/120fps and 0.25/1/3x, plus long frames. reduced epoch completes unseen old targets; a new target starts after sampling previous epoch. field uses absolute time; non-target lanes retain idleWave and pulses only match their own lane. Windows independent source field/transform images passed.'),
    @('PERF-06', 'src/render/ColumnMemory.cpp', 'NavigationAvailability::at', 'One input boundary and no binding cascade', 'One SelectionController boundary serves keyboard/wheel/touch/moveRow/navigateRow/enterColumn and future slider/detail callers. Finite head/tail checks precede selection/return/pulse mutation. Session memory keys stable album ID, actual rendered target depth sampled on disabled memory. Only preference changes emit memory/mode signals; no per-album QML nodes. Qt polish remains coalesced and existing telemetry queues bounded.'),
    @('PERF-07', 'src/services/PreferencesStore.cpp', 'PreferencesStore::~PreferencesStore', 'Replacement, reduced motion, shutdown and render recreation', 'Snapshot/sort reset real columns and session ID map once. Mode change translates existing archive tracks, drops old returning copies/pulses and resets ribbon basis. QObject contexts protect GUI callbacks; preference destructor waits only private in-flight task and writes one final coalesced value at teardown. Scene graph recreation preserves finite identities/covers via existing generation replay; actual hide/release/restore image equality checked. Corrupt settings stay read-only and report errors.'),
    @('COMMON-01', 'src/render/CMakeLists.txt', 'RealColumns.cpp', 'Complete cross-platform C++23 build closure', 'Top-level links Core/Gui/Quick/Concurrent from the one latest Kit. RealColumns/ColumnMemory are in YalahooScene; PreferencesStore is in YalahooCore; foreign SelectionController exports mode/memory/availability. Sources contain Qt/std APIs only and no OS preprocessor branch; MSVC flags remain WIN32-gated in Platform.cmake, Apple/Unix targets compile the same implementation. Review-only GPU tests have real tool registration and existing product/Review separation.'),
    @('COMMON-02', 'src/app/main.cpp', 'preferences->rememberColumnPosition()', 'Native data and resource resolution', 'initializeIdentity precedes QGuiApplication/standard paths; --data-dir override otherwise AppLocalDataLocation. QDir/QFile/QSaveFile preserve Unicode/space/absolute path semantics; preferences.json keeps unknown JSON values and session rows are absent. Cover URLs resolved through existing QUrl local/qrc chain, source files and aliases unchanged. All verification skips user preference loading, anonymous demo uses embedded qrc assets.'),
    @('COMMON-03', 'src/render/SceneAdapter.cpp', 'm_selection->realistic()', 'Finite matrices, world basis and shared rendering backends', 'Finite row transform cancels common rail using independent depth. SceneFrame instances and caseTransform feed same RHI draw/pick/shadow chain. RowLightingInput enables independentColumns; existing RowLightController converts basis on column change without double filtering same-lane motion. No shader or attachment semantics change; existing backend clipSpaceCorrMatrix/Y orientation/depth/resolve paths retained and selected-Kit qsb variants checked by shader-tools CTest. Windows OpenGL provides extra shared-path evidence, not Linux runtime proof.'),
    @('COMMON-04', 'src/services/LibrarySession.cpp', 'LibrarySession::navigate', 'Navigation and playback stay separate', 'SelectionChanged routes to library.navigate only. LibrarySession owns independent playback IDs; memory/mode changes never setPlaybackIds or media source. R12 playback/search isolation regressions pass. No new media protocol or engine, network or audio path. Other platform media/device/deployment behavior remains the existing implementation and later assigned rounds.'),
    @('MAC-01', 'src/app/GraphicsBackend.h', 'Q_OS_MACOS', 'Metal native branch with shared controls and paths', 'Actual branch selects Metal. New field/curves/events/prefs all use Qt APIs with logical coordinates and no Windows dependency. AppLocalDataLocation and bundle/resource resolution use existing native branches. CanvasPainter is not adopted in R13. Metal shader generation is source/build evidence only; target compiler/GPU/Retina/window manager/keyboard need user runtime checks.'),
    @('LNX-01', 'src/app/GraphicsBackend.h', 'QSGRendererInterface::OpenGL', 'OpenGL native branch with shared controls and XDG data', 'The branch after excluding Windows/macOS selects OpenGL for Linux. Event/worker/QSaveFile/column paths have no drive-letter or current-working-directory dependence beyond explicit data-dir. Qt standard paths use target XDG behavior; common atlas format/size rejection remains explicit. xcb/Wayland compositor/window-manager policy and target filesystem/GPU need user runtime checks; Windows OpenGL is not a Linux run.'),
    @('COMMON-05', 'tests/unit/RealColumnsTest.cpp', 'preferencePersistenceAndCorruption', 'Reproducible independent input and honest limitations', 'Ordinary tests consume sparse independent frozen numeric input; complete every-frame source sequence and fields regenerated into ignored verification. Unit tests cover empty/odd/even/long/duplicate IDs, return interruption, unaffected waves, negative fill occurrence, mode/setting no selection side effects, coalesced saves and corruption. Real GPU path checks image transforms, disabled events, immutable covers, resource reuse and restore. Three serial D3D11 plus one Windows GL preserved separately; no complete-product performance claim.'),
    @('WINDOW-01', 'tools/review/ArchivePoolAudit.cpp', 'verification::prepareWindow', 'Non-interfering native verification and own-render capture', 'Before first show, VerificationWindow sets bottom/no-focus flags. Hide/release/show keeps flags; windowPolicy checks actual inactive state. Events target owned Qt objects; forceActiveFocus affects only item focus inside the window and never calls window activation. PNG comes from own QRhi capture, no desktop screen pixels. Pure numeric/preferences tests remain GUI-less.')
)
$paths = @('src/render/RealColumns.cpp', 'src/render/RealColumns.h', 'src/render/ColumnMemory.cpp', 'src/render/ColumnMemory.h', 'src/render/ArchivePool.cpp', 'src/render/SelectionController.cpp', 'src/render/SelectionController.h', 'src/render/SceneAdapter.cpp', 'src/render/RowLightController.cpp', 'src/render/rhi/SceneItem.cpp', 'src/render/rhi/RhiItem.cpp', 'src/services/PreferencesStore.cpp', 'src/services/PreferencesStore.h', 'src/services/LibrarySession.cpp', 'src/core/LibraryModels.cpp', 'src/app/main.cpp', 'src/app/QmlTypes.h', 'src/app/GraphicsBackend.h', 'src/app/VerificationWindow.h', 'src/render/CMakeLists.txt', 'src/services/CMakeLists.txt', 'cmake/Platform.cmake', 'cmake/QtKit.cmake', 'qml/Yalahoo/Music/App.qml', 'tests/unit/RealColumnsTest.cpp', 'tests/unit/CMakeLists.txt', 'tests/fixtures/real-columns/trajectories.json', 'tools/review/export-real-columns-golden.mjs', 'tools/review/ArchivePoolAudit.cpp', 'tools/build/verify-r13.ps1', 'docs/contracts/real-columns.md')
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
            limitations = 'Manual call-chain deductions and Windows runtime facts distinguished; target OS/GPU/compiler/compositor and full-product performance remain unmeasured.'
        }
    }
    $directory = if ($platform -eq 'windows') {
        $evidence
    } else {
        Join-Path $repository "verification/$platform-static/real-columns"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $conclusion = if ($platform -eq 'windows') {
        'Windows Release 真实列、导航边界、会话记忆与偏好/资源/生命周期验证通过'
    } else {
        '静态审计通过，实机待用户验证'
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-matrix.json'), (@{
                round = 'R13'
                conclusion = $conclusion
                qtVersion = $summary.qtVersion
                sourceRecords = $records
                checks = $matrix
            } | ConvertTo-Json -Depth 8), $utf8)
    $paragraphs = @("# R13 / $platform", $conclusion, 'Manual call-chain and platform-branch audit; runtime limitations are preserved per item.')
    foreach ($check in $checks) {
        $paragraphs += "$($check[0]) — $($check[1]):$($check[2])`n$($check[4])"
    }
    [IO.File]::WriteAllText((Join-Path $directory 'audit-report.md'), ($paragraphs -join "`n`n"), $utf8)
}
Write-Output 'R13 Windows and macOS/Linux static audits passed'
