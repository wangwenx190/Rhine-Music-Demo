"""R19G1 historical provenance and reviewed production call paths, separate from runtime."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPPING = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]
BASELINE = MAPPING["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"]


def record(revision, path, reason):
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    return dict(revision=revision, sourcePath=path,
                sourceSha256=hashlib.sha256(data).hexdigest(), evidence=reason)


frozen = [record(BASELINE, "src/" + name, reason) for name, reason in [
    ("music-app.ts", "Actual detail ports, overview toggle/exit/input, stylesheet order and label host."),
    ("scene.ts", "Placement/lift, rate-nine detail navigation, returning world height/rotation/clarity, camera blend, transformed folding/picking and actual readiness."),
    ("music-camera.ts", "MusicPlacementMotion and MusicPresentation phases, quintic duration and 80ms readiness hold."),
    ("decryption.ts", "Six monotone Hermite knots, ready/start mapping and independent outgoing clarity decay."),
    ("music-overview.ts", "Rate-6.5 spring, source framing and bounded selected/expanded label collision admission."),
    ("music-overview-edge.ts", "Source smoother extent and pickability boundary."),
    ("music-overview-ui.ts", "Anchor/card split, current projection during exit, two-step entry, 460/240ms lifetime and focus ownership."),
    ("music-overview.css", "Actual <=600 CSS card dimensions, padding, typography, border, 87% tint, 12px backdrop blur and expansion transitions."),
    ("ui-transitions.ts", "Authored curves, interruption distance, callback revision and final hidden state."),
    ("music-real-columns.ts", "Per-column displayed depth and shared overview-progress centering."),
    ("style.css", "MiSans source faces, no synthetic 500 weight and border-box inheritance."),
]]
history = [record(revision, path, reason) for revision, path, reason in [
    ("33e8260", "src/presentation/PresentationController.cpp", "R17 ports/readiness/revisions are consumed by R18/R19. Detail text/layout/virtualization remains explicitly R19G2; no R17 completion inferred here."),
    ("e98c798", "src/render/SceneAdapter.cpp", "macOS followup: logical viewport and detail target/pointer/quality behavior. Target-resolution correction already has R19D4C provenance, not a new camera offset."),
    ("ed5aaef", "src/render/DetailMotion.cpp", "R18 authored 1.45/1.05 C2 placement and placing/presented/returning phases retained."),
    ("ed5aaef", "src/render/SelectionController.cpp", "R18 independent selected/three-returning identities, clearance/world-height ownership, rate-nine detail vs ordinary rates, transform-based drag and pick."),
    ("ed5aaef", "src/render/CaseClarity.cpp", "R18 reveal and independent per-copy decay retained; scalar override uses the existing optical lid formula."),
    ("ed5aaef", "src/render/rhi/SceneItem.cpp", "R18 actual input gates and four cover leases; geometry/lights/pick share matrices, stale async work is generation-rejected."),
    ("ed5aaef", "src/app/DetailReview.cpp", "R18 matched cover entity/revision readiness and full root-window rebuilds retained; basic surface reuse cannot be called a new context."),
    ("1c9b8c6", "src/render/OverviewController.cpp", "R19 clock order, source framing, bounded physical projection and archive-only guided entry retained."),
    ("1c9b8c6", "src/render/OverviewLabelsModel.cpp", "R19 fixed current/retiring slots, hysteresis, anchor-follow and reversible source fade/expansion retained."),
    ("1c9b8c6", "qml/Yalahoo/Music/pages/Overview.qml", "R19 source card boxes and native text; count block width restored to CSS block width. Crop is restricted to the backdrop; labels stay at output DPR."),
    ("1c9b8c6", "resources/shaders/overview-blur.frag", "R19 sigma12/radius36 normalized discrete Gaussian, paired linear taps. Intentional local blur is an API adaptation, never a whole-scene sharpness compensation."),
    ("1c9b8c6", "src/render/OverviewLabelsModel.cpp", "R19 half-lane physical candidate endpoints are culled after priority selection, preventing loss of selected labels."),
    ("5454fb2", "src/render/rhi/SceneItem.cpp", "R19D4C removes duplicate reference-stage downscaling; overview and detail consume actual logical viewport/DPR targets."),
    ("cfce597", "qml/Yalahoo/Music/components/Header.qml", "R19E preserves transport row geometry while overview fades sibling controls; actual slot cap follows final portrait CSS."),
    ("a04db48", "qml/Yalahoo/Music/components/ArchiveText.qml", "R19F common whole-line/vector font metrics and actual DPR are consumed unchanged; no overview-specific screenshot offset."),
]]
CHECKS = [
    ("COMMON-01", "src/render/CMakeLists.txt", "OverviewController.cpp", "Scene/presentation/core/QML/qsb are linked into product and Review. Latest native Kit, C++23 and official lint targets retain common implementation closure."),
    ("COMMON-02", "src/render/rhi/SceneItem.cpp", "selection->coverSource(id)", "Stable ID indexes and immutable source descriptors feed bounded cover leases. No frame-path filesystem resolution, user music write or network. QML/blur use module/qrc resources."),
    ("COMMON-03", "src/render/SelectionController.cpp", "matrix.scale(1, float(extent), 1)", "Authored rotation precedes local-Y fold and rotated .175 bottom compensation. Same transform reaches opaque/DOF/ID/shell/pick; light orientation excludes fold. Common clip/Y/MSAA and sorted optical passes are unchanged."),
    ("COMMON-04", "src/render/DetailPorts.cpp", "library.navigate", "Detail switch and overview entry mutate navigation only. Current real playback/track locating remains R23/R24. No media/provider command in camera, fold, drag or label paths."),
    ("COMMON-05", "tools/review/OverviewAudit.cpp", "structurePassed", "Source CSS/DOM boxes, authored numeric fixtures, actual Qt input and own output quality are distinct. Successful image export alone no longer establishes structure. Native no-frame runs are invalid sampling, never functional passes."),
    ("PERF-01", "src/render/OverviewController.cpp", "std::ceil(center - 4.5)", "At most ten inclusive physical candidates, nine admitted and eighteen fixed model slots. Work scales with <=432 shelf instances plus three returning and one selected; no per-frame full-library scan."),
    ("PERF-02", "src/render/PointerDragController.cpp", "m_target = std::clamp", "One pointer/latest yaw and one pending selection; existing decode/backpressure retained. Unique scaled FrameClock orders overview progress before selection/scene and label sync after them. No product readback/wait/new timer."),
    ("PERF-03", "src/render/ReturningCasePool.cpp", "m_cases.erase(it)", "Reversal reclaims actual identity/lift/yaw/clarity and its cover lease. Full pool blocks admission rather than evicting unfinished copies. SceneFrame implicit sharing and change-aware uniforms avoid pixel/model/material recreation."),
    ("PERF-04", "qml/Yalahoo/Music/components/OverviewBackdrop.qml", "targetWidth: labelWidth + 76 + 72", "Two fixed RGBA8 padded crop/horizontal layers per live slot; vertical composition is direct. Source sigma12 blur uses logical grid, while text remains native DPR. Motion changes no 3D target compatibility keys. Descriptor and driver allocation remain distinct."),
    ("PERF-05", "src/render/CaseClarity.cpp", "constexpr auto derivatives", "One cached Hermite derivative table, actual reveal mapping and per-copy exp(-9dt). Column extent caches center/progress; published VP reused for label projection. No source-less image compensation found in these retained algorithms."),
    ("PERF-06", "src/render/OverviewLabelsModel.cpp", "if (previouslyLive[row]", "Signals affect only current/retiring bounded slots; projected anchors have no follow animation. Exit immediately disables entry. Shared clock stops while hidden and static scene/labels skip update. Detail text/scroll work is R19G2."),
    ("PERF-07", "tools/review/OverviewAudit.cpp", "cycle < requestedCycles", "Ten explicit root/RenderControl invalidations restore live labels, then retirement removes their blur Loaders. Default window policy must pass. Native same-window restore, physical input and VSYNC require separate target evidence."),
]
current_paths = {r[1] for r in CHECKS} | {
    "qml/Yalahoo/Music/pages/Overview.qml", "resources/shaders/overview-blur.frag",
    "src/render/DetailMotion.cpp", "src/render/DetailPorts.cpp", "src/render/SceneAdapter.cpp",
    "src/render/OverviewLabelsModel.cpp", "src/app/DetailReview.cpp", "src/app/OverviewReview.cpp",
    "tests/unit/DetailMotionTest.cpp", "tests/unit/OverviewTest.cpp",
    "tools/review/export-detail-golden.mjs", "tools/review/export-overview-golden.mjs",
    "tools/review/export-browse-ui.mjs", "tools/review/overview-ui-harness.js",
    "tools/review/compare-overview-structure.py", "tools/audit/audit-detail.py",
}
current = [dict(sourcePath=p, sourceSha256=hashlib.sha256((ROOT / p).read_bytes()).hexdigest())
           for p in sorted(current_paths)]
for platform, marker in [("windows", "WIN32"), ("macos", "YalahooVerification"),
                         ("linux", "UNIX AND NOT APPLE")]:
    rows = []
    for identity, path, symbol, reason in CHECKS + [
        (platform.upper() + "-01", "cmake/Platform.cmake", marker,
         "Shared Qt implementations and native compiler/backend/resource branches remain. Host qsb target translation does not establish target GPU/runtime.")
    ]:
        data = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(data.decode().split()):
            raise SystemExit(f"Source drift: {path}::{symbol}")
        rows.append(dict(checkId=identity, requirement=reason, platform=platform,
                         sourcePath=path, lineOrSymbol=symbol, evidence=reason,
                         sourceSha256=hashlib.sha256(data).hexdigest(), result="static-audit-passed",
                         limitations="Runtime/quality checked separately. Missing target systems, native visibility/input/VSYNC remain R34; R17 presentation review remains R19G2."))
    output = ROOT / "verification" / f"{platform}-static" / "detail-overview" / "r19g1"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(
        frozen=frozen, history=history, current=current, checks=rows), indent=2) + "\n")
print("R19G1 motion/overview history and COMMON/PERF/platform audit passed; R17 remains R19G2")
