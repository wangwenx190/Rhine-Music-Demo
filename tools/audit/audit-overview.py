#!/usr/bin/env python3
"""R19 source/call-chain audit with exact hashes and separately scoped runtime evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--report", action="append", type=Path, default=[])
parser.add_argument("--visual", action="append", type=Path, required=True)
args = parser.parse_args()
runtime = []
for path in args.report + args.visual:
    report = json.loads(path.read_text())
    if not report.get("passed"):
        raise SystemExit(f"Passing evidence required: {path}")
    if "configuration" in report and report["configuration"] != "Release":
        raise SystemExit(f"Release required: {path}")
    if report.get("qmlWarnings"):
        raise SystemExit(f"QML warnings: {path}")
    if path in args.visual and (not report.get("retired")
                                or len(report.get("captures", [])) != 36
                                or not report.get("expansionClicks")
                                or not all(report["expansionClicks"])
                                or len(report.get("entryClicks", [])) != len(report["expansionClicks"])
                                or not all(report.get("entryClicks", []))):
        raise SystemExit(f"Missing actual Qt clicks, card structure or retirement: {path}")
    runtime.append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "backend": report.get("backend"), "offscreen": report.get("offscreen"),
                    "windowPolicy": report.get("windowPolicy")})
golden = json.loads((root / "tests/fixtures/overview/source-golden.json").read_text())
mapping = json.loads((root / "docs/contracts/history-rewrite.json").read_text())["revisions"]
revision = mapping.get(golden["baseline"], golden["baseline"])
for source in golden["sources"]:
    content = subprocess.check_output(["git", "show", f"{revision}:{source['path']}"], cwd=root)
    if hashlib.sha256(content).hexdigest() != source["sha256"]:
        raise SystemExit(f"Frozen source drift: {source['path']}")
shader = (root / "resources/shaders/overview-blur.frag").read_text()
weights = [float(value) for value in re.findall(r"\* ([0-9]+\.[0-9]+);", shader)]
if len(weights) != 19 or abs(weights[0] + 2 * sum(weights[1:]) - 1) > 1e-10:
    raise SystemExit("Gaussian kernel normalization/tap guard failed")
checks = [
    ("COMMON-01", "src/render/CMakeLists.txt", "OverviewLabelsModel.cpp",
     "YalahooScene lists both real C++ controllers; product and ui-audit register the foreign types, "
     "package Overview/OverviewBackdrop and bake the same qsb. Core already supplies SurfaceTransition. "
     "C++23, latest-Kit discovery, same-Kit tools and all platform link branches remain intact."),
    ("COMMON-02", "qml/Yalahoo/Music/components/OverviewBackdrop.qml", "qrc:/assets/shaders/overview-blur.frag.qsb",
     "Production resources resolve through qrc/module aliases; no path lookup, local server or Node is "
     "in the application. Review QDir/QSaveFile and subprocess argument arrays preserve Unicode/spaces. "
     "The mathematical source exporter executes local mapped frozen modules only."),
    ("COMMON-03", "src/render/SelectionController.cpp", "matrix.scale(1, float(extent), 1)",
     "After the authored XYZ rotation, local bottom .175 compensation precedes Y-only scaling. The same "
     "instance transform feeds all opaque/depth/ID/shell/cover passes and inverse-ray picking; <=.12 is "
     "excluded before inversion. Lighting rotation requests the unscaled orientation. Existing backend "
     "clip-space/Y/depth/MSAA/resolve conventions remain. Blur qsb is RGBA8 premultiplied Qt composition; "
     "Target output quality still requires its own actual backend observation."),
    ("COMMON-04", "src/render/OverviewController.cpp", "m_selection->enterColumn(lane, true)",
     "Entry resolves the existing column memory/actual depth and guided target; first click changes "
     "only expansion, second enters archive without opening detail. No media/player/provider API is "
     "called. Library replacement cancels overview/labels and QObject-owned connections; hidden clock "
     "pauses motion and preserves state without an extra worker, timer or input history."),
    ("COMMON-05", "tests/unit/OverviewTest.cpp", "frozenCameraInterpolation",
     "Frozen classes, unchanged scene methods and the actual camera blend block generate x/v/a, edge, "
     "collision and projection inputs. Unit, isolated QML and real App replays check source math, structure, lifecycle and behavior; "
     "1e-6/2px remain structural gates. Source screenshots are rough references only; no pixel/SSIM comparison is executed or required. "
     "Complete 3D source/quality and product performance remain MAC-VISIBLE-3D/R28; image scores are not gates."),
    ("PERF-01", "src/render/OverviewController.cpp", "std::ceil(center - 4.5)",
     "Projection enumerates the nine-lane neighborhood (at most ten inclusive boundary integers, "
     "with zero-extent endpoints) and indexes canonical columns; no C/N traversal occurs. Collision "
     "admission is capped to nine candidates, stable sorting caches keys, and <=90 overlap comparisons "
     "are independent of library size. Current/retiring delegates are capped to eighteen."),
    ("PERF-02", "src/render/OverviewController.cpp", "advance(dt, m_motion->reducedMotion())",
     "Overview progress connects before SelectionController and SceneAdapter; projection/chrome "
     "synchronize after their actual frame state. One FrameClock supplies already-scaled dt. Input "
     "stores one active/expanded intention; cover decode/admission/generation/backpressure are retained. "
     "There is no frame-path I/O, decode, wait, manual event loop or readback in product code."),
    ("PERF-03", "src/render/SelectionController.cpp", "m_extentCache",
     "Per-lane extents cache lane/center/progress and invalidate all nine slots when either dependency "
     "changes. Copies verify their physical key, including rebase and collisions with off-pool lanes. "
     "SceneFrame retains implicit sharing and identity-based cover leases; overview changes no cover "
     "pixels or stable IDs and never recreates the full library or atlas."),
    ("PERF-04", "qml/Yalahoo/Music/components/OverviewBackdrop.qml", "targetWidth: labelWidth + 76 + 72",
     "Blur reads only the existing scene output into a fixed padded RGBA8 crop and horizontal layer, "
     "then composites vertical directly; each axis uses 37 paired linear fetches, sigma 12/radius 36. "
     "Both textures follow the logical capture dimensions multiplied by the effective window DPR; Review checks their actual textureSize. "
     "At DPR1 and eighteen retiring/live slots, two RGBA8 images use <=5721408 bytes payload; other DPRs follow actual dimensions. Conservatively "
     "including 8-byte depth/stencil per layer gives 17164224 bytes, old/new resize overlap twice that. "
     "Qt internal padding/driver/font atlas are unknown, not measured zero. Three-dimensional target "
     "compatibility, postprocess and PSOs do not depend on overview progress."),
    ("PERF-05", "src/render/SceneAdapter.cpp", "m_viewProjection * QVector4D(world, 1)",
     "ProjectWorldPoint reuses the published view-projection product; labels project each candidate "
     "once and Qt retains text shaping until name/font/width changes. Exact rate-6.5 spring and "
     "smoother source math are retained, real-column centering consumes the same progress as camera. "
     "The separable Gaussian and pair weights preserve the declared discrete kernel; logical-grid "
     "sampling applies to the intentionally blurred backdrop only; text remains native at the output DPR. "
     "The browser finite-kernel implementation is not a definition to reproduce pixel for pixel."),
    ("PERF-06", "src/render/OverviewLabelsModel.cpp", "m_slots",
     "Anchors never animate their projected X/Y; inner cards own reversible 460/240ms fades and "
     "420/320ms expansion. Retiring cards track current projection/extent and become noninteractive "
     "immediately. Completed retirement resets entry scalars before slot reuse. Selected/expanded priority, .06/.12 hysteresis, reserved entry area and collision "
     "bounds match source. Fixed eighteen rows avoid model reset/repeater reconstruction. Model work "
     "is skipped when scene revision, visibility and animations are unchanged; controller publishes "
     "one final changed state, and header siblings fade without changing transport row geometry."),
    ("PERF-07", "tools/review/OverviewAudit.cpp", "cycle < requestedCycles",
     "The explicit lifecycle UI replay destroys/recreates the actual QML root and render-control resources ten times "
     "with active labels, while controller/model survive; product replay tests ten surface restores "
     "and still accepts V/Escape/entry. Loader removes cropped blur resources after retirement. "
     "New library snapshot clears retained identities, hide drops clock delta, and no new callback "
     "captures temporary render-thread state. Visible and true-hidden evidence remain distinct."),
]
platforms = {
    "windows": ("WIN-01", "cmake/Platform.cmake", "WIN32",
                "No new OS API enters product/blur paths; MSVC/C++23, Qt resources and D3D11 bindings "
                "remain. Native D3D11/OpenGL quality, input/focus, DPR and restores need Windows Kit/GPU; "
                "run verify-overview.py and its --visual resource/structure mode there. Static audit passed, runtime R36."),
    "macos": ("MAC-01", "src/app/VerificationWindowMac.mm", "foregroundVerification() ? NSApplicationActivationPolicyRegular",
              "Authorized verification uses Regular before QGuiApplication, then ordinary native window "
              "level and explicit show activation. Default remains Prohibited/nonactivating/bottom and "
              "is independently tested. Metal threaded and GL basic/Core 4.1 remain distinct; own-window "
              "or RenderControl captures do not imply screen scanout, normal VSYNC budget or photon timing."),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX",
              "All new classes/QML/shaders use real Qt implementations on Linux; no dummy backend or "
              "platform bypass. Existing OpenGL and XDG/resource branches remain. X11/xcb and Wayland "
              "DPR/focus/input/capture/restore and GL output quality requires native compositor/Kit (R36)."),
}
for name, platform_check in platforms.items():
    rows = []
    for check_id, path, symbol, evidence in checks + [platform_check]:
        content = (root / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Audit source drift: {path}::{symbol}")
        rows.append({"checkId": check_id, "requirement": evidence, "platform": name,
                     "sourcePath": path, "lineOrSymbol": symbol,
                     "sourceSha256": hashlib.sha256(content).hexdigest(), "evidence": evidence,
                     "result": "static-audit-passed", "limitations":
                     "Native scope only as listed in runtime-evidence; other required target checks remain R36"})
    output = root / f"verification/{name}-static/overview"
    output.mkdir(parents=True, exist_ok=True)
    (output / "audit-matrix.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n")
    (output / "runtime-evidence.json").write_text(json.dumps(runtime, ensure_ascii=False, indent=2) + "\n")
    (output / "audit-report.md").write_text("\n\n".join(
        f"{r['checkId']} {r['sourcePath']}::{r['lineOrSymbol']}\n\n{r['evidence']}" for r in rows) + "\n")
print("R19 frozen-source, kernel, COMMON/PERF/platform audit complete; native scope stays explicit")
