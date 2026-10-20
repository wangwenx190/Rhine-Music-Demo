"""Record reviewed source fog/color edges by symbol and hash; output remains local."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "legacy-web/src/scene.ts", "fog.near = renderedDistance",
     "Production uses actual camera-to-aim distance and lerp(5,-1,detail)/lerp(25,12,detail), "
     "not requested target distance or construction defaults. Theme animates fog/background together."),
    ("COMMON-02", "legacy-web/src/music-model.ts", "toneMapped: false",
     "Album Lambert print is fog-enabled. Locked Three 0.183.2 disables material tone mapping "
     "on ordinary render targets; both transmission and composer use linear working colors. "
     "OutputPass still applies final ACES/sRGB to paper and glass together."),
    ("COMMON-03", "resources/shaders/rhi/fog.glsl", "smoothstep(f.fog.x, f.fog.y, viewDepth)",
     "Exact source smoothstep on negative view Z, linear background equals authored fog color. "
     "Paper shades then fogs before capture resolve/mips; shells sample that capture, shade, "
     "then fog again at their own depth. No fog in normal/depth or background passes."),
    ("COMMON-04", "src/render/rhi/RhiItem.cpp", "submittedFogUniforms",
     "Explicit captures inspect the actual uploaded draw bytes for paper and all visible shells. "
     "Output options come from the same array passed to updateDynamicBuffer; GUI receives values."),
    ("COMMON-05", "tools/review/RhiFogAudit.cpp", "Fog/color order differs from source math",
     "84 GPU states execute production fog/output helpers with analytic RGB, seven source depth "
     "fractions, enable/disable and two theme colors/exposures; expected values are independent "
     "scalar formulas, not source screenshots or browser GPU bytes."),
    ("PERF-01", "src/render/SceneAdapter.cpp", "const double renderedDistance",
     "One O(1) camera distance and two ranges per publish. No library scan or per-entity camera math; "
     "existing visible bound 432+4 and sort keys unchanged."),
    ("PERF-02", "src/render/rhi/RhiItem.h", "bool fogEnabled = false",
     "Immutable value snapshot, no worker, queue, disk, synchronization wait or new product readback. "
     "Review uses normal event completion, at most two existing pending batches."),
    ("PERF-03", "src/render/rhi/RhiItem.cpp", "offsetof(Uniform, fog) == 816",
     "Append one std140 vec4: 832 bytes. Previous offsets remain; upload uses existing aligned "
     "written prefix and bounded uniform capacity. Actual stride depends on backend alignment."),
    ("PERF-04", "resources/shaders/rhi/opaque.frag", "sceneFog(outgoing, viewDepth)",
     "No new pass/attachment/SRB/texture/PSO. Opaque RGBA16F resolve+mips precede separate optical "
     "composite; postprocess precedes one ACES and exact sRGB. Fog values are not resource keys."),
    ("PERF-05", "src/render/SceneAdapter.cpp", "m_frame.fogNear == fogNear",
     "Detail-only changes invalidate scene publication even when camera pose is unchanged. "
     "View depth is already computed by both invariant production geometry variants; reused directly."),
    ("PERF-06", "src/render/rhi/SceneItem.cpp", "in.fogEnabled = !m_geometryOnly",
     "All production music states enable authored fog; geometry-only inspection stays diagnostic. "
     "No QML binding, input, clock, motion or default quality changes."),
    ("PERF-07", "tools/review/RhiFogAudit.cpp", "cycle < 10",
     "Real hidden invalidation/rebuild checks restore fog uniforms and valid geometry ten times. "
     "No new owned resource to leak. Native window, VSYNC and unavailable target hardware remain R36."),
]
for host, check_id, marker in [("windows", "WIN-01", "WIN32"),
                               ("macos", "MAC-01", "YalahooVerification"),
                               ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = []
    entries = CHECKS + [(check_id, "cmake/Platform.cmake", marker,
                        "Qt Kit is selected dynamically. Shared std140/qsb API adapters preserve "
                        "color semantics; five shader target translations are static evidence only. "
                        "D3D11/Linux hardware execution and normal presentation stay R36.")]
    for check_id, path, symbol, conclusion in entries:
        content = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Audit source drift: {path}::{symbol}")
        rows.append(dict(checkId=check_id, sourcePath=path, lineOrSymbol=symbol,
                         sourceSha256=hashlib.sha256(content).hexdigest(), evidence=conclusion,
                         result="static-audit-passed", platform=host,
                         limitations="Source/graph audit only; actual runtime reported separately. "
                                     "No image fitting, no BLUR-01/DOF/AA/shadow/presentation closure."))
    output = ROOT / "verification" / f"{host}-static" / "rhi-fog"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
print("Source fog/color matrices generated")
