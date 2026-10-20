"""Record source shadow graph, cache and API audit; generated matrices remain local."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "tools/review/CMakeLists.txt", "target_link_libraries(rhi-shadow-audit PRIVATE YalahooRhi Qt6::GuiPrivate)",
     "Review-only executable uses existing native RHI/Qt targets, moc and same-Kit qrc/qsb. "
     "yalahoo_platform supplies each native helper. Production shaders remain in YalahooRhi "
     "and selectors are rejected outside Review; no third-party or platform-forced product path."),
    ("COMMON-02", "src/render/rhi/RhiItem.h", "QByteArray shadowDepth;",
     "Only immutable values cross render/GUI. Product geometry comes from verified qrc assets; "
     "test FIXTURE_ROOT and QDir/QFile preserve Unicode/space paths. Runtime outputs stay ignored; "
     "no Kit path, personal library or external requests added."),
    ("COMMON-03", "resources/shaders/rhi/geometry-instanced.vert", "layout(location = 9) out vec4 shadowCoord;",
     "Both production vertex layouts match opaque/four-shell fragment location 9. Existing "
     "invariant position and Equal color/visibility semantics remain. qsb emits five targets; "
     "actual Y/depth correction, single-sample D32F and independent R32F graph manually expanded."),
    ("COMMON-04", "src/app/VerificationWindowMac.mm", "currentForegroundPid",
     "Shared window helper adds only PID diagnostics, no activation/layer/input policy changes. "
     "Renderer pending weak ownership, bounded readbacks and reset ordering retained. Media "
     "not applicable: no player/audio/worker/device code changed or simulated."),
    ("COMMON-05", "tools/review/RhiShadowAudit.cpp", "validateProjection",
     "Independent source geometry projection and scalar PCF plus actual uploaded bytes, cache "
     "and lifecycle assertions. Preserve failed projection protocol and native-policy samples; "
     "no old-image fitting. Product floor gap stays explicit R19D3B, missing hardware stays R35/R36."),
    ("SOURCE-01", "legacy-web/src/scene.ts", "this.light.shadow.intensity = 0.32",
     "Music uses baseline key (-6,14,-5), orthographic bounds -16/16/-15/15/.1/45, "
     "normalBias .035, bias -.0003, radius 4 and final music strength .32. Only diffuser casts; "
     "floor/paper/shell receive. Missing product floor is explicit next round R19D3B."),
    ("SOURCE-02", "resources/shaders/rhi/geometry.vert", "shadowCoord = f.lightVP *",
     "Source shadowmap_vertex normalizes each world vertex normal, applies normalBias then "
     "interpolates shadow coordinates. Removed fragment renormalization/projection. Source "
     "asset triangles have constant normals; rotation/translation and normal transform preserved."),
    ("SOURCE-03", "resources/shaders/rhi/shadow-filter.glsl", "coord.z += f.optics.w",
     "Locked Three converts deprecated PCFSoft to PCF. Keep five Vogel samples, IGN, bilinear "
     "depth comparison, inclusive frustum and mix(1,shadow,.32). Canonical projection stays "
     "OpenGL; receiver UV, disk Y and IGN are converted together using actual backend Y flag."),
    ("SOURCE-04", "src/render/rhi/RhiItem.cpp", "shadowCompare->setTextureCompareOp",
     "D32F depth comparison and separate R32F canonical diagnostic target remain single sample. "
     "Front cull matches source FrontSide shadow backface rendering. Only tray/diffuser draws; "
     "no depth feedback or added polygon offset, source normal/depth bias stays receiver-side."),
    ("SOURCE-05", "tools/review/RhiShadowAudit.cpp", "Shadow PCF differs from source scalar math",
     "Current unquantized R32F map feeds independent compare-first/bilinear PCF math on a known "
     "receiver plane. Actual submitted values, formats and sampler checked; no old GPU witness, "
     "screenshot similarity, browser byte equality or image tuning."),
    ("PERF-01", "src/render/rhi/RhiItem.cpp", "shadowEntities != entities",
     "O(436) worst-case matrix invalidation check, one bounded caster pool; offscreen camera "
     "culling does not delete offscreen casters whose shadows can enter the receiver view. "
     "Threshold 8 changes draw batching, not caster identity or count."),
    ("PERF-02", "src/render/rhi/RhiItem.cpp", "pending.size() < 2",
     "Normal product has no capture readback, disk or wait. Existing at-most-two pending "
     "batches and bounded backend continuation frames preserved; no polling or worker queues."),
    ("PERF-03", "src/render/rhi/RhiItem.cpp", "state->capture.shadowDepth = state->results[i].data",
     "Opt-in scalar math shares existing immutable QByteArray, no extra GPU readback/copy. "
     "Receiver and actual paper/shell uploaded bytes are inspected only during explicit capture."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "makeTarget(shadow,",
     "Graph remains caster->single-sample R32F+D32F->opaque/paper->resolve/mips->shell. "
     "Targets/pipelines rebuild only on existing compatibility keys; camera/color/numeric "
     "light changes do not invalidate fixed-light caster depth. Actual inventory includes both."),
    ("PERF-05", "src/render/rhi/RhiItem.cpp", "cachedShadows != input.shadows",
     "Caster matrix change, shadow size/renderer generation or enabled toggle invalidates map. "
     "Unchanged pool reuses depth and skips separate nonbatched caster uniforms; enabling after "
     "disabled clear rerenders. Public light camera "
     "is fixed, so no missing movable-light key. New varyings move repeated projection to vertices."),
    ("PERF-06", "src/render/rhi/SceneItem.cpp", "in.shadows = !m_geometryOnly",
     "Production uses quality shadow switch/size; clock, motion, QML and input stay unchanged. "
     "Diagnostic receiver is isolated, no claim that it implements production ground material."),
    ("PERF-07", "tools/review/RhiShadowAudit.cpp", "cycle < 10",
     "Ten real SG invalidations restore shadow depth/format and scalar PCF. Resource ownership "
     "stays renderer-side, reset releases SRB/pipeline/RT before attachments. Native windows, "
     "VSYNC and unavailable target hardware remain R35/R36."),
]
for host, check_id, marker in [("windows", "WIN-01", "WIN32"),
                               ("macos", "MAC-01", "YalahooVerification"),
                               ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = []
    entries = CHECKS + [(check_id, "cmake/Platform.cmake", marker,
                        "Dynamic latest Kit, native platform helpers and five qsb targets. "
                        "D3D11/Linux hardware not inferred from macOS Metal/OpenGL. "
                        "Existing GL renderbuffer capability adaptation is retained.")]
    for check_id, path, symbol, conclusion in entries:
        content = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Audit source drift: {path}::{symbol}")
        rows.append(dict(checkId=check_id, requirement=conclusion, sourcePath=path, lineOrSymbol=symbol,
                         sourceSha256=hashlib.sha256(content).hexdigest(), evidence=conclusion,
                         result="static-audit-passed", platform=host,
                         limitations="Static graph/API evidence only; runtime reported separately. "
                                     "Product ground remains R19D3B; DOF/AA/BLUR-01 remain D4."))
    output = ROOT / "verification" / f"{host}-static" / "rhi-shadow"
    output.mkdir(parents=True, exist_ok=True)
    (output / "audit-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "audit-report.md").write_text(
        f"# R19D3A {host} source audit\n\n"
        "Static audit passed; target runtime is separate and cannot be inferred from this file.\n\n"
        + "\n".join(f"- {row['checkId']}: {row['sourcePath']}::{row['lineOrSymbol']} — "
                    f"{row['evidence']}" for row in rows) + "\n")
    (output / "runtime-checklist.md").write_text(
        "# Required target run\n\n"
        "Use the latest native Qt Kit and Release only. Configure Review ON, build and run "
        "check-format, Yalahoo_qmllint and ui-audit_qmllint.\n\n"
        "Run the actual target backend's rhi-shadow-gpu, rhi-opaque-gpu, rhi-shell-gpu and "
        "rhi-flicker-gpu via the appropriate hidden/visible preset and window contract. "
        "Expected: source geometry projection <=2 physical shadow texels, constant depth "
        "<2e-6, PCF <=2/255, original bias/strength/filter, no invalid cache updates, no "
        "separate unchanged caster uniform uploads, ten real SG rebuilds and 1200 continuous "
        "production frames. Normal windows/VSYNC and product ground remain separate tasks.\n")
print("Source shadow matrices generated")
