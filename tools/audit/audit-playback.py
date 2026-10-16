"""R24 public Qt playback source review; generated evidence remains local."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/media/PlaybackController.cpp"
product = SOURCE.read_text() + (ROOT / "src/media/PlaybackController.h").read_text()
for forbidden in ["QAudioSink", "QAudioDecoder", "QAudioBufferOutput", "AVAudioEngine", "CoreAudio", "QtMultimedia/private", "QProcess", "QNetwork"]:
    if forbidden in product:
        raise SystemExit(f"Forbidden song backend: {forbidden}")
CHECKS = [
    ("COMMON-01", "src/media/CMakeLists.txt", "PlaybackController.cpp PlaybackController.h",
     "Root CMake builds the controller into YalahooCore with public Qt Multimedia. Both production and Review register the same QML foreign type and panel/control resource closure. Embedded reference notice is in the shared asset closure."),
    ("COMMON-02", "src/media/PlaybackController.cpp", "url.isLocalFile()",
     "Every start validates absolute local QUrl/query/fragment, using scanner/imported sourceUrl rather than old browser/PCM fields. Unicode/# fixture uses fromLocalFile. Playback and navigation are separate; retained album-scoped queue IDs can survive index replacement through a controller-only friend boundary."),
    ("COMMON-03", "qml/Yalahoo/Music/components/PlaybackControls.qml", "onMoved",
     "Controls are Qt Quick items and values only. No media object or device handle enters immutable SceneFrame/render thread. Existing popup blur/descriptor graph is reused; no new GPU captures or shaders, and no changes to scene quality/camera/materials."),
    ("COMMON-04", "src/media/PlaybackController.cpp", "void PlaybackController::load",
     "One player/output, revision before disconnect/stop/clear/reconnect. Callbacks validate revision/current URL and actual Qt state. Intent protects late ready, device recovery and queued end. End commits once outside signal stack; stop cancels deferred source/seek, destruction clears ownership. Rate stays 1, all decoding/output is public Qt."),
    ("COMMON-05", "tests/unit/PlaybackTest.cpp", "void realEndAndUnsupportedFormat()",
     "Real Qt WAV playback/progress/pause/seek/natural end and malformed-input errors are separate from injected race/output-list branches. Empty lists and real device objects do not prove physical unplug or listening. Review uses actual production button/panel and preserves default hidden window policy."),
    ("PERF-01", "src/media/PlaybackController.cpp", "m_library->album(albumId)",
     "Album ID lookup uses session index; one retained album value queue and dedupe set are O(album tracks). Next/previous/position are O(1). No whole-library search, normalization or model reset occurs on media signals or visual frames."),
    ("PERF-02", "src/media/PlaybackController.cpp", "m_refreshPending",
     "Qt owns decoding threads. App hot paths have no disk IO/worker wait/manual event pumping. Device refresh coalesces to one QObject queued task; source callbacks and one end completion carry the current revision. No unbounded audio/UI request queue."),
    ("PERF-03", "src/services/LibrarySession.cpp", "void LibrarySession::adoptPlaybackIds",
     "Only playback IDs notify, without replacing immutable library or disrupting scanning/provider generations. Private friend accepts controller-validated retained queue IDs; public setPlaybackIds still validates current snapshot. Title/state/progress/device signals stay separate."),
    ("PERF-04", "src/media/PlaybackController.h", "QMediaPlayer m_player",
     "One song player and one output member outlive visual tree. No PCM buffers/decoder/output engine or graphics allocation is introduced. New controls have fixed cardinality; popup capture and old+new budgets retain existing ownership. Qt media internal allocations are not measured VRAM or claimed bounded bytes."),
    ("PERF-05", "src/media/PlaybackController.cpp", "m_lastPosition",
     "Last valid position is a recovery checkpoint, not a clock. Explicit zero/stop/new source reset it; transient backend zero cannot erase it before loss. Healthy devices use setDevice; source reset only for deferred source or observed Qt error during loss, preserving current intent/volume."),
    ("PERF-06", "qml/Yalahoo/Music/pages/MusicStage.qml", "stage.inputRouter.ownsFocus()",
     "Input ownership is reused for Space; controls own seek/volume/device input. Track row accent checks album+track identity. Preview never seeks on each move, release commits once; source changes cancel old drag. Header/scene do not query media per frame."),
    ("PERF-07", "src/app/main.cpp", "QCoreApplication::aboutToQuit",
     "Quit stops media before storage teardown. Visual rebuild/hide does not own media lifetime. No-output defer/reconnect, pause/stop, stale events and orphan queue are explicitly tested. Platform-specific hardware/backends/physical input remain R24-P, with graphics split under R34/R35."),
]
paths = sorted({path for _, path, _, _ in CHECKS} | {
    "CMakeLists.txt", "cmake/Assets.cmake", "cmake/BrowseQml.cmake", "cmake/TestPolicy.cmake",
    "src/app/QmlTypes.h", "src/app/CMakeLists.txt", "src/services/LibrarySession.h",
    "src/core/LibraryModels.cpp", "src/core/MusicTypes.h", "src/media/PlaybackController.h",
    "src/render/navigation/InputRouter.h", "src/render/navigation/InputRouter.cpp",
    "src/presentation/PanelController.cpp", "qml/Yalahoo/Music/App.qml",
    "qml/Yalahoo/Music/pages/Browse.qml", "qml/Yalahoo/Music/pages/Detail.qml",
    "qml/Yalahoo/Music/panels/LibraryPanels.qml", "qml/Yalahoo/Music/panels/PlaybackPanel.qml",
    "tools/review/LibraryAudit.cpp", "tools/review/LibraryAudit.h", "tools/review/UiAudit.cpp",
    "tools/review/CMakeLists.txt", "tests/unit/CMakeLists.txt", "tests/integration/CMakeLists.txt",
    "tests/support/AudioFixture.h", "resources/licenses/qt-playback-reference-NOTICE.md",
})
dependencies = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest()) for path in paths]
revision = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"]
frozen = []
for path in ["src/music-player.ts", "src/music-app.ts"]:
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    frozen.append(dict(sourcePath=path, revision=revision, sourceSha256=hashlib.sha256(data).hexdigest()))
for platform, identity, target in [
    ("macos", "MAC-01", "Actual public Qt backend and Metal hidden results remain local reports; physical unplug, output listening and native input/window/DPR remain R24-P/R34."),
    ("windows", "WIN-01", "Public Qt APIs use real native devices with no old private layout patch. Target compiler/plugins/audio backend/physical output and D3D11 remain R24-P/R34; OpenGL graphics remain R35."),
    ("linux", "LNX-01", "Local URLs and Qt Multimedia use native backend/device support. Native compiler/audio plugin/PipeWire or PulseAudio/ALSA availability and output remain R24-P/R34; GL GUI/input/lifecycle remain R35."),
]:
    rows = []
    for check_id, path, symbol, requirement in CHECKS + [(identity, "src/media/PlaybackController.cpp", "void PlaybackController::applyDevices", target)]:
        data = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(data.decode().split()):
            raise SystemExit(f"Playback source review drift: {path}::{symbol}")
        rows.append(dict(checkId=check_id, requirement=requirement, platform=platform,
                         sourcePath=path, lineOrSymbol=symbol, evidence=requirement,
                         result="source-reviewed", sourceSha256=hashlib.sha256(data).hexdigest(),
                         limitations="Source review binding only; execution, output-list injection, physical devices and listening are distinct facts. Missing targets retain original R24-P gates."))
    output = ROOT / "verification" / f"{platform}-static" / "playback"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=rows, dependencies=dependencies, frozen=frozen), indent=2) + "\n")
print("R24 public Qt playback COMMON/PERF/platform source bindings verified; target runtime is separate")
