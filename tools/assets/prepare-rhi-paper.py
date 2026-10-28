"""Adapt shared source paper math; preserve per-entity hoisting in the RHI variant."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shader_source import (LIGHTING_UNIFORMS, hoist_origin_lighting, read_shader,
                           replace_identifiers, tokens)

ROOT = Path(__file__).resolve().parents[2]
source = read_shader(ROOT / "resources/shaders/album-paper.frag")
source = source[source.index("// BEGIN SHARED LIGHTING"):source.index("// END SHARED LIGHTING")]
a = source.index("vec3 musicShell(")
b = source.index("#define CUBEUV_MAX_MIP", a)
source = source[:a] + source[b:]
source = source.replace("Shared field, diffuse and shell transport", "Shared field and diffuse transport")
source = replace_identifiers(source, LIGHTING_UNIFORMS)
# Only immutable origin/frame terms are hoisted; local normal/view terms stay in GLSL.
source = hoist_origin_lighting(source, "entityLight0")
source = source.rstrip() + "\n"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
target = ROOT / "resources/shaders/rhi/paper-lighting.glsl"
if args.check:
    if tokens(source) != tokens(target.read_text()):
        raise SystemExit("Stale RHI paper helper: regenerate and run the native formatter")
    print("RHI paper helper matches shared math with explicit uniform/hoisting adaptations")
else:
    target.write_text(source, newline="\n")
    print("RHI paper helper generated; run the native format target before building")
