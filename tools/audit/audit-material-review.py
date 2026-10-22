"""Bind the manually reviewed MaterialReview call chain to source hashes; output is local."""
from audit_support import source_rows, write_source_report
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Guards detect drift, not correctness. These conclusions cover the review driver only.
CHECKS = [
    ("COMMON-01", "src/app/CMakeLists.txt", "OffscreenSurface.h",
     "Review-only Quick3D/Quick3DPrivate/RuntimeRenderPrivate/GuiPrivate/qrc use the selected native Kit and a real configure-time API probe; "
     "C++23/platform requirements remain shared. No new runtime dependency is introduced."),
    ("COMMON-02", "src/app/MaterialReview.cpp", 'component.loadFromModule',
     "GeometryScene, constant test ink, meshes and environments come from qrc. Committed "
     "source-state fixtures replace browser screenshots; no disk music, network or browser is read."),
    ("COMMON-03", "src/app/MaterialReview.cpp", 'actual.size() != state.window->size()',
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
    ("PERF-02", "src/app/MaterialReview.cpp", 'surface->connectFrames',
     "Normal frame-completion events advance four preparation frames per state. There is no polling, "
     "nested event pumping or unbounded capture queue. GPU wait/readback is explicit Review only."),
    ("PERF-03", "src/app/MaterialReview.cpp", 'surface->grab()',
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
     "The old QML root is released before native window destruction; the surface is hidden/invalidated first, old frames are epoch-rejected and descriptors cross threads as locked values. The invalidation counter is atomic."),
]
for host, platform_guard in {
    "windows": ("WIN-01", "cmake/Platform.cmake", "WIN32"),
    "macos": ("MAC-01", "cmake/Platform.cmake", "YalahooVerification"),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX AND NOT APPLE"),
}.items():
    check_id, path, symbol = platform_guard
    entries = CHECKS + [(check_id, path, symbol,
                        "Native platform build/window/GPU branches remain real implementations; "
                        "macOS uses the true hidden Metal/OpenGL helper; missing Cocoa multisample-texture storage selects the stock same-format 4x buffer/resolve path. Windows/D3D11 and "
                        "Linux/OpenGL target execution remains R36, not proved by macOS.")]
    rows = source_rows(ROOT, entries,
                       result='static-audit-passed', platform=host, limitations='Review driver only; actual runs documented separately. No VSYNC, production optical or target-device conclusion.')
    write_source_report(ROOT, host, 'material-review', rows, 'audit-matrix.json')
print("Material Review source-hash matrices generated for Windows, macOS and Linux")
