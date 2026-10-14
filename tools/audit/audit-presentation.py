#!/usr/bin/env python3
"""Source guards plus reviewed R17 call-chain deductions; outputs stay local."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
report_path = Path(sys.argv[1]) if len(sys.argv) == 2 else root / "build/release/verification/presentation/report.json"
report = json.loads(report_path.read_text())
if not report["passed"] or report["configuration"] != "Release" or report["qmlWarnings"]:
    raise SystemExit("A passing Release production Detail/TrackList report is required")
if len(report["captures"]) != 18 or report["trackCount"] != 10000 or not 0 < report["delegates"] < 100:
    raise SystemExit("Nine sizes, both themes and bounded 10,000-track virtualization are required")
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
     "Qt Quick owns text/clip/opacity rendering. No shader, target format, depth, MSAA or sampling "
     "change is introduced. The surface translates only the content panel to screen right, fades "
     "back/caption as well, and retains the exiting layer until completion. Backend-specific "
     "rendering remains in the existing renderer; qsb variants are checked by shader-tools."),
    ("COMMON-04", "src/render/DetailPorts.cpp", "ports.select",
     "Selection/navigation are the only ports called by this state machine. Playback IDs, "
     "QMediaPlayer and provider request queues are untouched. Track requested is a UI intent for "
     "R24; no simulated playing state is assigned. Library replacement cancels surface callbacks."),
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
     "Two fixed surfaces interpolate numbers; no render attachment or explicit layer is added. "
     "Qt Quick resources follow visible text delegates. GPU target graph and peak budgets are "
     "unchanged. Offscreen capture uses the existing separate RGBA8/depth-stencil audit target; "
     "readback is a validation operation and never a production hot path."),
    ("PERF-05", "src/presentation/SurfaceTransition.cpp", "std::sqrt",
     "Frozen enter/exit durations and cubic Bezier control points are ported, including the "
     "72ms minimum and remaining-opacity square-root duration for reversal. Motion dt is scaled "
     "once by FrameClock. Shared albumTitleParts preserves the frozen Han/Latin subtitle rule."),
    ("PERF-06", "qml/Yalahoo/Music/components/TrackList.qml", "reuseItems: true",
     "ListView uses visible/reuse delegates and 100px cache, not a track Repeater. Credits use "
     "the same virtualization. The only Repeater is eight facts. Browse fades overlays separately "
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
              "can establish geometry/resources; focus, visible restore and VSYNC remain R34."),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX",
              "New presentation code uses Qt values and qrc only. Existing ELF/RPATH/plugin "
              "branches remain, and no D3D backend is hardcoded. Target X11/Wayland focus, "
              "OpenGL text/input and native lifecycle need a Linux Kit/window manager in R34."),
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
                     "limitations": "Native visible input/focus/VSYNC and other-platform runtime remain R34"})
    out = root / f"verification/{platform}-static/presentation"
    out.mkdir(parents=True, exist_ok=True)
    (out / "audit-matrix.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n")
    paragraphs = [f"# R17 {platform} source audit", "Reviewed call-chain deductions with drift guards."]
    paragraphs += [f"{row['checkId']}: {row['sourcePath']}::{row['lineOrSymbol']}\n\n{row['evidence']}"
                   for row in rows]
    (out / "audit-report.md").write_text("\n\n".join(paragraphs) + "\n")
print("R17 COMMON/PERF/platform source matrices generated; target runtime is not inferred")
