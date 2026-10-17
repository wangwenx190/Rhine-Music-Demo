"""R25 public Qt ambience source bindings; runtime facts are recorded separately."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/media/CMakeLists.txt", "BackgroundMusicController.cpp BackgroundMusicController.h",
     "Root/Core build and public Multimedia linkage own one BGM player/output and at most four QSoundEffects. Shared QmlTypes/App/MusicStage/LibraryPanels close both application and ui-audit resources; production assets keep existing attribution."),
    ("COMMON-02", "src/media/InterfaceSoundController.cpp", "source.scheme() == \"qrc\"_L1",
     "BGM uses the embedded atmosphere Ogg; event sources accept existing embedded audio URLs only, no user song writes/network/processes. All seventeen absent short assets stay silent. Preferences use the existing QSaveFile ownership and atomic commit."),
    ("COMMON-03", "qml/Yalahoo/Music/panels/PlaybackPanel.qml", "objectName: \"bgmVolume\"",
     "Fixed controls use existing popup/scroll/render ownership, no shader/attachment/scene quality changes. BGM and effects follow system default, song retains independent Qt device list. No audio objects enter SceneFrame or render thread."),
    ("COMMON-04", "src/media/PlaybackController.cpp", "m_background && !m_background->silent()",
     "All play intents reach BGM before ready; ready holds backend play until silence, rechecks current source/status/intent. Pause/error suppress ambience, explicit stop/final queue end restore; intermediate queue changes keep Playing. One replaceable fade and context-bound callbacks prevent stale starts."),
    ("COMMON-05", "tests/unit/AudioAmbienceTest.cpp", "void realBgmGateInterruptionsAndHiddenPlayback()",
     "Actual Qt Ogg and anonymous WAV test the silence gate, pause/stop/queue completion and navigation isolation. Test-only short WAV exercises four QSoundEffects and saturation. Missing resources, independent settings and persistence have separate assertions; no physical output/listening claim."),
    ("PERF-01", "src/media/InterfaceSoundController.h", "std::array<std::unique_ptr<QSoundEffect>, 4> m_pool",
     "Event hash lookup plus at most four slots is constant bounded work, drops saturation with no pending queue. Manifest scanned once at construction; no library traversal, per-frame audio polling or media allocation on actions."),
    ("PERF-02", "src/media/BackgroundMusicController.cpp", "m_refreshPending",
     "Public Qt owns decode/output threads. One coalesced device refresh per controller; one 24ms volume timer only while fading, elapsed-time progress. Necessary audio continues without visual ticks; GUI audio hot paths never wait/read files/pump events."),
    ("PERF-03", "src/services/PreferencesStore.cpp", "void PreferencesStore::setAudio",
     "Only changed audio fields merge into the existing implicitly shared JSON; one worker plus one coalesced latest save. Import copies audio values before notifications, preserving current values and unknown extensions. UI observes settings separately from fade output volume."),
    ("PERF-04", "src/media/BackgroundMusicController.h", "QMediaPlayer m_player",
     "Native controllers outlive QML, no GPU resources or custom PCM. Existing full BGM asset retained; zero effect instances when no playable assets, maximum four when available. Qt backend internal memory is not claimed as measured bounded bytes."),
    ("PERF-05", "src/media/BackgroundMusicController.cpp", "if (target == m_target)",
     "Repeated activity/settings cannot restart identical fade. New target starts at actual current output volume, independent user BGM volume remains stable. Same short resource reuses effect source; absent events do no loading."),
    ("PERF-06", "qml/Yalahoo/Music/panels/PlaybackPanel.qml", "onMoved",
     "Only user interaction writes volumes. Fade publishes no per-step settings notifications, controls use the existing modal input owner/virtual scene lifecycle. No QML timer/media instance, no sound-driven visual wakeup."),
    ("PERF-07", "src/app/main.cpp", "background.stop()",
     "Quit stops song before BGM/effects; destruction stops timer/player and clears source/output. Hidden media remains alive, pause cancels any restore, new play cancels restore before opening gate. Devices route through Qt public objects on all platforms; physical recovery/listening remains R25-P."),
]
paths = {path for _, path, _, _ in CHECKS} | {
    "src/media/BackgroundMusicController.cpp", "src/media/BackgroundMusicController.h",
    "src/media/InterfaceSoundController.cpp", "src/media/InterfaceSoundController.h",
    "src/media/PlaybackController.cpp", "src/media/PlaybackController.h", "src/app/QmlTypes.h",
    "src/app/CMakeLists.txt", "src/services/PreferencesStore.h", "cmake/Assets.cmake", "cmake/BrowseQml.cmake",
    "qml/Yalahoo/Music/App.qml", "qml/Yalahoo/Music/pages/MusicStage.qml",
    "qml/Yalahoo/Music/panels/LibraryPanels.qml", "tools/review/LibraryAudit.cpp",
    "tools/review/CMakeLists.txt", "tests/unit/CMakeLists.txt", "cmake/TestPolicy.cmake",
    "resources/audio/interface-events.json", "resources/audio/README.md",
}
media = "\n".join((ROOT / path).read_text() for path in paths if path.startswith("src/media/"))
for forbidden in ["QAudioSink", "QAudioDecoder", "QAudioBufferOutput", "AVAudioEngine", "CoreAudio", "QtMultimedia/private", "QNetwork", "QProcess"]:
    if forbidden in media:
        raise SystemExit(f"Forbidden ambience backend: {forbidden}")
revision = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"]
frozen = []
for path in ["src/music-player.ts", "src/music-app.ts"]:
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    frozen.append(dict(sourcePath=path, revision=revision, sourceSha256=hashlib.sha256(data).hexdigest()))
dependencies = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest()) for path in sorted(paths)]
for platform, platform_id in [("macos", "MAC-01"), ("windows", "WIN-01"), ("linux", "LNX-01")]:
    checks = []
    for check_id, path, symbol, evidence in CHECKS + [(platform_id, "src/media/BackgroundMusicController.cpp", "void BackgroundMusicController::refreshOutput", "Public Qt default-device routing; actual native Qt plugin, devices and audio recovery require this target. Windows/Linux execution, physical route changes/listening and native minimize remain R25-P; GUI backend split remains R34/R35.")]:
        data = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(data.decode().split()):
            raise SystemExit(f"Ambience source drift: {path}::{symbol}")
        checks.append(dict(checkId=check_id, requirement=evidence, platform=platform,
                           sourcePath=path, lineOrSymbol=symbol, evidence=evidence,
                           sourceSha256=hashlib.sha256(data).hexdigest(), result="source-reviewed",
                           limitations="Source bindings only; actual Qt progress, physical devices, listening and native windows are separate. Missing target execution retains R25-P original gates."))
    output = ROOT / "verification" / f"{platform}-static" / "audio-ambience"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=checks, dependencies=dependencies, frozen=frozen), indent=2) + "\n")
print("R25 public Qt ambience COMMON/PERF/platform source bindings verified; runtime is separate")
