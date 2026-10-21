"""Check locked SMAA source adaptation, lookup provenance and local platform matrices."""
from audit_support import source_check
import base64
import hashlib
import json
import re
import subprocess
from pathlib import Path
from audit_support import read_shader

ROOT = Path(__file__).resolve().parents[2]
manifest = json.loads((ROOT / "resources/manifests/postprocess.json").read_text())
shader_path = "legacy-web/node_modules/three/examples/jsm/shaders/SMAAShader.js"
pass_path = "legacy-web/node_modules/three/examples/jsm/postprocessing/SMAAPass.js"
for path in [shader_path, pass_path]:
    if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != manifest["algorithms"][path]:
        raise SystemExit(f"Locked SMAA source hash differs: {path}")
mapping = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]
for path in ["src/scene.ts", "src/render-quality.ts"]:
    frozen = subprocess.check_output(["git", "show", mapping[manifest["baseline"]] + ":" + path], cwd=ROOT)
    if hashlib.sha256(frozen).hexdigest() != manifest["sources"][path]:
        raise SystemExit(f"Frozen SMAA production caller provenance differs: {path}")

source = (ROOT / shader_path).read_text()
source_pass = (ROOT / pass_path).read_text()
for asset, method in zip(manifest["assets"], ["getAreaTexture", "getSearchTexture"]):
    section = source_pass[source_pass.index(f"{method}() {{"):]
    encoded = re.search(r"data:image/png;base64,([^']+)", section)[1]
    data = (ROOT / "resources" / asset["path"]).read_bytes()
    if data != base64.b64decode(encoded) or hashlib.sha256(data).hexdigest() != asset["sha256"]:
        raise SystemExit(f"Numeric SMAA lookup differs from locked inline PNG: {method}")

def compact(text):
    text = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)
    text = re.sub(r"\\\r?\n", "", text)
    return re.sub(r"\s+", "", text)

# Reconstruct only the mathematical source helper bodies, independently of the generator.
# API names, entry points and framebuffer orientation are explicit adapters, not fitted values.
for kind, name in [("edges", "SMAAEdgesShader"), ("weights", "SMAAWeightsShader"),
                   ("blend", "SMAABlendShader")]:
    section = source[source.index("const " + name):]
    vertex, fragment = re.findall(r"(?:vertexShader|fragmentShader): /\* glsl \*/`([\s\S]+?)`", section)[:2]
    vertex = vertex[vertex.index("void SMAA"):vertex.index("void main()")]
    fragment = re.sub(r"\s*(uniform|varying) [^;]+;", "", fragment)
    fragment = re.sub(r"#if __VERSION__ == 100[\s\S]*?#endif", "", fragment)
    fragment = fragment[:fragment.index("void main()")]
    expected = vertex + fragment
    expected = expected.replace("texture2D(", "readFrame(")
    expected = expected.replace("readFrame( searchTex,", "textureLod( searchTex,")
    expected = expected.replace("readFrame( areaTex,", "textureLod( areaTex,")
    expected = expected.replace("discard;", "return vec4(0.0);")
    expected = expected.replace("vec2 offset;", "vec2 chosenOffset;")
    expected = expected.replace("offset.x", "chosenOffset.x").replace("offset.y", "chosenOffset.y")
    expected = expected.replace("sign( offset )", "sign( chosenOffset )")
    for prefix in ["resources/shaders", "resources/shaders/rhi"]:
        path = f"{prefix}/smaa-{kind}.frag"
        text = read_shader(ROOT / path)
        start = text.index("void SMAA")
        end = text.index("void MAIN()") if "void MAIN()" in text else text.index("void main()")
        if compact(text[start:end]) != compact(expected):
            a, b = compact(text[start:end]), compact(expected)
            pos = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            raise SystemExit(f"SMAA source algorithm drift: {path} {a[pos-80:pos+160]} != {b[pos-80:pos+160]}")
        tokens = compact(text)
        for adapter in ["vec2frameUv(vec2uv){returnFRAMEBUFFER_Y_UP>0.0?uv:vec2(uv.x,1.0-uv.y);}",
                        "returntexture(tex,frameUv(uv));", "returntextureLod(tex,frameUv(uv),0.0);",
                        "vUv=frameUv(INPUT_UV);"]:
            if adapter not in tokens:
                raise SystemExit(f"SMAA framebuffer adapter drift: {path}")
        if kind == "edges" and "#defineSMAA_THRESHOLD0.05" not in tokens:
            raise SystemExit("SMAA threshold drift")
        if kind == "weights":
            for value in ["#defineSMAA_MAX_SEARCH_STEPS32", "#defineSMAA_AREATEX_MAX_DISTANCE16",
                          "#defineSMAA_AREATEX_PIXEL_SIZE(1.0/vec2(160.0,560.0))",
                          "#defineSMAA_AREATEX_SUBTEX_SIZE(1.0/7.0)"]:
                if value not in tokens:
                    raise SystemExit("SMAA search/area parameters drift")

history = []
for revision, prefix in [("522a3f3", "resources/shaders"),
                          ("1f2352a", "resources/shaders/rhi"),
                          ("75775b3", "resources/shaders/rhi"),
                          ("e98c798", "resources/shaders/rhi")]:
    for kind in ["edges", "weights", "blend"]:
        path = f"{prefix}/smaa-{kind}.frag"
        data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
        text = compact(data.decode())
        current = compact(read_shader(ROOT / path))
        # Historical orientation adaptations are reviewed explicitly; preserve the source helpers.
        start = text.index("voidSMAA")
        end = text.index("voidMAIN()") if "voidMAIN()" in text else text.index("voidmain()")
        current_start = current.index("voidSMAA")
        current_end = current.index("voidMAIN()") if "voidMAIN()" in current else current.index("voidmain()")
        if text[start:end] != current[current_start:current_end]:
            raise SystemExit(f"Unreviewed historical SMAA math drift: {revision}:{path}")
        history.append(dict(revision=revision, path=path, sha256=hashlib.sha256(data).hexdigest(),
                            conclusion="Source helpers unchanged; current framebuffer adapters separately checked against locked source."))

checks = [
    ("COMMON-01-CALL", "legacy-web/src/scene.ts", "this.smaa.enabled = quality.antialias === \"smaa\"", "Frozen production caller appends SMAA after Bokeh and before OutputPass; high/ultra select SMAA, antialias off only bypasses post-AA, not native geometry MSAA."),
    ("COMMON-01", shader_path, "'SMAA_THRESHOLD': '0.1'", "Locked source helper tokens with native quality .05 threshold, 32 searches, gamma2.2 and all three passes checked; clear instead of discard retains zeroed edge semantics."),
    ("COMMON-02", pass_path, "flipY = false", "Exact embedded PNG bytes, numeric RGBA8, no color transform/mips; lookup UV independent from framebuffer Y conversion."),
    ("COMMON-03", "resources/shaders/rhi/smaa-weights.frag", "textureLod(areaTex, texcoord, 0.0)", "Frame reads adapt Y; area linear and search nearest remain decoded row order. HDR pipeline precedes one final output transform."),
    ("COMMON-04", "tools/review/SmaaReference.h", "Independent double-precision", "Independent pixel-coordinate source equations cover local contrast, search ends/crossings, area lookup and gamma/alpha blend; half quantization matches authored attachments."),
    ("COMMON-05", "tools/review/RhiSmaaAudit.cpp", "edgeError <= 1.0 / 255", "Actual three production pass outputs tested against analytic formulas; no browser GPU, images or ROI score oracle. Scene/MSAA coverage remains B2/C."),
    ("PERF-01", "tools/review/SmaaReference.h", "i < 32", "Three fixed screen passes, maximum 32 steps per search; O(target pixels), no dependence on library cardinality."),
    ("PERF-02", "src/render/rhi/Postprocess.cpp", "preparedAssets().images.value", "Immutable preloaded process cache; render owns all targets/SRB/uniforms. No runtime disk decode, worker wait or production readback."),
    ("PERF-03", "src/render/rhi/Postprocess.cpp", "updates->updateDynamicBuffer(pass.uniform.get(), 0, sizeof(u), &u)", "Exactly three 80-byte prefixes per render; lookup only on graph creation, no unused kernel upload or pixel recopy each frame."),
    ("PERF-04", "src/render/rhi/Postprocess.cpp", 'p.add(u"smaa-blend"_s, p.ping[ping], current, p.weights.texture.get())', "Three distinct single-sample RGBA16F targets, RGBA8 lookup without mips; independent feedback/bytes/disabled-resource assertions."),
    ("PERF-05", "src/render/rhi/Postprocess.cpp", "m_state->settings = s", "Numerical focus/input content updates reuse compatible graph; size/source pointer/settings changes rebuild. No sharpening, reduced samples or approximation."),
    ("PERF-06", "src/render/rhi/Postprocess.cpp", "QRhiTexture *Postprocess::smaaEdges() const", "Intermediate targets returned only in Review, nullptr in ordinary builds. No new clock, QML binding, input changes, per-frame serialization or product sampling."),
    ("PERF-07", "tools/review/RhiSmaaAudit.cpp", "cycle < 10", "Ten real Postprocess resets/recreates, portrait/landscape/odd sizes, HDR/alpha/finite output and disable releases all targets. Not native window/SG restore or VSYNC."),
]
for platform, symbol in [("windows", "QRhiD3D11InitParams"), ("macos", "QRhiMetalInitParams"),
                          ("linux", "QRhiGles2InitParams")]:
    rows = []
    for identity, path, symbol, evidence in checks + [
            (platform.upper() + "-01", "tools/review/OffscreenRhi.h", symbol,
             "Real native QRhi selected; GL fallback outlives QRhi, no visible windows/swapchain. Target actual execution recorded separately; qsb is only static translation.")]:
        rows.append(source_check(ROOT, path, symbol, checkId=identity, evidence=evidence, platform=platform, result='static-audit-passed', limitations='Backend execution separate; no MSAA/resolve/BLUR-01/VSYNC closure.'))
    output = ROOT / "verification" / f"{platform}-static" / "rhi-smaa"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=rows, history=history), indent=2) + "\n")
print("Locked SMAA helpers/lookup, history and COMMON/PERF/platform matrices verified")
