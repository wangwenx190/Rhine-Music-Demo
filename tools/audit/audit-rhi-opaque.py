"""Source-hash guards for the manually reviewed opaque RHI boundary; outputs stay local."""
from audit_support import source_check
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "tools/review/CMakeLists.txt", "yalahoo_rhi_review(rhi-audit",
     "Opaque Review uses the same YalahooRhi/Qt native Kit. GuiPrivate is selected by the existing capability probe."),
    ("COMMON-02", "tools/review/RhiAudit.cpp", "Input fixture",
     "Only committed demo artwork and source-derived scene values enter the driver. Models, "
     "environment/lookups and qsb stay qrc resources. No browser PNG, user music or network input."),
    ("COMMON-03", "resources/shaders/rhi/geometry.vert", "transpose(inverse(mat3(model)))",
     "Source GLB positions/UVs are retained; normal inverse transpose and QRhi clip correction "
     "are necessary adaptations. View normal/linear depth tests use independent plane math."),
    ("COMMON-04", "tools/review/RhiCaptureHarness.h", "state->loop = nullptr;",
     "Only guarded value results reach GUI; failed render/timeout clears the local loop pointer. "
     "Window/control/state destruction order and atomic scene-graph invalidation retain ownership."),
    ("COMMON-05", "tools/review/RhiAudit.cpp", "source-math-and-own-output",
     "36 theme/view/cover/channel captures preserve known plane depth 2e-6, normal code precision "
     "and two-pixel authored-corner projection checks. No GPU image oracle/score is used."),
    ("PERF-01", "tools/review/RhiCaptureHarness.h", "class RhiCaptureHarness",
     "Finite serial fixture work is independent of library size; Review alone scans diagnostic "
     "targets. Production remains at most 432+4 entities. No new scene traversal is introduced."),
    ("PERF-02", "tools/review/RhiCaptureHarness.h", "Concurrent Review capture",
     "One requested capture waits on its readback completion in a normal event loop, never a "
     "processEvents/poll loop. Thirty resize states publish once per real frame; renderer stays "
     "at most two pending batches with a backend-derived finite drain continuation."),
    ("PERF-03", "src/render/rhi/RhiItem.cpp", "*outputs[field] = image;",
     "Each distinct texture is read and converted once per explicit capture. Aliased semantic "
     "fields share immutable QImage storage; upload-byte/layout/format behavior is unchanged."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "textures[prior] == textures[i]",
     "Pointer identity deduplicates real readback resources before command submission: no-shell "
     "diagnostics with captureAtlas=false use four unique textures for twelve field slots. "
     "Actual resource descriptor inventory is attached only to captures; driver memory stays null."),
    ("PERF-05", "resources/shaders/rhi/opaque.frag", "min(source * max(irradiance, vec3(0)), source)",
     "Source Lambert print clamp, field exposure, alpha .025, contain/inset and exact sRGB decode "
     "remain. PMREM diffuse helper is the same shared source adaptation; no ROI scale or tone fit."),
    ("PERF-05-GEN", "tools/assets/prepare-rhi-paper.py", "entityLight0.y",
     "The current paper generator preserves explicit uniform adaptation and per-entity hoisting. "
     "Its token check compares the whole generated helper with the shared source math; default "
     "generation plus Qt formatting must reproduce the checked-in shader exactly."),
    ("PERF-06", "tools/review/RhiCaptureHarness.h", "prepare(frames);",
     "The driver has no QML oracle or extra frame animation. State is prepared once per completed "
     "frame; pending readback retirement comes from the renderer, not repeated GUI submit calls."),
    ("PERF-07", "tools/review/RhiAudit.cpp", "cycle < 10",
     "Twelve ordered sizes, thirty frame-driven target replacements and ten actual graph rebuilds "
     "test stale dimensions/callbacks. Same capture request still permits a cover revision. "
     "Hidden resource invalidation does not prove native window restoration or VSYNC."),
]
for host, check_id, marker in [("windows", "WIN-01", "WIN32"),
                               ("macos", "MAC-01", "YalahooVerification"),
                               ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = []
    entries = CHECKS + [(check_id, "cmake/Platform.cmake", marker,
                        "Actual native build/backend/window branches remain separate. This host "
                        "runs macOS Metal RenderControl; Windows/D3D11 and Linux/OpenGL runtime "
                        "remain R36 when their compiler/device/window-manager environment is absent.")]
    for check_id, path, symbol, conclusion in entries:
        rows.append(source_check(ROOT, path, symbol, checkId=check_id, evidence=conclusion, result='static-audit-passed', platform=host, limitations='Opaque driver/paper scope only; full optical source and continuous flicker remain R19C2B2/C/D. Runtime is separate.'))
    output = ROOT / "verification" / f"{host}-static" / "rhi-opaque"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
print("Opaque RHI source-hash matrices generated")
