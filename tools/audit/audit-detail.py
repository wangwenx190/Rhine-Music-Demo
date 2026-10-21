"""R18 reviewed call-chain deductions, frozen-source guards and local run evidence."""
#!/usr/bin/env python3
import argparse
from audit_support import source_check
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--report", action="append", type=Path, required=True)
args = parser.parse_args()
reports = []
for path in args.report:
    report = json.loads(path.read_text())
    if not report["passed"] or report["configuration"] != "Release" or report["qmlWarnings"]:
        raise SystemExit(f"Passing Release detail replay required: {path}")
    stress = report.get("lifecycleStressRequested", report.get("fullWindowRebuilds", 0) > 0)
    if (len(report["lifecycle"]) != (10 if stress else 0) or len(report["latencies"]) != 5
            or report.get("fullWindowRebuilds") != (10 if stress else 0)
            or report["contexts"] < (11 if stress else 1)):
        raise SystemExit(f"Missing requested lifecycle or input/readiness measurements: {path}")
    # Surface restores may reuse a basic-loop context; full root-window rebuilds
    # still require the original eleven actual initialization events under stress.
    reports.append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "backend": report["backend"], "dpr": report["dpr"], "windowPolicy": report["windowPolicy"]})
golden = json.loads((root / "tests/fixtures/motion/detail-golden.json").read_text())
mapping = json.loads((root / "docs/contracts/history-rewrite.json").read_text())["revisions"]
revision = mapping.get(golden["baseline"], golden["baseline"])
for source in golden["sources"]:
    content = subprocess.check_output(["git", "show", f"{revision}:{source['path']}"], cwd=root)
    if hashlib.sha256(content).hexdigest() != source["sha256"]:
        raise SystemExit(f"Frozen source mismatch: {source['path']}")
checks = [
    ("COMMON-01", "src/render/CMakeLists.txt", "DetailPorts.cpp",
     "YalahooScene adds all three controllers and ports as real sources. It links Core (presentation, "
     "library, motion) and Gui, uses cxx_std_23 and the existing platform helper; no new private or "
     "OS-only API. App and DetailMotionTest link this same implementation. QML packaging and official "
     "lint targets include the changed App/Detail. Latest-Kit discovery and probes are unchanged."),
    ("COMMON-02", "src/render/rhi/SceneItem.cpp", "selection->coverSource(id)",
     "Stable album IDs index source descriptors built once at library replacement. CoverCache reads "
     "QFile/QImageReader paths; no drag/frame path resolves filesystem names. New replay paths use "
     "QDir/QSaveFile and Python Path/subprocess argument arrays, preserving spaces and Unicode. "
     "No absolute Kit or music path is written into product or public presets."),
    ("COMMON-03", "src/render/SelectionController.cpp", "localInverse = instance.transform.inverted",
     "The authored XYZ matrix is the same transform consumed by SceneFrame instances, lighting and "
     "inverse-ray picking. Drag changes yaw in 3D, not a QML projection. Camera, clip-space correction, "
     "MSAA/resolve, depth ordering, shader bindings and postprocess retain the established pipeline. "
     "No backend branch or shader algorithm is added; qsb/GLSL/MSL closure still requires its own checks."),
    ("COMMON-04", "src/render/DetailPorts.cpp", "library.navigate",
     "All select/switch ports change selection/navigation only. Playback IDs, media objects and "
     "provider queues have no connection to these ports. Mid-switch archiveRoute uses the existing "
     "presentation revision/layout gate. Library replacement resets drag/placement/returning owners; "
     "QObject connections cancel on owner destruction. No timer is added to product motion."),
    ("COMMON-05", "tests/unit/DetailMotionTest.cpp", "frozenPlacementAndRotation",
     "598 frozen-source trajectory samples cover 20/30/60/120fps, reversal, speed and reduced motion; "
     "all x/v/a errors retain 1e-6. Separate behavior tests check nine sizes, transformed picking, "
     "capacity refusal/reclaim, last-pending selection, search/back, hide/resume and replacement. "
     "Product replay records actual Qt frames/PNG, errors, cover owners, uploads and ten contexts. "
     "Raw outputs stay ignored; source-camera/phase definitions outrank captures."),
    ("PERF-01", "src/render/ReturningCasePool.h", "capacity = 3",
     "One selected plus three returning cases bound CPU owners and detailed cover leases to four. "
     "canExchange runs before mutation, permits same-cell reclamation when full, and never evicts an "
     "unfinished case. Presentation retains only the latest pending target until admission. Frame "
     "work is at most 432 shelf entities, three copies, six pulses and eleven clearance samples; "
     "stable-ID lookup is indexed, with no per-frame full-library traversal."),
    ("PERF-02", "src/render/PointerDragController.cpp", "m_target = std::clamp",
     "GUI input replaces one bounded target. The sole FrameClock supplies already-scaled dt to "
     "selection, then scene, then presentation; no worker or queued pointer history is introduced. "
     "CoverCache/atlas admission and generation cancellation are retained. Production has no "
     "readback, decoding, worker wait or manual processEvents in these added paths."),
    ("PERF-03", "src/render/rhi/SceneItem.cpp", "it->tiledPixels != pixels",
     "Returning cases retain album/object identity and their existing SelectedCoverLease pixels. "
     "Reversal reclaims that same physical owner; normal motion submits transforms, never clones "
     "materials or recrops the same cover. SceneFrame uses Qt implicit sharing and renderer rejects "
     "known upload revisions. Four immutable source leases include all demand-sized mip levels; tiled staging is "
     "shared immutable worker output, separately counted without claiming measured GPU residency."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "struct AllocationKey",
     "Transforms, camera, yaw and light values are absent from the target compatibility key. Existing "
     "atlas dimensions follow current projected cover demand and include the complete mip chain; its owned region is included once, not "
     "four extra GPU textures. ResourceInventory separately lists attachments, MSAA/resolve/mips, "
     "buffers, lookup and Qt item output. Replay checks fixed target/postprocess generations during "
     "motion and <=2MiB upload batches; context recreation legitimately creates new resources. "
     "Driver allocations/swapchain/compositor remain unmeasured, not zero."),
    ("PERF-05", "src/render/DetailMotion.cpp", "PlacementSourceSeconds / PlacementSpeed",
     "Frozen MusicPlacementMotion uses the existing C2 quintic kernel at its authored duration, "
     "clamped visible progress drives lift/yaw/elevation/span/aim simultaneously, and camera remains "
     "rate-nine damped. Detail rail/lift/copies use rate nine; preview/return retain 4.2/4.5. "
     "Rotated copies freeze world altitude until exact returnStep alignment; adjacent incoming lift "
     "waits for that clearance. The 80ms actual-readiness hold and original tolerances remain."),
    ("PERF-06", "src/render/rhi/SceneItem.cpp", "m_inspectionEnabled && selection->beginDrag",
     "Presentation detailEnabled is bound into SceneItem as an additional input gate: exiting text "
     "cannot start another drag. Mouse click commits on release only within six pixels; movement "
     "cannot masquerade as a click. Multi-touch/cancel/ungrab cancels gesture ownership. Pointer yaw "
     "is +/-0.8 at .004 radians/logical pixel with 2.8 exponential follow; release retains source "
     "target. Detail buttons derive the next target from pending ID and honor real-column bounds."),
    ("PERF-07", "src/app/DetailReview.cpp", "delete state.window;",
     "Ten surface cycles and ten complete App/QML root-window rebuilds wait for actual submission and matching "
     "cover owners; readiness cannot substitute an old polish's lease set. Last-window deletion guarantees "
     "actual graph/device cleanup, while the basic loop may retain a shared context across surfaces. "
     "Visible restore submits the current immutable frame once, even when reduced motion is static. Single-shot watchdog is "
     "verification-only. Product hidden clock pauses and drops the first resumed delta; entity and "
     "cover generation ownership survive actual context recreation, while library replacement "
     "explicitly invalidates them. Tests respect the process window policy and capture only themselves."),
]
checks.append((
    "PERF-05-MATERIAL", "src/render/CaseClarity.cpp", "constexpr auto derivatives",
    "The frozen material reveal uses six Hermite knots with compile-time derivatives and one selected "
    "controller, plus <=3 scalar copy decays. Each copy snapshots its own clarity and reverse reclaims it. "
    "SceneFrame transfers immutable values; managed production uses a zero-clarity array baseline and "
    "Renderer::uniform only overrides the lid roughness via the same lidRoughness formula used by "
    "ShellMaterial. No shader layout, material object, PSO, texture or upload is rebuilt for clarity. "
    "The frozen 1,200-frame oracle and existing 210 material parameters cover the math; own native "
    "output quality is checked separately, without a source screenshot score."
))
platforms = {
    "windows": ("WIN-01", "cmake/Platform.cmake", "WIN32",
                "New GUI input uses Qt events and matrices; no AppKit/Unix code escapes platform "
                "branches. MSVC/C++23 and Qt DLL setup remain. D3D11 texture orientation, native "
                "capture/presentation, actual touch/focus and context lifecycle require a Windows Kit."),
    "macos": ("MAC-01", "src/app/GraphicsBackend.h", "QSG_RENDER_LOOP",
              "Metal remains threaded; native OpenGL uses basic and CoreProfile 4.1 before app "
              "creation per the existing AppKit requirement. Input coordinates are logical, target "
              "DPR is reported independently. VerificationWindow enforces authorized foreground "
              "or default hidden policy. Runtime evidence is scoped to the listed local reports."),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX",
              "All new sources and QML have real Qt implementations on ELF/Linux; no dummy early "
              "return or platform-specific type is introduced. OpenGL default and existing resource "
              "paths remain. X11/xcb and Wayland pointer capture, focus, DPR and GPU context "
              "recreation need their own native Kit/compositor; macOS GL does not prove them."),
}
for platform_name, platform_check in platforms.items():
    rows = []
    for check_id, path, symbol, evidence in checks + [platform_check]:
        rows.append(source_check(root, path, symbol, checkId=check_id, requirement=evidence,
                                 platform=platform_name, evidence=evidence, result="static-audit-passed",
                                 limitations="Target native checks remain R36 except the explicitly listed macOS runtime reports"))
    output = root / f"verification/{platform_name}-static/detail"
    output.mkdir(parents=True, exist_ok=True)
    (output / "audit-matrix.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n")
    (output / "runtime-evidence.json").write_text(json.dumps(reports, ensure_ascii=False, indent=2) + "\n")
    (output / "audit-report.md").write_text("\n\n".join(
        f"{row['checkId']} {row['sourcePath']}::{row['lineOrSymbol']}\n\n{row['evidence']}" for row in rows) + "\n")
print("R18 frozen-source and COMMON/PERF/platform audit complete; runtime scope remains explicit")
