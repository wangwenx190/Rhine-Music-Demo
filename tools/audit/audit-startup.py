"""R28 call-chain audit. Descriptors and source review never substitute for target runtime."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/render/CMakeLists.txt", "StartupCoordinator.cpp",
     "Root C++23/latest native Kit builds StartupCoordinator in YalahooScene. Product QmlTypes injects it through App/MusicStage; Review uses the identical stage. Public media and core remain independent of render resources. StartupTest/startup-qml provide fast/hidden functional coverage; startup-stress is separately opt-in and excluded from ordinary full regression."),
    ("COMMON-02", "tools/review/StartupAudit.cpp", "Snapshot fixture",
     "The stress input is generated under QTemporaryDir: 576 invented album IDs, 96 distinct 2048-square JPEGs, one muted generated WAV, plus embedded qrc assets. No user library is read, no songs are written, and release removes only owned temporary inputs. Resource paths use QUrl/QFile/QString; startup passes immutable snapshot values."),
    ("COMMON-03", "src/render/rhi/SceneItem.cpp", "void SceneItem::updatePolish",
     "RenderingActive gates GUI publication without changing source matrices, formats, samples, effects or render scale. Existing QRhi/ResourcePlan target compatibility, format and sample probes apply to D3D11/Metal/OpenGL. Review RenderControl target DPR is explicit; its endFrame waiting is measured separately from sync/render and is not a display timestamp."),
    ("COMMON-04", "src/app/main.cpp", "StartupCoordinator startup",
     "Coordinator owns only library display admission and visual clock suspension. PlaybackController remains outside the QML Loader, with no media commands on snapshot, hide, film, or reload. Real muted QtMultimedia source revision/current track/paused intent survive paused and playing visual rebuilds; repeated ownership stress uses the separate startup-stress entry; actual routing/hearing and other targets remain R28-P."),
    ("COMMON-05", "tools/build/verify-startup.py", "sourceSha256",
     "Serialized runs preserve binary/source/input construction hashes, OS/Qt/backend/DPR/quality, requested swap mode, raw samples, descriptor inventories, logs and failure exit status. No pixel comparison or historical result rewriting. A missing target/presentation sample cannot become a pass."),
    ("PERF-01", "src/render/rhi/SceneItem.cpp", "void SceneItem::preparePool",
     "Per-frame reconcile visits only <=432 array plus four owned entities, bounded pool/source hashes and the fixed atlas retained tiles. Startup LibrarySession index/sort rebuild is O(N) at admitted snapshot boundaries, not per frame. Snapshot coalescing has one pending value irrespective of incoming count."),
    ("PERF-02", "src/services/CoverCache.cpp", "void CoverCache::pump",
     "Cold SceneItem construction requests no cover until the real pool/fallback mode is known at active polish. Three decoder workers use global tryStart and a maximum of 96 pending identities. Suspension stops retry and admission; <=3 already-running completions may populate bounded cache/leases, with no visual polish or upload. Generation/wanted checks reject superseded work, and QObject receiver clearing prevents destroyed-cache callbacks. Final entry-point scope guards join the global decoder pool before QGuiApplication/font teardown; ordinary tree/cache destruction remains nonblocking. No readback, waiting or disk IO was added to GUI/render hot paths."),
    ("PERF-03", "src/render/StartupCoordinator.cpp", "bool StartupCoordinator::applyPending",
     "GUI consumes the latest immutable snapshot once per queued turn; Film freezes display replacement until handedOff. A pending scan noticed at scenePrepared changes preparation token and requires a fresh actual cover/render acknowledgement. Library models share Qt containers, and unchanged cover sources retain atlas/leases, avoiding full-image reupload on metadata changes."),
    ("PERF-04", "src/render/rhi/ResourcePlan.h", "navigation-ruler-depth-upper-bound",
     "Existing plan includes RHI pass attachments, atlas/owned cover slots, text layers, ruler Canvas, opening Canvas and local blur. No pass or texture added by the coordinator. Inventory is descriptor payload with shared attachments deduplicated; replacement overlap is a conservative upper bound. Driver residency, Qt internal fonts/buffers and compositor remain explicitly unobservable."),
    ("PERF-05", "src/motion/FrameClock.cpp", "m_workPending = false;",
     "Reduced-motion idle consumes the pending-frame marker before callbacks. New invalidation during a frame persists and wakes the same FrameAnimation. Ordinary authored waves still use the continuous clock; presentation transitions/track highlight retain clock demand. Unchanged SceneItem submission and stable Canvas paths/text do not repaint/rebuild; identical cover cache hits do not decode again."),
    ("PERF-06", "qml/Yalahoo/Music/pages/MusicStage.qml", "readonly property bool visualActive",
     "One production FrameAnimation is gated by window/minimized/application visibility and actual reduced-motion work. InputRouter inputReceived wakes queued input before flush. Snapshot notifications are coalesced while hidden; restore applies one latest snapshot. Existing virtual delegates and controller ownership remain unchanged, and no per-frame library JSON is introduced into the product."),
    ("PERF-06", "src/motion/MotionController.cpp", "void MotionController::observe",
     "SelectionController observes all four displayed springs through the shared clock. Mathematical observation always updates the archive handoff; stateChanged is emitted only when an exact value/velocity/acceleration differs. An unchanged reduced-motion frame therefore drains visual demand instead of scheduling itself again. StartupTest covers settled demand and a real subsequent row navigation; native idle/window behavior remains independently measured."),
    ("PERF-07", "src/render/rhi/SceneItem.cpp", "const auto epoch = ++m_telemetryEpoch;",
     "Suspend resets telemetry/diagnostic segments; old queued callbacks are epoch-rejected. Resume schedules latest polish/update and the shared clock discards its first interval. QObject/QML tree dies before OffscreenSurface invalidation and QRhi renderer-owned resources; resource counters must return to zero. Native surface/SG, real playing intent, driver peaks and normal VSYNC are separate R28-P/R36 facts."),
]
DEPENDENCIES = {path for _, path, _, _ in CHECKS} | {
    "src/render/StartupCoordinator.h", "src/motion/FrameClock.h", "src/render/rhi/SceneItem.h",
    "src/services/CoverCache.h", "tools/review/ArchivePoolAudit.cpp", "tools/review/CoverAudit.cpp",
    "tools/review/RhiFlickerAudit.cpp", "tools/review/RhiScaleAudit.cpp", "src/render/rhi/PreparedAssets.cpp", "src/render/rhi/RhiItem.cpp", "src/app/QmlTypes.h", "qml/Yalahoo/Music/App.qml",
    "tests/unit/StartupTest.cpp", "tests/unit/CoverCacheTest.cpp", "tests/unit/CMakeLists.txt",
    "tests/integration/CMakeLists.txt", "cmake/TestPolicy.cmake", "tools/review/CMakeLists.txt",
    "tools/review/OffscreenSurface.h", "tools/review/UiAudit.cpp", "src/services/LibrarySession.cpp",
    "src/library/LibraryStore.cpp", "src/media/PlaybackController.cpp",
}
# Only the listed REFDB-06 scenario boundary is used; no source or assets copied.
reference = ROOT.parent / "Rhine-Music-Database/Qt/tests/UiChecks.cpp"
reference_evidence = dict(reference="REFDB-06", symbol="idleRenderFrames through selectedBeforeRelease",
                          author="LBEILC", license="MIT", adopted="scenario ideas only; no code/assets copied")
if reference.is_file():
    reference_evidence["sourceSha256"] = hashlib.sha256(reference.read_bytes()).hexdigest()
source_hashes = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in sorted(DEPENDENCIES)}
for platform, target in [("windows", "D3D11"), ("macos", "Metal"), ("linux", "OpenGL/Vulkan")]:
    checks = []
    for check_id, path, symbol, evidence in CHECKS:
        if " ".join(symbol.split()) not in " ".join((ROOT / path).read_text().split()):
            raise SystemExit(f"Startup source binding changed: {path}::{symbol}")
        checks.append(dict(checkId=check_id, requirement=evidence, platform=platform,
                           sourcePath=path, lineOrSymbol=symbol, evidence=evidence,
                           sourceSha256=source_hashes[path], result="source-reviewed",
                           limitations=f"Target {target} runtime independent. Remaining native/physical/VSYNC and target OpenGL gaps retain the R28-P/R36 gates."))
    output = ROOT / "verification" / f"{platform}-static" / "startup"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=checks, sourceSha256=source_hashes,
                                                             reference=reference_evidence), indent=2) + "\n")
print("R28 COMMON/PERF source review bindings verified; target runtime conclusions remain separate")
