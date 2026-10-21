"""Bind R26 source review to current symbols; runtime reports stay independent."""
from audit_support import BASELINE
from audit_support import source_check
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/render/CMakeLists.txt", "SettingsController.cpp",
     "Root C++23/latest-Kit build links SettingsController through Scene/Core. Shared BrowseQml/QmlTypes include settings, metadata and control components in product and ui-audit; Review alone includes injection and App reload checks."),
    ("COMMON-02", "src/services/PreferencesStore.cpp", "file.setDirectWriteFallback(false)",
     "Native Unicode paths, owner-only QSaveFile and symlink rejection preserve original files on error. Explicit local JSON import is bounded, object/schema/type validated, revision-rejected and retains unknown fields, including nested quality extensions."),
    ("COMMON-03", "src/render/SettingsController.cpp", "&SelectionLighting::nightChanged",
     "Theme targets and motion update existing states/uniforms; depth of field updates the existing artistic effect strength. No new shaders, render targets or backend branches; panel uses native-density blur targets and existing ownership. Actual Metal UI, D3D11 and GL execution remain distinct facts."),
    ("COMMON-04", "qml/Yalahoo/Music/App.qml", "visualTree.active = false",
     "Reload only replaces the MusicStage visual tree; native song/BGM/effects, source, play intent, data and authored quality outlive it. Settings use existing public Qt media volume/device interfaces, not a second player or audio clock."),
    ("COMMON-05", "tests/unit/LibraryDataTest.cpp", "void batchGenerationAndMergedLateSubscribers()",
     "Queued results require matching requestId/generation/album/kind. Tests exercise old/new subscribers finishing together, cancel/replacement/destruction and statuses; settings-qml renders injected Matched/Uncertain/Error. Product uses OnlineMetadataProvider with explicit configuration; demo and verification use OfflineMetadataProvider."),
    ("PERF-01", "src/services/LibrarySession.cpp", "void LibrarySession::advanceBatch()",
     "A batch stores O(album-count) stable IDs and advances one request after each completion; UI reads only selected metadata through revision, not the whole result map per delivery. Credits serialize producerData only, without tracks or whole-album JSON. Fixed controls/delegates are independent of library size."),
    ("PERF-02", "src/services/PreferencesStore.cpp", "m_scheduled = true",
     "A patch validates only supplied fields and mutates changed keys, preserving implicit sharing instead of copying a whole JSON document per action. One event-loop-coalesced save intention and one worker/in-flight write; serialization/atomic IO run off GUI. Import shares the worker and stale revisions are rejected; teardown alone waits. Provider outstanding messages including cancellation acknowledgement are capped at eight and batch concurrency is one; network/JSON work runs on its own thread, with timer wakeups and no polling or subprocess."),
    ("PERF-03", "src/render/SettingsController.cpp", "if (m_applying)",
     "Field routes use runtime-owned states and implicitly shared values. Import applies a fixed snapshot under a persistence guard; nested unknown fields merge. No library model/cover/instance rebuild on theme/speed; sort/config changes are explicit snapshot operations."),
    ("PERF-04", "qml/Yalahoo/Music/panels/LibraryPanels.qml", "active: root.controller.panel === \"settings\"",
     "Settings loader only survives shown panel lifecycle. Backdrop remains the existing two RGBA8 native-density targets plus conservative depth/resize overlap in ResourcePlan; no new GPU allocation class or production readback. Closing releases panel and backdrop; unknown driver memory is not claimed measured."),
    ("PERF-05", "src/services/PreferencesStore.cpp", "if (!modified)",
     "No-op preferences do not write. Theme persistence uses target-nightChanged instead of animated lighting.changed; render size notifications cannot rewrite equal authored values. Artistic depth-of-field normalization stays centralized in RenderQuality."),
    ("PERF-06", "qml/Yalahoo/Music/panels/SettingsPanel.qml", "property bool navigationInputOwner: true",
     "Scroll/controls own input behind existing modal Popup; no per-frame JSON, timers or capture. Theme cards use source two-column layout, depth-of-field control and MusicBrainz fold, actual Qt focus/keyboard and small-screen scrolling. Status reads revision plus current stable album only."),
    ("PERF-07", "src/render/SettingsController.cpp", "m_library->cancelAllMetadata()",
     "Reload cancels request generation before QML replacement and stops the visual clock. Context-bound completions cannot target destroyed views; ten actual App visual replacements keep native media/source/intent/quality. Cross-platform native input/DPR/window recovery retains R26-P."),
]
PATHS = {path for _, path, _, _ in CHECKS} | {
    "src/app/main.cpp", "src/app/QmlTypes.h", "src/core/MusicTypes.cpp", "src/core/MusicTypes.h", "src/services/PreferencesStore.h",
    "src/services/LibrarySession.h", "src/services/OfflineMetadataProvider.cpp",
    "src/library/LibraryStore.cpp", "src/library/LibraryStore.h", "src/render/RenderQuality.cpp",
    "src/render/SettingsController.h", "src/presentation/PanelController.cpp",
    "qml/Yalahoo/Music/panels/SettingChoice.qml", "qml/Yalahoo/Music/panels/SettingRange.qml",
    "qml/Yalahoo/Music/panels/ThemeCard.qml", "qml/Yalahoo/Music/panels/SettingsSection.qml", "qml/Yalahoo/Music/panels/MetadataSection.qml",
    "qml/Yalahoo/Music/pages/MusicStage.qml", "tests/unit/SettingsTest.cpp",
    "tools/review/LibraryAudit.cpp", "tools/review/CMakeLists.txt", "cmake/BrowseQml.cmake",
    "tests/unit/CMakeLists.txt", "tests/integration/CMakeLists.txt", "cmake/TestPolicy.cmake",
}
for path in ["src/services/LibrarySession.cpp", "src/services/OfflineMetadataProvider.cpp",
             "src/render/SettingsController.cpp", "src/services/PreferencesStore.cpp"]:
    source = (ROOT / path).read_text()
    for name in ["QNetwork", "QProcess", "QDesktopServices"]:
        if name in source:
            raise SystemExit(f"Unexpected external request boundary: {path}: {name}")
revision = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"][BASELINE]
frozen = []
for path in ["src/music-app.ts", "src/music.css", "src/quality-settings.ts", "src/music-motion-settings.ts"]:
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    frozen.append(dict(sourcePath=path, revision=revision, sourceSha256=hashlib.sha256(data).hexdigest()))
dependencies = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest())
                for path in sorted(PATHS)]
for platform, platform_id in [("macos", "MAC-01"), ("windows", "WIN-01"), ("linux", "LNX-01")]:
    checks = []
    platform_check = (platform_id, "src/app/main.cpp", "yalahoo::SettingsController settings",
                      "Shared Qt/QML settings use platform-neutral paths/media/controls and existing backend selection. Target build/plugins, native keyboard/scroll/DPR and media survival need the target Kit/window/audio session. Missing targets retain R26-P under the R36 target/backend gates.")
    for check_id, path, symbol, evidence in CHECKS + [platform_check]:
        checks.append(source_check(ROOT, path, symbol, checkId=check_id, requirement=evidence, platform=platform, evidence=evidence, result='source-reviewed', limitations='Source review is separate from target execution; physical input/display/audio/VSYNC and absent environments retain original R26-P gates.'))
    output = ROOT / "verification" / f"{platform}-static" / "settings"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=checks, dependencies=dependencies, frozen=frozen), indent=2) + "\n")
print("R26 settings COMMON/PERF/platform source bindings verified; runtime is separate")
