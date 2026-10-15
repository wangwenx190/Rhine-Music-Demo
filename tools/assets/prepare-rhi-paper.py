"""Adapt shared source paper math; preserve per-entity hoisting in the RHI variant."""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "resources/shaders/album-paper.frag").read_text()
source = source[source.index("// BEGIN SHARED LIGHTING"):source.index("// END SHARED LIGHTING")]
a = source.index("vec3 musicShell(")
b = source.index("#define CUBEUV_MAX_MIP", a)
source = source[:a] + source[b:]
source = source.replace("Shared field, diffuse and shell transport", "Shared field and diffuse transport")
for name, replacement in {
    "rowColumn": "f.rowColumn.xyz", "rowSettings": "f.rowSettings",
    "lightColumn": "f.lightColumn.xyz", "printAmbient": "f.settings.x",
    "rowFill": "f.settings.z", "rowDetail": "f.settings.y",
    "spotPosition": "f.spotPosition.xyz", "spotTarget": "f.spotTarget.xyz",
    "spotColor": "f.spotColor.xyz", "spotIntensity": "f.optics.x",
    "groundColor": "f.groundColor.xyz", "skyColor": "f.skyColor.xyz",
    "lightIntensities": "f.lightIntensities.xyz", "keyColor": "f.keyColor.xyz",
}.items():
    source = re.sub(r"\b" + name + r"\b", replacement, source)
# Only immutable origin/frame terms are hoisted; local normal/view terms stay in GLSL.
for name, value in [("musicRowField", "entityLight0.x"),
                    ("musicPrintExposure", "entityLight0.y")]:
    source, count = re.subn(r"float " + name + r"\(vec3 origin\)\s*\{.*?\n\}",
                            "float " + name + "(vec3 origin)\n{\n    return " + value + ";\n}",
                            source, flags=re.S)
    if count != 1:
        raise SystemExit("Unexpected shared paper function: " + name)
source = source.rstrip() + "\n"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
target = ROOT / "resources/shaders/rhi/paper-lighting.glsl"
if args.check:
    def tokens(text):
        text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
        return re.findall(r"[A-Za-z_][A-Za-z_0-9]*|(?:\d*\.\d+|\d+\.?\d*)(?:[eE][+-]?\d+)?|\S", text)
    if tokens(source) != tokens(target.read_text()):
        raise SystemExit("Stale RHI paper helper: regenerate and run the native formatter")
    print("RHI paper helper matches shared math with explicit uniform/hoisting adaptations")
else:
    target.write_text(source, newline="\n")
    print("RHI paper helper generated; run the native format target before building")
