#!/usr/bin/env python3
"""Source guards plus reviewed R17 call-chain deductions; outputs stay local."""
from shader_parameters import numeric_source
import hashlib
import math
import re
import json
from pathlib import Path
import sys
import subprocess

root = Path(__file__).resolve().parents[2]
report_path = Path(sys.argv[1]) if len(sys.argv) >= 2 else root / "build/release/verification/presentation/report.json"
report = json.loads(report_path.read_text())
if not report["passed"] or report["configuration"] != "Release" or report["qmlWarnings"]:
    raise SystemExit("A passing Release production Detail/TrackList report is required")
if len(report["captures"]) != (54 if report["offscreen"] else 18) or report["trackCount"] != 10000 or not 0 < report["delegates"] < 100:
    raise SystemExit("Nine sizes, both themes and bounded 10,000-track virtualization are required")
if not all(report["interaction"].values()) or len(report["lifecycle"]) != 10 or not all(c["passed"] for c in report["lifecycle"]):
    raise SystemExit("Real Qt tab input, scroll retention and ten lifecycle checks are required")
source_report = Path(sys.argv[2]) if len(sys.argv) >= 3 else root / "verification/presentation-source/report.json"
subprocess.run([sys.executable, str(root / "tools/review/compare-presentation-structure.py"),
                str(report_path), str(source_report), str(report_path.parent / "source-structure.json")], check=True)
mapping = json.loads((root / "docs/contracts/history-rewrite.json").read_text())["revisions"]
def record(revision, path, reason):
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=root)
    return dict(revision=revision, sourcePath=path, sourceSha256=hashlib.sha256(data).hexdigest(), evidence=reason)
frozen = [record(mapping["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"], "src/" + name, reason) for name, reason in [
    ("music-app.ts", "Production renderDetail, trackList, albumAbout, groupProducers, lookup states, setTab and sticky indicator."),
    ("music-theme-switch.css", "Final portrait back-button position overrides; explicit responsive font intent follows SDE-01."),
    ("music.css", "Final aspect and width rules, title override, facts, track borders and typography, offline badge and sticky sigma18 backdrop."),
    ("music-title.ts", "Actual title/translation split and markup, no synthetic face."),
    ("music-presentation.ts", "Actual state/ports, ready and focus lifetime."),
    ("ui-transitions.ts", "Two surfaces, interruption ownership and 150ms content reveal."),
    ("style.css", "Border-box and embedded font faces with font-synthesis:none."),
]]
history = [record(revision, path, reason) for revision, path, reason in [
    ("33e8260", "qml/Yalahoo/Music/pages/Detail.qml", "R17 initial responsive hierarchy; unsupported right/bottom/caption/facts/tab/row differences are corrected in G2."),
    ("33e8260", "src/presentation/DetailModel.cpp", "R17 immutable selected snapshot, facts and rows retained; format/codec/range units and source/lookup content corrected."),
    ("33e8260", "src/presentation/PresentationController.cpp", "R17 two surfaces, layout revision, batched notifications and replaceable pending target retained."),
    ("e98c798", "qml/Yalahoo/Music/components/TrackList.qml", "Separate typed ListViews and prepare-time top restoration retain native model safety; user/tab switching now preserves and clamps scroll."),
    ("ed5aaef", "src/presentation/PresentationController.cpp", "R18 canSelect backpressure and navigation ports retained; no scene or playback bypass."),
    ("ed5aaef", "qml/Yalahoo/Music/pages/Detail.qml", "R18 overline/navigation native metrics retained, with final source CSS geometry and input gates."),
    ("1c9b8c6", "src/presentation/SurfaceTransition.cpp", "R19 optional overview enter curve does not alter existing detail timing."),
    ("a04db48", "qml/Yalahoo/Music/components/ArchiveText.qml", "R19F fractional vector font metrics retained; CSS normal mixed-fallback line boxes now use separately rounded resolved ascent/descent."),
]]
# Validate the local Gaussian from the authored CSS sigma, never source pixels.
css = subprocess.check_output(["git", "show", mapping["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"] + ":src/music.css"], cwd=root, text=True)
tabs_css = re.search(r"\.music-tabs \{([^}]+)\}", css).group(1)
sigma = float(re.search(r"backdrop-filter: blur\(([0-9.]+)px\)", tabs_css).group(1))
weights = [math.exp(-i * i / (2 * sigma * sigma)) for i in range(int(3 * sigma) + 1)]
total = weights[0] + 2 * sum(weights[1:])
weights = [weight / total for weight in weights]
shader = (root / "resources/shaders/detail-tabs-blur.frag").read_text()
shader = numeric_source(shader)
actual_weights = [float(x) for x in re.findall(r"\)\s+\* ([0-9.]+);", shader)]
actual_offsets = [float(x) for x in re.findall(r"sampleStep \* ([0-9.]+)", shader)][::2]
expected_weights = [weights[0]] + [weights[i] + weights[i + 1] for i in range(1, len(weights), 2)]
expected_offsets = [i + weights[i + 1] / (weights[i] + weights[i + 1]) for i in range(1, len(weights), 2)]
if len(actual_weights) != len(expected_weights) or len(actual_offsets) != len(expected_offsets):
    raise SystemExit("Incomplete sticky backdrop Gaussian")
if any(abs(a - b) > 1e-10 for a, b in zip(actual_weights + actual_offsets, expected_weights + expected_offsets)):
    raise SystemExit("Sticky backdrop Gaussian differs from frozen CSS sigma")
checks = [
    ("COMMON-01", "CMakeLists.txt", "add_subdirectory(src/presentation)",
     "C++23 remains required. Latest native Kit discovery is unchanged. Presentation is in Core, "
     "without Quick private types; product and ui-audit register the same foreign types and package "
     "Detail/TrackList through BrowseQml. Tests use Release and the shared platform helper."),
    ("COMMON-02", "src/presentation/DetailModel.cpp", "m_snapshot = snapshot",
     "LibrarySession resolves navigation through its ID index. Detail retains the immutable snapshot "
     "and a pointer into it; URLs/notes/source fields are plain data. QML is qrc-based; file/metadata "
     "requests and absolute Kit paths are absent from presentation. No external links are fetched."),
    ("COMMON-03", "qml/Yalahoo/Music/pages/Detail.qml", "detailSurface.offset",
     "Qt Quick owns text/clip/opacity rendering. Sticky backdrop uses premultiplied source-over "
     "then two separable sigma18/54-radius Gaussian passes, with output-DPR text unaffected. The surface translates only the content panel to screen right, fades "
     "back/caption as well, and retains the exiting layer until completion. Backend-specific "
     "rendering remains in the existing renderer; qsb variants are checked by shader-tools."),
    ("COMMON-03-BACKDROP", "resources/shaders/detail-tabs-blur.frag", "sampleBackdrop",
     "Authored CSS sigma independently generates expected normalized weights/paired offsets. "
     "Horizontal uses premultiplied source-over, vertical skips the unused overlay; both retain "
     "the local logical Gaussian. Same-Kit qsb translates actual product/Review resources; target GPU/runtime remains separate."),
    ("PERF-05-TEXT", "src/presentation/DetailModel.cpp", "m_lineHeights.maxCost()",
     "Normal line boxes shape the entire string/font once and independently round resolved "
     "ascent/descent. The byte-bounded cache skips oversized keys; no image offset, global scale or sharpening."),
    ("COMMON-04", "src/render/DetailPorts.cpp", "ports.select",
     "Selection/navigation are the only ports called by this state machine. Playback IDs, "
     "QMediaPlayer and provider request queues are untouched. Track requested is a UI intent for "
     "R24; no simulated playing state is assigned. Library replacement cancels surface callbacks."),
    ("COMMON-05", "tools/review/PresentationAudit.cpp", "QTest::mouseClick",
     "Frozen production DOM/CSS boxes and native hierarchy are compared independently <=1 logical px. "
     "Actual Qt tab input, clamped scroll, bounded virtualization and lifecycle are distinct from own-output quality. No image matching."),
    ("PERF-01", "src/presentation/DetailModel.cpp", "case DiscHeading:",
     "Hot-path update has constant phase/readiness work. Prepare visits one selected album's "
     "tracks and credits once to calculate facts; rows read one immutable track by index. Disc "
     "headings compare only the preceding track. No sorting, decoding or file I/O runs per frame."),
    ("PERF-02", "src/presentation/PresentationController.cpp", "m_pending = PresentationSelection",
     "One pending target replaces older input. No worker, await or unbounded request queue is "
     "introduced. Layout-ready carries revision; retired/superseded layouts cannot unlock text. "
     "QML callLater merges the stable layout callback and does no production render readback."),
    ("PERF-03", "src/presentation/DetailModel.cpp", "snapshot == m_snapshot",
     "No track QVariant array is copied into QML. Equal snapshot/album/ordinal skips model reset; "
     "only prepare handoff rebuilds facts. Controller transact batches nested phase changes into "
     "one notification. No per-frame complete model/material/texture reconstruction occurs."),
    ("PERF-04", "src/presentation/SurfaceTransition.cpp", "m_fromOpacity",
     "Two fixed surfaces plus bounded tab numeric state use one clock. One visible tab backdrop "
     "owns three RGBA8 logical-grid textures of (contentWidth+108)*(tabHeight+108), two crops and "
     "one horizontal result; vertical output is direct. Loader retirement releases them. No 3D "
     "target compatibility key changes. Offscreen capture uses the existing separate RGBA8/depth-stencil audit target; "
     "readback is a validation operation and never a production hot path."),
    ("PERF-05", "src/presentation/SurfaceTransition.cpp", "std::sqrt",
     "Frozen enter/exit durations and cubic Bezier control points are ported, including the "
     "72ms minimum and remaining-opacity square-root duration for reversal. Motion dt is scaled "
     "once by FrameClock. Shared albumTitleParts preserves the frozen Han/Latin subtitle rule."),
    ("PERF-06", "qml/Yalahoo/Music/components/TrackList.qml", "reuseItems: true",
     "ListView uses visible/reuse delegates and 100px cache, not a track Repeater. Credits use "
     "the same virtualization: only visible rows instantiate at most floor((width+26)/236) persons. "
     "Eight facts use a fixed Repeater. Indexed grouping is once per prepare, text metrics cache "
     "is capped at 256KiB; sticky scroll uses current ListView coordinates. Browse fades overlays separately "
     "to preserve header layering. Phase enables input; SceneItem mouse/touch guards enforce it "
     "even outside the QML UI. Details scroll cannot navigate the archive or change playback."),
    ("PERF-07", "src/presentation/SurfaceTransition.cpp", "revision == m_revision",
     "Completion ownership is local and replaces old callbacks; reset invalidates revision. "
     "Reduced motion finishes the same callback path. FrameClock pauses visual dt when hidden; "
     "no new timer or lifecycle source is added. QObject-owned connections and engine-before-"
     "controller teardown prevent QML pointers outliving owners. Snapshot lives through model rows."),
]
platform_checks = {
    "windows": ("WIN-01", "cmake/Platform.cmake", "WIN32",
                "No Windows-only header or path is added to Core/QML. Existing platform helper "
                "and CTest PATH setup remain. Actual D3D11, window/input and lifecycle checks await "
                "a Windows native Kit/window manager; macOS images cannot establish them."),
    "macos": ("MAC-01", "src/app/VerificationWindow.h", "prepareWindow",
              "Audit uses shared Prohibited/native low-level window guards and RenderControl "
              "without native show. Logical layout is independent of DPR. Hidden Metal rendering "
              "can establish geometry/resources; focus, visible restore and VSYNC remain R36."),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX",
              "New presentation code uses Qt values and qrc only. Existing ELF/RPATH/plugin "
              "branches remain, and no D3D backend is hardcoded. Target X11/Wayland focus, "
              "OpenGL text/input and native lifecycle need a Linux Kit/window manager in R36."),
}
for platform, platform_check in platform_checks.items():
    rows = []
    for check_id, source_path, symbol, evidence in checks + [platform_check]:
        source = root / source_path
        content = source.read_text()
        if symbol not in content:
            raise SystemExit(f"Source drift: {source_path}: {symbol}")
        rows.append({"checkId": check_id, "requirement": evidence,
                     "platform": platform, "sourcePath": source_path, "lineOrSymbol": symbol,
                     "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                     "evidence": evidence, "result": "static-audit-passed",
                     "limitations": "Native visible input/focus/VSYNC and other-platform runtime remain R36"})
    out = root / f"verification/{platform}-static/presentation"
    out.mkdir(parents=True, exist_ok=True)
    (out / "audit-matrix.json").write_text(json.dumps(dict(frozen=frozen, history=history, checks=rows), ensure_ascii=False, indent=2) + "\n")
    paragraphs = [f"# R17 {platform} source audit", "Reviewed call-chain deductions with drift guards."]
    paragraphs += [f"{row['checkId']}: {row['sourcePath']}::{row['lineOrSymbol']}\n\n{row['evidence']}"
                   for row in rows]
    (out / "audit-report.md").write_text("\n\n".join(paragraphs) + "\n")
print("R19G2/R17 COMMON/PERF/platform source matrices generated; target runtime is not inferred")
