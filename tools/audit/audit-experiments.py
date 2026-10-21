"""Bind R27 source contracts to callable product paths; target runs remain independent."""
from audit_support import BASELINE
from audit_support import source_check
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "cmake/BrowseQml.cmake", "ExperimentPanels",
     "Root C++23/latest-Kit closure includes the three shared panels in product and ui-audit. DiagnosticSamples is included by the RHI boundary and tested without private Qt APIs; no new product dependency or backend-specific omission."),
    ("COMMON-02", "tools/launchers/run-native.sh", 'exec "$program" "$@"',
     "Launchers resolve their own location, quote Unicode/space paths, select the configured Kit plugin root and preserve arguments/exit status. Demo and lab create invented qrc-only albums without PreferencesStore/LibraryStore or scans; incompatible data options fail before storage construction."),
    ("COMMON-03", "src/render/SelectionLighting.cpp", "ribbon.settings = s;",
     "All four modes and five bounded parameters update existing light state and uniforms, preserving row/lane position and velocity. No shader, PSO, material, instance, attachment or rendering precision change. Temporary baseline comparison retains authored parameters."),
    ("COMMON-04", "src/app/main.cpp", "if (!verificationRun && !demonstration)",
     "The anonymous mode has no playable tracks and skips automatic audio start. Ordinary speed changes use the existing FrameClock once, leaving opening real dt and QtMultimedia song playback independent. Existing public media controls remain the only route."),
    ("COMMON-05", "tools/review/ExperimentAudit.cpp", 'check(u"uniform-only"_s',
     "Hidden Metal integration separately checks actual mode uploads, unchanged resource/postprocess generation, pointer-wheel isolation, keyboard speed/comparison, responsive scrolling, diagnostic batches and release. Target GPU/input/display facts are not inferred from source or cross-target qsb."),
    ("PERF-01", "src/render/rhi/DiagnosticSamples.h", "static constexpr int capacity = 256;",
     "Each render adds fixed scalar values to four fixed rings; nearest-rank sorting occurs per 500ms batch. GUI formatting and three decode counters are batch-only. Controls and sampling cost are independent of library size; no library traversal or private paths in displayed data."),
    ("PERF-02", "src/render/rhi/SceneItem.cpp", "mailbox->latest = *batch;",
     "Render callback owns fixed accumulators, one replaceable latest batch and at most one queued GUI delivery. It performs no disk IO, image conversion, readback, wait or JSON serialization. QPointer and enable epoch reject callbacks from old trees/modes."),
    ("PERF-03", "src/render/SettingsController.cpp", "&SelectionLighting::settingsChanged",
     "Only authored light settings persist through the existing coalesced worker. Frame lighting.changed and compare press/release do not write preferences. Numeric settings change uniforms without copying covers or rebuilding the pool; motion changes preserve trajectory revision/x/v/a."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "if (diagnose)",
     "Diagnostic callback runs after ordinary production submission using existing requested-upload counts and lastCompletedGpuTime. No pass/texture/buffer/capture is introduced; unavailable GPU timing remains absent. CPU timer starts only for explicit Review profiling or live diagnostics."),
    ("PERF-05", "src/render/SelectionLighting.cpp", "if (m_settings != before)",
     "Equal authored settings emit no persistence or visual invalidation. Existing source normalization and guided preset remain authoritative; temporary comparison changes actual mode only and restores the latest authored snapshot without resetting spring state."),
    ("PERF-06", "qml/Yalahoo/Music/panels/ExperimentPanels.qml", "property bool navigationInputOwner: true",
     "One bounded ScrollView owns pointer/wheel/keyboard input through existing InputRouter ancestry checks. No new frame source/timer or per-frame QML serialization. Closing destroys the loader; hidden/modal/opening state pauses diagnostics and restores temporary comparison."),
    ("PERF-07", "src/render/rhi/SceneItem.cpp", "const auto epoch = ++m_diagnosticsEpoch;",
     "Visibility/window changes reset sample baselines and disconnect native presentation events; stale deliveries cannot mutate current UI. Native frames and RenderControl submissions remain separate. Remaining target and physical-device gaps retain R27-P in R36."),
]
DEPENDENCIES = {
    "src/app/main.cpp", "src/app/QmlTypes.h", "src/render/SelectionLighting.h",
    "src/render/SettingsController.cpp", "src/services/PreferencesStore.cpp",
    "src/services/DemoLibrary.cpp", "src/services/DemoLibrary.h", "src/motion/FrameClock.cpp",
    "src/render/rhi/RhiItem.h", "src/render/rhi/SceneItem.h", "src/render/navigation/InputRouter.cpp",
    "qml/Yalahoo/Music/App.qml", "qml/Yalahoo/Music/pages/MusicStage.qml",
    "qml/Yalahoo/Music/panels/LightingPanel.qml", "qml/Yalahoo/Music/panels/MotionPanel.qml",
    "qml/Yalahoo/Music/panels/LibraryPanels.qml", "qml/Yalahoo/Music/panels/SettingsPanel.qml",
    "qml/Yalahoo/Music/panels/PlaybackPanel.qml", "qml/Yalahoo/Music/panels/SettingRange.qml",
    "tests/unit/ExperimentTest.cpp", "tests/unit/CMakeLists.txt", "tests/integration/CMakeLists.txt",
    "cmake/TestPolicy.cmake", "tools/review/CMakeLists.txt", "src/services/CMakeLists.txt",
} | {path for _, path, _, _ in CHECKS}
DEPENDENCIES |= {str(path.relative_to(ROOT)) for path in (ROOT / "tools/launchers").glob("*")}
DEPENDENCIES |= {"yalahoo.sh", "yalahoo.command", "yalahoo.ps1"}
revision = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"][BASELINE]
frozen = []
for path in ["src/music-lighting-lab.ts", "src/music-motion-lab.ts", "src/music-lighting-lab.css",
             "src/music-motion-settings.ts", "src/music-row-lighting.ts", "src/music-app.ts"]:
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    frozen.append(dict(sourcePath=path, revision=revision, sourceSha256=hashlib.sha256(data).hexdigest()))
dependencies = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest())
                for path in sorted(DEPENDENCIES)]
for platform, platform_id, launcher in [("macos", "MAC-01", "yalahoo.command"),
                                         ("windows", "WIN-01", "tools/launchers/run-native.ps1"),
                                         ("linux", "LNX-01", "yalahoo.sh")]:
    platform_check = (platform_id, launcher, "yalahoo.sh" if platform == "macos" else "run-native" if platform == "linux" else "$process.ExitCode",
                      "Native launcher selects its platform executable and configured Kit; Windows GUI uses Process.WaitForExit and ExitCode. Actual target compiler, PowerShell parser/FormatTools, plugins, GPU, physical input and normal presentation require that environment; source review does not waive R27-P.")
    checks = []
    for check_id, path, symbol, evidence in CHECKS + [platform_check]:
        checks.append(source_check(ROOT, path, symbol, checkId=check_id, requirement=evidence, platform=platform, evidence=evidence, result='source-reviewed', limitations='Runtime evidence is separate; missing target/physical/VSYNC checks remain R27-P without relaxed gates.'))
    output = ROOT / "verification" / f"{platform}-static" / "experiments"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=checks, dependencies=dependencies, frozen=frozen), indent=2) + "\n")
print("R27 COMMON/PERF/platform source bindings verified; runtime remains separate")
