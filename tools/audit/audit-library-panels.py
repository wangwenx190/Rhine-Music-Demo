"""R23 source review bindings; runtime reports and generated matrices stay local."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "cmake/BrowseQml.cmake", "foreach(name SearchPanel LibraryPanel LibraryPanels PanelHeading",
     "Both product and Review compile the same panels, foreign C++ types and qsb resources. No web process or new dependency; Qt Quick Dialogs was already in the native closure."),
    ("COMMON-02", "src/library/LibraryStore.cpp", "bool LibraryStore::setRootPaths",
     "Editor paths become local URLs in C++; the existing lexical absolute-path/64-root validation, worker scanner, immutable snapshot handoff and atomic data-root persistence remain authoritative. FolderDialog URLs round-trip through toLocalFile; no JavaScript URL decoding or source music writes."),
    ("COMMON-03", "resources/shaders/panel-blur.frag", "void main()",
     "Two separable RGBA8 logical-pixel captures implement authored CSS sigma=8 Gaussian over the existing visible content. ShaderEffectSource smooth sampling and explicit logical texture dimensions preserve the logical filter grid; source scene rendering quality is unchanged. Analytic rectangle shadow adds no capture. Same Kit qsb emits platform targets; actual GPU execution is separate."),
    ("COMMON-04", "src/presentation/TrackFocusController.cpp", "bool TrackFocusController::locatePlayback()",
     "Playback IDs are read only. Requests validate album-scoped track identity, collapse overview and route through PresentationController select(...,true,true). Same open album bypasses re-extraction. No media object, source change, play or pause call exists in the focus path."),
    ("COMMON-05", "tools/review/LibraryAudit.cpp", "int reviewLibrary",
     "Production MusicStage/panels/detail run in Review with anonymous values and real Qt events. Geometry, focus, virtual lists, final exact track and navigation/playback separation are checked independently of own-render output. No source screenshot scoring."),
    ("PERF-01", "src/core/LibraryModels.cpp", "void SearchListModel::update",
     "Snapshot replacement builds O(albums+tracks+genres) normalized search entries and album/album-scoped track hashes once. Query processing normalizes only query and scans cached strings, O(total searched text + result count); role lookup is indexed. Old/new result lists and index are bounded by current snapshot, with no retained generations."),
    ("PERF-02", "src/presentation/TrackFocusController.cpp", "void TrackFocusController::cancel()",
     "One revision and one destination replace old requests; stale layout callbacks cannot begin scrolling. Store retains one worker and one latest scan. Reveal/paint hot paths do not read files, wait for workers or read GPU data. QML result covers are asynchronous and decoder-sized, with virtual visible delegates."),
    ("PERF-03", "src/core/LibraryModels.cpp", "beginRemoveRows",
     "Query updates retain the common prefix/suffix, update overlapping rows in one dataChanged batch and issue one range for only the size delta; no query reset, no per-row notification loop. Snapshot reset is transactional for changed role ownership. Search does not reorder library columns or mutate selection."),
    ("PERF-04", "src/render/rhi/ResourcePlan.h", "panel-blur-%1-color",
     "Two logical viewport-sized RGBA8 filter textures and conservative D32FS8 attachments join the cumulative descriptor plan while visible; Popup Loader destroys them on close. No mips/MSAA/production readback, and panel values do not change 3D PSO/target keys. Resize overlap includes old plus new plans; Qt internal/driver memory remains unknown."),
    ("PERF-05", "src/presentation/TrackFocusController.cpp", "void TrackFocusController::advance",
     "Reveal uses source cubic in/out scroll, source distance-dependent 320..700ms duration and two 1s CSS ease-in-out pulses, or immediate scroll plus 1100ms reduced-motion hold. It consumes raw dt from the single FrameClock; panel transitions consume motionDt. No extra timer/frame source."),
    ("PERF-06", "qml/Yalahoo/Music/panels/SearchPanel.qml", "reuseItems: true",
     "Results and detail tracks remain virtualized. Search notifications affect the result model, not the whole GUI. Popup traps focus and disables scene navigation, Escape dismisses, Find focuses query, arrows/Enter select stable IDs. User wheel/pointer/keyboard/resize cancels reveal; no unbounded pending input queue."),
    ("PERF-07", "qml/Yalahoo/Music/panels/LibraryPanels.qml", "active: root.visible",
     "Backdrop exists only during visible lifetime. Close completion handles one last target, reopening cancels stale close intent, and library replacement/back/opening cancels focus revisions. Default hidden VerificationWindow policy applies to the Review. Public Qt URLs/dialogs/models/events plus cross-target qsb preserve all three native implementations."),
    ("PERF-05", "qml/Yalahoo/Music/components/TrackList.qml", "function applyRevealScroll()",
     "Native extents and origin-relative target geometry preserve the source cubic distance while virtual row estimates change. Only realized rows are polished; CD heading is excluded from song centering. Revision-guarded queued focus occurs after layout, and user input/tab changes cancel old reveals."),
    ("PERF-07", "src/render/rhi/RhiItem.cpp", "replacementOldPlusNewUpperBoundBytes",
     "Panel layer size participates in descriptor invalidation without changing 3D target/PSO keys. Old plus new full plans include Qt UI upper bounds; actual RHI inventory, Qt/driver unknown storage and measured-VRAM null remain separate."),
]
mapping = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]
revision = mapping["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"]
frozen = []
for name in ["src/music-search.ts", "src/music-track-focus.ts", "src/music-app.ts", "src/music.css", "src/ui-transitions.ts"]:
    data = subprocess.check_output(["git", "show", f"{revision}:{name}"], cwd=ROOT)
    frozen.append(dict(sourcePath=name, revision=revision, sourceSha256=hashlib.sha256(data).hexdigest()))
focus = (ROOT / "src/presentation/TrackFocusController.cpp").read_text()
for forbidden in ["QMediaPlayer", "QAudioOutput", "setSource(", "->play(", "->pause("]:
    if forbidden in focus:
        raise SystemExit(f"Playback boundary violation: {forbidden}")
platforms = {
    "windows": ("WIN-01", "Public Qt URL/dialog/device-independent focus path; native D3D11 build, QML input, Unicode root picker and real output/playback integration remain R23-P/R36."),
    "macos": ("MAC-01", "Actual Release Metal hidden RenderControl checks are separate local reports. Native picker/physical input, screen DPR/focus and normal VSYNC remain R23-P/R36."),
    "linux": ("LNX-01", "Qt local paths remain case-sensitive and platform-native. Target build/dialog/physical input remains R23-P/R36; OpenGL graphics and lifecycle remain R36."),
}
dependencies = sorted({path for _, path, _, _ in CHECKS} | {
    "CMakeLists.txt", "cmake/TestPolicy.cmake", "src/app/QmlTypes.h", "src/app/CMakeLists.txt",
    "src/core/LibraryModels.h", "src/core/MusicTypes.cpp", "src/library/LibraryStore.h",
    "src/library/DirectoryScanner.cpp", "src/presentation/CMakeLists.txt",
    "src/presentation/TrackFocusController.h", "src/presentation/PanelController.h",
    "src/presentation/PanelController.cpp", "src/presentation/PresentationController.cpp",
    "src/render/DetailPorts.cpp", "src/render/SelectionController.cpp",
    "src/render/rhi/RhiItem.h", "src/render/rhi/SceneItem.h", "src/render/rhi/SceneItem.cpp",
    "src/services/LibrarySession.h", "src/services/LibrarySession.cpp", "src/services/CoverCache.cpp",
    "qml/Yalahoo/Music/App.qml", "qml/Yalahoo/Music/pages/MusicStage.qml",
    "qml/Yalahoo/Music/pages/Detail.qml", "qml/Yalahoo/Music/panels/LibraryPanel.qml",
    "qml/Yalahoo/Music/panels/PanelHeading.qml", "resources/shaders/panel-shadow.frag",
    "tools/review/CMakeLists.txt", "tools/review/LibraryReview.qml", "tools/review/UiAudit.cpp",
    "tests/unit/CMakeLists.txt", "tests/unit/TrackFocusTest.cpp", "tests/unit/LibraryDataTest.cpp",
    "tests/unit/DirectoryScannerTest.cpp", "tests/unit/CoverCacheTest.cpp",
    "tests/integration/CMakeLists.txt",
})
dependency_hashes = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest()) for path in dependencies]
for platform, (identity, evidence) in platforms.items():
    rows = []
    for check_id, path, symbol, requirement in CHECKS + [(identity, "src/app/main.cpp", "yalahoo::TrackFocusController trackFocus", evidence)]:
        data = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(data.decode().split()):
            raise SystemExit(f"Source review drift: {path}::{symbol}")
        rows.append(dict(checkId=check_id, requirement=requirement, platform=platform,
                         sourcePath=path, lineOrSymbol=symbol, evidence=requirement,
                         sourceSha256=hashlib.sha256(data).hexdigest(), result="source-reviewed",
                         limitations="Source reasoning binding only; native builds/tests/physical display claims require their actual reports and R23-P gates."))
    target = ROOT / "verification" / f"{platform}-static" / "library-panels"
    target.mkdir(parents=True, exist_ok=True)
    (target / "source-matrix.json").write_text(json.dumps(dict(checks=rows, frozen=frozen, dependencies=dependency_hashes), indent=2) + "\n")
print("R23 COMMON/PERF/platform review bindings verified; actual runtime remains separate")
