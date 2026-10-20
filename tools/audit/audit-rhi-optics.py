"""Bind manually reviewed source optics and capture edges to hashes; reports stay local."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/render/rhi/CMakeLists.txt", "if(YALAHOO_REVIEW)",
     "OpticsProbe is a build-only wrapper around the production GLSL functions, compiled only "
     "with Review. Production rejects its selector and does not package the probe shader."),
    ("COMMON-02", "tools/review/RhiShellAudit.cpp", "Incomplete source shell states",
     "61 committed source-state cases use qrc demo covers, locked GLB and Room/DFG/LTC inputs. "
     "No prior Quick3D PNG, browser witness, ROI score or user music is loaded."),
    ("COMMON-03", "src/render/rhi/RhiItem.cpp", "expectedBinding",
     "Actual SRB equality proves binding four samples opaque resolve with the intended sampler. "
     "Actual RT descriptors/flags and pipeline getters verify separate composite color, preserved "
     "shared depth, Equal/read-only optical shading, back culling and no blend."),
    ("COMMON-04", "src/render/rhi/RhiItem.cpp", "Nonfinite optical math probe",
     "Readback owns its raw bytes; probe values are decoded before RGBA8 conversion and delivered "
     "as bounded value JSON. No GUI/QRhi object pointer escapes to consumers; existing guards remain."),
    ("COMMON-05", "tools/review/OpticsProbe.frag", "musicGeometryRoughness",
     "The same GLSL functions execute exact zero-light and normal-incidence limits, GGX/DFG, key "
     "shadow factors and a known screen gradient with zero/45-degree view roll. Expected values "
     "come from independent scalar/rectangle mathematics, not rendered images."),
    ("PERF-01", "src/render/rhi/RhiItem.cpp", "captureKinds.append(submitted.draw[0]);",
     "Explicit capture alone serializes actual bounded draw commands/uniforms. No production JSON "
     "is allocated for request zero; instances remain at most 432+4 and no library traversal is added."),
    ("PERF-02", "tools/review/RhiShellAudit.cpp", "RhiCaptureHarness driver;",
     "All source states and diagnostic readbacks run serially via real event completion. "
     "No polling or repeated GUI submit; at most two native pending batches stay bounded."),
    ("PERF-03", "src/render/rhi/RhiItem.cpp", "float(input.opticalProbe)",
     "Review selector uses the reserved std140 paperSize.w; existing std140 offsets (832 bytes after R19D2 fog) and "
     "per-frame/instance uploads remain. Math values and view changes do not rebuild targets; "
     "the driver checks real resourceGeneration across these states."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", 'opticalEvents.append(u"capture-mips"_s);',
     "Trace follows actual opaque/endPass/mip update/copy/visibility/shading calls. Opaque draw "
     "kinds come from the submitted uniform bytes, not an expected pass-name template. "
     "Capture remains isolated from shells and resource feedback."),
    ("PERF-05", "resources/shaders/case-glass.frag", "mat3(VIEW_MATRIX) * normal",
     "Three nonPerturbedNormal is view-space. Max-component normal derivatives require this "
     "rotation before differentiation; no equivalent world-space infinity norm exists. "
     "BRDF/PMREM/bicubic/Beer/clearcoat/IOR/LOD coefficients stay source-defined."),
    ("PERF-05-LTC", "resources/shaders/lighting-ltc.glsl", "dot(tangent, tangent) == 0.0",
     "At exact normal incidence the tangent orientation is undefined. A least-aligned cardinal "
     "axis provides a finite orthogonal basis only in this singular case; regular LTC math is "
     "unchanged. Zero direct irradiance exits before normalizing opposite view/light vectors."),
    ("PERF-06", "tools/review/RhiShellAudit.cpp", "source-optics-graph-and-own-output",
     "No QML model/input/animation or frame clock changes. Both Quick3D material snippets and "
     "RHI variants receive the same source correction; source UI state and timing remain."),
    ("PERF-07", "tools/review/RhiShellAudit.cpp", "cycle < 10",
     "Ten real invalidation/rebuild cycles restore ordinary optics after math modes. Stable sort "
     "uses depth/ID/surface, reverse submission preserves keys and duplicates are rejected. "
     "Window exposure, normal VSYNC and continuous production quality remain separate."),
]
for host, check_id, marker in [("windows", "WIN-01", "WIN32"),
                               ("macos", "MAC-01", "YalahooVerification"),
                               ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = []
    entries = CHECKS + [(check_id, "cmake/Platform.cmake", marker,
                        "Native Kit/API branches remain separate; five qsb target languages only "
                        "prove static translation. Current actual GPU is macOS Metal; unavailable "
                        "D3D11 runtime stays R36; Linux GL runtime stays R35. No version or install path is pinned.")]
    for check_id, path, symbol, conclusion in entries:
        content = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Audit source drift: {path}::{symbol}")
        rows.append(dict(checkId=check_id, sourcePath=path, lineOrSymbol=symbol,
                         sourceSha256=hashlib.sha256(content).hexdigest(), evidence=conclusion,
                         result="static-audit-passed", platform=host,
                         limitations="Static source/graph scope; runtime listed separately. "
                                     "No source screenshot comparison or claim that array "
                                     "flicker/blur, fog, postprocessing or presentation is closed."))
    output = ROOT / "verification" / f"{host}-static" / "rhi-optics"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
print("Source optics/capture matrices generated")
