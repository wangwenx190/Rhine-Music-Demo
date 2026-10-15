"""Export only the locked Bokeh sample coordinates as a mathematical test input."""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
source = ROOT / "legacy-web/node_modules/three/examples/jsm/shaders/BokehShader.js"
manifest = json.loads((ROOT / "resources/manifests/postprocess.json").read_text())
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
if source_hash != manifest["algorithms"][str(source.relative_to(ROOT))]:
    raise SystemExit("Locked Bokeh source hash differs from the existing manifest")
body = source.read_text()
pattern = (
    r"col \+= texture2D\( tColor, vUv.xy"
    r"(?: \+ \( vec2\(\s*([-\d.]+),\s*([-\d.]+)\s*\)"
    r" \* aspectcorrect \) \* dofblur([974])?)? \);"
)
taps = []
for x, y, ring in re.findall(pattern, body):
    scale = int(ring) / 10 if ring else 1
    taps.append([float(x or 0) * scale, float(y or 0) * scale])
if len(taps) != 41 or "gl_FragColor = col / 41.0" not in body:
    raise SystemExit("Locked source Bokeh structure changed")
for relative in ["resources/shaders/bokeh.frag", "resources/shaders/rhi/bokeh.frag"]:
    native = (ROOT / relative).read_text()
    # Whitespace is normalized separately; the source coordinates must not be fitted.
    compact = re.sub(r"\s+", "", native)
    source_body = body[body.index("vec2 aspectcorrect"):body.index("gl_FragColor = col / 41.0;")]
    expected = re.sub(r"\s+", "", source_body)
    start = compact.index("vec2aspectcorrect")
    end = compact.index("FRAGCOLOR=col/41.0;")
    linear_depth = (
        "-mix(cameraNear,cameraFar,dot(texture(depthMap,INPUT_UV).rgb,"
        "vec3(65536.0,256.0,1.0)/65793.0))"
    )
    expected = expected.replace("getViewZ(getDepth(vUv))", linear_depth)
    expected = expected.replace("texture2D(tColor,vUv", "texture(INPUT,INPUT_UV")
    # The original equation comment mentions viewZ only; keep it in both sources.
    if compact[start:end] != expected:
        raise SystemExit(f"Bokeh source algorithm drift: {relative}")
fixture = ROOT / "tests/fixtures/postprocess/bokeh-kernel.json"
result = json.dumps({
    "source": "three@0.183.2/examples/jsm/shaders/BokehShader.js",
    "sha256": source_hash,
    "taps": taps,
}, indent=2) + "\n"
if "--check" in sys.argv:
    if fixture.read_text() != result:
        raise SystemExit("Locked Bokeh mathematical test input differs")
else:
    fixture.write_text(result)
print("41 locked source taps verified")
