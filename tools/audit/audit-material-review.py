"""Bind the manually reviewed MaterialReview call chain to source hashes; output is local."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Guards detect drift, not correctness. These conclusions cover the review driver only.
CHECKS = [
    ("COMMON-01", "src/app/CMakeLists.txt", "OffscreenSurface.h",
     "Review-only Quick3D/GuiPrivate/qrc and executable registration use the selected native Kit; "
     "C++23/platform requirements remain shared. No new runtime dependency is introduced."),
    ("COMMON-02", "src/app/MaterialReview.cpp", 'component.loadFromModule',
     "GeometryScene, constant test ink, meshes and environments come from qrc. Committed "
     "source-state fixtures replace browser screenshots; no disk music, network or browser is read."),
    ("COMMON-03", "src/app/MaterialReview.cpp", 'actual.size() != window.size()',
     "Actual physical dimensions are checked against logical size times real target DPR. "
     "Independent Quick3D/CPU/source projections retain the two-logical-pixel bound."),
    ("COMMON-04", "src/app/main.cpp", 'reviewMaterials(app, scene',
     "Material CLI dispatch precedes ApplicationWindow loading. Public verification policy applies "
     "before the first frame. Exit codes are real process results, including Python/Windows waits."),
    ("COMMON-05", "src/app/MaterialReview.cpp", 'paperProbeError <= 3',
     "99 material/ink states and 48 lighting states keep parameter/projection/output assertions. "
     "Ink follows source sRGB decode, linear diffuse clamp and encode, independent of rendered images."),
    ("PERF-01", "src/app/MaterialReview.cpp", 'actual.constScanLine(y)',
     "A finite fixture runs serially. Each explicit capture scans its own physical target once, "
     "without Gaussian ROI scoring, edge search or pairwise image comparisons; no library traversal."),
    ("PERF-02", "src/app/MaterialReview.cpp", 'surface.connectFrames',
     "Normal frame-completion events advance four preparation frames per state. There is no polling, "
     "nested event pumping or unbounded capture queue. GPU wait/readback is explicit Review only."),
    ("PERF-03", "src/app/MaterialReview.cpp", 'surface.grab()',
     "One own-output readback per finite state is consumed synchronously and released. "
     "Only JSON/image outputs accumulate; production Input/SceneFrame/atlas paths are unchanged."),
    ("PERF-04-ATLAS", "src/app/MaterialReview.cpp", "std::make_unique<RenderProbeAtlas>()",
     "The 2048-square 16MiB probe atlas is created only for backendReview, never for material/light "
     "states; those reports explicitly record zero probe bytes/submissions. No new asset is uploaded."),
    ("PERF-04", "tools/review/OffscreenSurface.h", 'm_texture->pixelSize() != size',
     "Shared helper reuses RGBA8 single-sample color/depth targets until physical size changes; "
     "it reports attachment estimates separately from unknown driver memory. No product pass changes."),
    ("PERF-05", "src/app/MaterialReview.cpp", 'source-state-and-own-output',
     "No source BRDF, environment bytes, LOD or material effect is tuned by this driver change. "
     "Full optical equivalence remains R19C2B; consecutive production quality remains R19C2C/D."),
    ("PERF-06", "src/app/MaterialReview.cpp", 'applyState();',
     "The standalone GeometryScene has no ResearchApp frame clock/media/controls. State is applied "
     "once before waiting for frames. Default macOS operation is RenderControl without native show."),
    ("PERF-07", "src/app/MaterialReview.cpp", 'qmlRootRecreated',
     "Ten full QML/scene-graph rebuilds for material and one for lighting reapply the same source "
     "state and validate output and real invalidation. Same-View3D resume is not claimed. "
     "Root/object/engine die before the owning surface/window/control; invalidation counter is atomic."),
]
for host, platform_guard in {
    "windows": ("WIN-01", "cmake/Platform.cmake", "WIN32"),
    "macos": ("MAC-01", "cmake/Platform.cmake", "YalahooVerification"),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX AND NOT APPLE"),
}.items():
    rows = []
    check_id, path, symbol = platform_guard
    entries = CHECKS + [(check_id, path, symbol,
                        "Native platform build/window/GPU branches remain real implementations; "
                        "macOS uses the existing true hidden Metal helper. Windows/D3D11 and "
                        "Linux/OpenGL target execution remains R34, not proved by macOS.")]
    for check_id, path, symbol, conclusion in entries:
        content = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Source audit drift: {path}::{symbol}")
        rows.append(dict(checkId=check_id, sourcePath=path, lineOrSymbol=symbol,
                         sourceSha256=hashlib.sha256(content).hexdigest(), evidence=conclusion,
                         result="static-audit-passed", platform=host,
                         limitations="Review driver only; actual runs documented separately. "
                                     "No VSYNC, production optical or target-device conclusion."))
    output = ROOT / "verification" / f"{host}-static" / "material-review"
    output.mkdir(parents=True, exist_ok=True)
    (output / "audit-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
print("Material Review source-hash matrices generated for Windows, macOS and Linux")
