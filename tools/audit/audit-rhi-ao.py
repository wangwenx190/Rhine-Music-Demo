"""Hash-bound manual SSAO/readback review; output is local, not release evidence."""
from audit_support import source_rows, write_source_report
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "src/render/rhi/RhiItem.cpp", "glGetIntegerv(GL_PACK_ALIGNMENT",
     "Query the current native GL packing once per renderer only during explicit capture. No "
     "Qt version/path or global GL state is changed. Metal/D3D retain their existing direct path."),
    ("COMMON-02", "resources/shaders/ssao.frag", "vec3 normal = geometry.rgb * 2.0 - 1.0;",
     "Locked Three SSAOShader unpacks the stored normal without a second normalization. Source "
     "radius .18 and normalized-depth min/max .001/.035, scalar noise, strict sample comparison "
     "and 16/32/64 full accumulation remain; no image fitting or extra blur is introduced."),
    ("COMMON-03", "resources/shaders/ssao.frag", "all(equal(depthCode, vec3(1)))",
     "Source background depth==1 is tested exactly in RGB24 code space, replacing the earlier "
     "1e-6 expansion. Background avoids the unnecessary normal fetch. Standard perspective "
     "view reconstruction and actual framebuffer row transforms retain source semantics."),
    ("COMMON-04", "src/render/rhi/RhiItem.cpp", "logicalReadbackSizes",
     "The padded scalar texture stays render-owned; pending results keep original logical size "
     "and decode actual storage stride before QImage value handoff. Width alone is padded, no "
     "uninitialized padding component is consumed. Existing QPointer/backpressure/drain remain."),
    ("COMMON-05", "tools/review/RhiAoAudit.cpp", "Source SSAO numerical sample differs",
     "27 background/count/resolution/odd-width cases, independent source SSAO/5x5/.22 numerical "
     "points with contact coverage, unchanged formats, repeated cache reuse and 10 real SG "
     "rebuilds use current primitive inputs, not previous GPU images or ROI scores."),
    ("PERF-01", "resources/shaders/ssao.frag", "vec4 geometry = texture(geometryMap, uv);",
     "Fetch normal only after exact background exclusion. No new library traversal or GPU "
     "production readback; source math remains finite bounded work per chosen sample count."),
    ("PERF-02", "src/render/rhi/RhiItem.cpp", "pending.size() < 2",
     "At most two explicit pending captures retain value results. Readback owners still "
     "deduplicate original texture identities; GL copy/read commands follow source pass completion."),
    ("PERF-03", "src/render/rhi/Postprocess.cpp", "PostPrefix u",
     "Steady updates send only the 80-byte changing prefix per pass. The source kernel uploads "
     "once to its SSAO uniform buffer and survives partial updates; unused per-frame 1024-byte "
     "copies and redundant GPU kernel texture are removed. Uniform block ABI stays 1104 bytes."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "alignedReadbacks[i]",
     "Only unaligned GL R16F captures copy unchanged values into a cached padded-width R16F "
     "texture. Same shape reuses resources; actual inventory includes the bounded two AO slots. "
     "Product target dimensions, sampling, precision and shaders do not use that padding."),
    ("PERF-05", "tools/review/RhiAoAudit.cpp", "Source 5x5 AO blur differs",
     "81 fixed points for each kernel execute independent source projection/kernel/TBN/depth "
     "math, 5x5 blur and .22 composite. Limits remain 1/255 raw/blur, 2/255 composite with "
     "two RGBA8 diagnostic conversions, without deleting difficult points or source images."),
    ("PERF-06", "src/render/rhi/Postprocess.cpp", "p.uniformUploadBytes += sizeof(u);",
     "Actual submitted byte counts distinguish first kernel upload from steady prefix updates. "
     "No UI clock, geometry, navigation, reduced-motion, theme or media behavior changes."),
    ("PERF-07", "tools/review/RhiAoAudit.cpp", "cycle < 10",
     "Real graph invalidation recreates alignment resources and restores valid output. Current "
     "Metal/GL hidden continuous production runs are separate from normal VSYNC/presentation; "
     "target D3D/Linux GL runtime and remaining D2-D4 effects are not declared complete."),
]
for host, check_id, marker in [("windows", "WIN-01", "WIN32"),
                               ("macos", "MAC-01", "YalahooVerification"),
                               ("linux", "LNX-01", "UNIX AND NOT APPLE")]:
    rows = source_rows(ROOT, CHECKS + [(check_id, "cmake/Platform.cmake", marker,
            "Native Kit/compiler/API branches remain. Real target Windows/D3D11 and Linux/GL "
            "environments are absent on this host; exact target matrix is retained for R36.")],
                       result='static-audit-passed', platform=host, limitations='Source/algorithm scope only; actual GPU runs are separate. No screenshot comparison, VRAM or normal VSYNC claim.')
    write_source_report(ROOT, host, 'rhi-ao', rows)
print("Source SSAO/readback/platform matrices generated")
