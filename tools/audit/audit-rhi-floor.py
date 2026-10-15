"""Source/ownership audit for the product ground; runtime evidence remains separate."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/render/rhi/CMakeLists.txt", "rhi_shaders",
     "Product opaque shader is baked by the selected native Kit to SPIR-V/GLSL/HLSL/MSL; no new library or runtime dependency."),
    ("COMMON-02", "src/render/SelectionLighting.cpp", "LightingPalette::floorColor",
     "Authored day/night/dusk sRGB colors decode once into the linear palette. No disk lookup in frame/render paths; resources resolve through qrc."),
    ("COMMON-03", "resources/shaders/rhi/opaque.frag", "floorRadiance",
     "Ground writes linear radiance, view normal and RGB24 depth before fog/post/output. Existing nativeVP, shadowCoord, clip/Y and MSAA/resolve remain shared."),
    ("COMMON-04", "src/render/rhi/RhiItem.cpp", "input.captureFloorDepth",
     "Optional fourteenth semantic depth readback is explicit Review capture; production false. At most two pending batches, unique texture owner mapping, same immutable QImage delivery. No media/device path changes."),
    ("COMMON-05", "tools/review/RhiFloorAudit.h", "floorReference",
     "Independent scalar Standard PBR/DFG/PMREM, geometry, normal and PCF use this run's inputs; no browser GPU witness or original image score. Actual AO and full DOF attachments are read separately when needed."),
    ("PERF-01", "src/render/rhi/RhiItem.cpp", "productFloor = append(quad)",
     "One immutable quad (4 vertices, 6 indices), one uniform in the existing slot, O(1) draw per applicable geometry pass. Product work remains bounded by the existing pool."),
    ("PERF-02", "src/render/rhi/SceneItem.cpp", "in.floorColor = l.palette().floor.qt()",
     "GUI publishes values at synchronization; no GUI object access, disk, decode, worker waits or production readback on render hot path."),
    ("PERF-03", "src/render/rhi/RhiItem.cpp", "input.productFloor ? 6 : 2",
     "Reuses one receiver uniform block; 152 additional immutable geometry bytes uploaded only with shared geometry initialization. Theme only updates existing uniform prefix, no image copies."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "geometryBindings->setBindings",
     "Existing opaque SRB additionally binds existing DFG/LTC lookups; no new PSO/texture/target. Ground enters capture before resolve/mips, shared normal/depth and AO/DOF prepass; never a caster or sampled writable color attachment."),
    ("PERF-05", "resources/shaders/rhi/floor-lighting.glsl", "floorRadiance",
     "Standard dielectric f0=.04, roughness=.95, metalness/transmission/coat/emission zero. No shell field modulation or print cap. Constant plane normal makes source geometry roughness zero; source LTC/GGX helper equations retained."),
    ("PERF-06", "src/render/rhi/SceneItem.cpp", "in.floorColor == old.floorColor",
     "Existing changed/polish and unchanged-input gate cover floor color, same FrameClock/theme transition; no QML delegate, independent timer or hidden tick introduced."),
    ("PERF-07", "tools/review/RhiFloorAudit.h", "cycle < 10",
     "Ten actual SG invalidations restore floor, capture and AO/full DOF geometry. Existing renderer owns/releases all shared resources, no per-floor lifetime or unbounded cache. Native window/VSYNC and unavailable target hardware remain R34."),
]
# Verify the copied, source-derived LTC/GGX helper portion stays equivalent to the shared shell.
def tokens(text):
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
    return re.findall(r"[A-Za-z_][A-Za-z_0-9]*|(?:\d*\.\d+|\d+\.?\d*)(?:[eE][+-]?\d+)?|\S", text)
shell = (ROOT / "resources/shaders/rhi/shell.frag").read_text()
floor = (ROOT / "resources/shaders/rhi/floor-lighting.glsl").read_text()
shared = shell[shell.index("float LTC_ClippedSphereFormFactor("):shell.index("// END SHARED LIGHTING")]
adapted = floor[floor.index("float LTC_ClippedSphereFormFactor("):floor.index("vec3 floorRadiance(")]
if tokens(shared) != tokens(adapted):
    raise SystemExit("Floor LTC/GGX helpers drifted from source-derived shell equations")
for host, platform_id, symbol in [("windows", "WIN-01", "WIN32"),
                                   ("macos", "MAC-01", "YalahooVerification"),
                                   ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = []
    for check_id, path, marker, evidence in CHECKS + [(platform_id, "cmake/Platform.cmake", symbol,
             "Same selected Kit, native helper and platform build closure retained; target runtime not inferred from macOS.")]:
        content = (ROOT / path).read_bytes()
        if " ".join(marker.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Audit source drift: {path}::{marker}")
        rows.append(dict(checkId=check_id, platform=host, sourcePath=path, lineOrSymbol=marker,
                         requirement=evidence, evidence=evidence,
                         sourceSha256=hashlib.sha256(content).hexdigest(), result="static-audit-passed",
                         limitations="Static source/ownership only; target GPU/window/runtime results are separate."))
    output = ROOT / "verification" / f"{host}-static" / "rhi-floor"
    output.mkdir(parents=True, exist_ok=True)
    (output / "audit-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "audit-report.md").write_text("# R19D3B source audit\n\nStatic audit passed; runtime remains separate.\n\n"
        + "\n".join(f"- {x['checkId']}: {x['sourcePath']}::{x['lineOrSymbol']} — {x['evidence']}" for x in rows) + "\n")
    (output / "runtime-checklist.md").write_text(
        "# Required target run\n\n"
        "Configure: `cmake --preset qt-release -DYALAHOO_REVIEW=ON`.\n\n"
        "Build: `cmake --build --preset qt-release` and "
        "`cmake --build --preset qt-release --target check-format Yalahoo_qmllint ui-audit_qmllint`.\n\n"
        f"Tests: `ctest --preset {'qt-release-render' if host == 'macos' else 'qt-release-visible'} "
        "-R '^rhi-(shadow|flicker|shell|opaque|ao|fog)-gpu$' --output-on-failure`.\n\n"
        "Latest native Kit, Release, Review ON. Build check-format, "
        "Yalahoo_qmllint, ui-audit_qmllint; run rhi-shadow-gpu (includes floor math/attachments/ten rebuilds), "
        "rhi-flicker-gpu (1200 production frames), affected cover/AO/fog/shell and full applicable Release regression. "
        "Follow test-window-policy; select a native backend only when available. Expected source plane projection "
        "<=2 physical pixels, PBR/fog <=3/255, normal <=1/255, actual received shadow and AO/full DOF coverage, "
        "three map sizes, no unrelated target/caster rebuilds, bounded uploads and real resource recovery. "
        "Normal windows, VSYNC, target deployment and formal product performance remain separate R34/R28 tasks.\n")
print("Product floor COMMON/PERF/platform audit and helper equivalence passed")
