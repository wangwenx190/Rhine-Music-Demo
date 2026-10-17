"""Audit Bokeh source, resource and platform edges; write only local matrices."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "legacy-web/src/scene.ts", "bokehUniforms.focus.value = -focalPoint.z",
     "Frozen frame updates focus at entity+(0,2,0) and aperture=lerp(.0003,.0008,detail)*strength/100; construction .0018 is not the steady frame value."),
    ("COMMON-02", "src/render/SceneAdapter.cpp", "origin + QVector3D(0, 2, 0)",
     "Rendered view matrix supplies signed optical depth, including translated/pitched camera. No second camera or damping clock."),
    ("COMMON-03", "resources/shaders/rhi/bokeh.frag", "vec3(65536.0, 256.0, 1.0) / 65793.0",
     "RGB24 normalized channels decode linear view depth; nearest depth, linear color and clamp-to-edge. Independent locked 41 source taps retain all offsets/rings/order, aspect, signed CoC and maxblur=.011."),
    ("COMMON-04", "tools/review/RhiBokehAudit.cpp", "Test every output pixel",
     "Real QRhi offscreen Postprocess runs analytic RGBA16F HDR and RGB24 input, independent double bilinear/source sum. Focus, front/back, zero strength, both clamp signs, boundaries and non-square portrait/landscape are covered; no browser GPU witness."),
    ("COMMON-05", "tools/review/RhiFlickerAudit.cpp", "Actual production Bokeh uniform/depth differs from frozen source",
     "1200-frame SceneItem playback checks actual uploaded prefix and bound full-size DOF depth against source focal point/aperture/near/far/aspect. Isolated synthetic samples alone cannot prove scene integration."),
    ("PERF-01", "resources/shaders/rhi/bokeh.frag", "FRAGCOLOR = col / 41.0",
     "Fixed 41 color taps plus one depth read per target pixel, O(target pixels), independent of library size; geometry retains bounded 432+4 pool."),
    ("PERF-02", "src/render/rhi/Postprocess.cpp", "updates->updateDynamicBuffer(pass.uniform.get(), 0, sizeof(u), &u)",
     "Render owns resources; preloaded immutable assets, no I/O, worker waits or production readback. Explicit diagnostic uses endOffscreenFrame only, not a product event loop or presentation benchmark."),
    ("PERF-03", "src/render/rhi/Postprocess.cpp", "sizeof(PostPrefix) == 80",
     "Each active Bokeh pass uploads only its actual 80-byte dynamic prefix; unused kernel capacity not uploaded. Actual prefix saved only in Review build; request captures return values."),
    ("PERF-04", "src/render/rhi/Postprocess.cpp", 'p.add(u"bokeh"_s, p.ping[ping], current, color)',
     "AO composite -> distinct RGBA16F Bokeh output -> optional SMAA -> one ACES/sRGB. Full geometry RGB24 DOF depth shares AO only at full resolution. No framebuffer feedback; one Bokeh-only color target and one uniform."),
    ("PERF-05", "src/render/rhi/Postprocess.cpp", "m_state->settings = s",
     "Focus/aperture/near/far are uniform values outside compatibility keys; changed optical values reuse textures/PSO/SRB. All 41 authored samples preserved; no approximate replacement renderer or sharpening."),
    ("PERF-06", "src/render/rhi/SceneItem.cpp", "in.aperture = float((.0003 + .0005 * adapter.detail())",
     "Existing polish publishes current scene and quality values. No new QML clock, bindings, library scan, input path or production readback."),
    ("PERF-07", "tools/review/RhiBokehAudit.cpp", "cycle < 10",
     "Ten real Postprocess reset/recreate cycles, alternate clip range and valid output, plus disable releases all owned targets. This is postprocess resource lifecycle, not native window or scene-graph restoration; those remain their original gates/R34."),
]

mapping = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]
baseline = json.loads((ROOT / "resources/manifests/postprocess.json").read_text())
scene = subprocess.check_output([
    "git", "show", mapping[baseline["baseline"]] + ":src/scene.ts"
], cwd=ROOT)
if hashlib.sha256(scene).hexdigest() != baseline["sources"]["src/scene.ts"]:
    raise SystemExit("Frozen scene source differs from provenance")
# Trace actual historical algorithm text; formatting and wrapper changes do not alter source math.
history = []
for revision, path in [("522a3f3", "resources/shaders/bokeh.frag"),
                       ("1f2352a", "resources/shaders/rhi/bokeh.frag"),
                       ("75775b3", "resources/shaders/rhi/bokeh.frag"),
                       ("e98c798", "resources/shaders/rhi/bokeh.frag")]:
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    text = re.sub(r"\s+", "", data.decode())
    current = re.sub(r"\s+", "", (ROOT / path).read_text())
    historical_start = text.index("voidMAIN()") if "voidMAIN()" in text else text.index("voidmain()")
    current_start = current.index("voidMAIN()") if "voidMAIN()" in current else current.index("voidmain()")
    if text[historical_start:] != current[current_start:]:
        raise SystemExit(f"Unreviewed historical Bokeh algorithm drift: {revision}")
    history.append(dict(revision=revision, path=path, sha256=hashlib.sha256(data).hexdigest(),
                        conclusion="Bokeh body unchanged after whitespace normalization; adapters, allocations and actual production bindings audited separately."))
for host, check_id, marker in [("windows", "WIN-01", "QRhiD3D11InitParams"),
                               ("macos", "MAC-01", "QRhiMetalInitParams"),
                               ("linux", "LNX-01", "QRhiGles2InitParams")]:
    rows = []
    for identity, path, symbol, evidence in CHECKS + [
            (check_id, "tools/review/RhiBokehAudit.cpp", marker,
             "Native backend selected without a window/swapchain. GL fallback surface lifetime exceeds QRhi. Shared bindings, output Y handling and RGBA16F tested per actual backend; cross-target qsb is only static translation.")]:
        data = (ROOT / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(data.decode().split()):
            raise SystemExit(f"Source audit drift: {path}::{symbol}")
        rows.append(dict(checkId=identity, sourcePath=path, lineOrSymbol=symbol,
                         sourceSha256=hashlib.sha256(data).hexdigest(), evidence=evidence,
                         platform=host, result="static-audit-passed",
                         limitations="Actual backend execution reported separately. No SMAA/scale/BLUR-01/window/VSYNC/performance closure."))
    output = ROOT / "verification" / f"{host}-static" / "rhi-bokeh"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=rows, history=history), indent=2) + "\n")
print("Bokeh historical and COMMON/PERF/platform source matrices generated")
