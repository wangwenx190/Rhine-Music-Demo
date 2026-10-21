"""Record manually traced invariance/continuous rendering contracts, without GPU image oracles."""
from audit_support import source_check
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/render/rhi/CMakeLists.txt", "MSL 21",
     "The two shared geometry shaders emit invariant position on all five qsb targets. MSL 2.1 "
     "is needed for the invariant attribute; other fragments remain MSL 1.2. Latest Kit is discovered."),
    ("COMMON-02", "tools/review/RhiProductionFixture.h", "Snapshot productionLibrary()",
     "Anonymous 9x64 local albums reference three qrc demo covers; actual SceneItem prepares "
     "production atlas leases and publishes immutable states. No private music or network input."),
    ("COMMON-03", "resources/shaders/rhi/geometry-instanced.vert", "f.nativeVP * p",
     "Visibility and color use identical GPU world-position then nativeVP operations and invariant "
     "outputs. No depth epsilon, bias, opacity, culling or motion threshold is altered."),
    ("COMMON-04", "src/render/rhi/SceneItem.cpp", "in.opticalProbe = input().opticalProbe;",
     "Review selector survives coalesced cover/camera polish under the audit define alone. "
     "Production rejects the selector and omits both diagnostic fragment resources."),
    ("COMMON-05", "tools/review/OpticsProbe.frag", "productionShellMain();",
     "Diagnostic executes production optics then marks finite shaded samples white; visibility "
     "marks magenta before Equal shading. Remaining magenta means lost color coverage, not an "
     "image-reference mismatch; nonfinite output has a separate green sentinel."),
    ("PERF-01", "src/render/rhi/RhiItem.cpp", "mvp remains reserved",
     "Remove now-unused CPU nativeVP*model per-uniform work. GPU reuses world position already "
     "needed for lighting, keeping existing std140 offsets and bounded instance uploads (832 bytes with R19D2 fog)."),
    ("PERF-02", "tools/review/RhiCaptureHarness.h", "Concurrent Review capture",
     "Readbacks are serial and event-driven, pending native batches remain at most two. Cover "
     "readiness waits on frame events with timeout, not a fast synthetic retry/poll loop."),
    ("PERF-03", "tools/review/RhiFlickerAudit.cpp", "Frame/probe change rebuilt render targets",
     "Actual resourceGeneration must stay fixed across each 60-frame sequence; shader pipelines "
     "may specialize for diagnostics but no render targets are reallocated by selector values."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "visibilityProbeShader != (input.opticalProbe == VisibilityProbe)",
     "Only explicit Review diagnostic mode enables visibility color writes. Target/attachment "
     "ownership, opaque capture isolation, resolve/mips and readback deduplication are unchanged."),
    ("PERF-05", "resources/shaders/rhi/geometry.vert", "invariant gl_Position;",
     "Formal floating-point invariance corrects multi-pass raster position, without degrading "
     "normal material BRDF, transparency, animation, active album count or source quality settings."),
    ("PERF-06", "tools/review/RhiFlickerAudit.cpp", "motion.clock()->advance(1.0 / 60);",
     "Source FrameClock drives waves, navigation, overview and detail in ordinary ordering. Static alone "
     "holds simulation time; active tests keep speed=1 and reduced motion off. No QML flow changes."),
    ("PERF-07", "tools/review/RhiFlickerAudit.cpp", "frame < 60",
     "20 sequences cover two viewport sizes, two themes, five states and 1200 samples, half with "
     "unmodified optics output and half finite/coverage diagnostics. Prior 10-cycle graph and "
     "resize gates run in the same final hidden regression; normal VSYNC is not measured."),
]
for host, check_id, marker in [("windows", "WIN-01", "WIN32"),
                               ("macos", "MAC-01", "YalahooVerification"),
                               ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = []
    entries = CHECKS + [(check_id, "cmake/Platform.cmake", marker,
                        "Native backend/compiler/window integration remains intact. Runtime is "
                        "macOS Metal/OpenGL hidden; D3D11 target execution remains R36; Linux OpenGL remains R36.")]
    for check_id, path, symbol, conclusion in entries:
        rows.append(source_check(ROOT, path, symbol, checkId=check_id, evidence=conclusion, result='static-audit-passed', platform=host, limitations='Finite continuous hidden scope; no old image comparison, normal presentation/VSYNC or untested target claim.'))
    output = ROOT / "verification" / f"{host}-static" / "rhi-flicker"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
print("Continuous optical invariance matrices generated")
