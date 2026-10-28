"""Read local shader include closures and tokenize equations without evaluating source code."""
from pathlib import Path
import re

TOKEN_PATTERN = r"[A-Za-z_][A-Za-z_0-9]*|(?:\d*\.\d+|\d+\.?\d*)(?:[eE][+-]?\d+)?|\S"
LIGHTING_UNIFORMS = {
    'rowColumn': 'f.rowColumn.xyz', 'rowSettings': 'f.rowSettings',
    'lightColumn': 'f.lightColumn.xyz', 'printAmbient': 'f.settings.x',
    'rowFill': 'f.settings.z', 'rowDetail': 'f.settings.y',
    'spotPosition': 'f.spotPosition.xyz', 'spotTarget': 'f.spotTarget.xyz',
    'spotColor': 'f.spotColor.xyz', 'spotIntensity': 'f.optics.x',
    'groundColor': 'f.groundColor.xyz', 'skyColor': 'f.skyColor.xyz',
    'lightIntensities': 'f.lightIntensities.xyz', 'keyColor': 'f.keyColor.xyz',
}


def replace_identifiers(source, mapping):
    # Replace original tokens once: a replacement may itself contain another mapped name.
    return re.sub(r'\b[A-Za-z_][A-Za-z_0-9]*\b',
                  lambda match: mapping.get(match[0], match[0]), source)


def hoist_origin_lighting(source, field):
    for name, channel in (('musicRowField', 'x'), ('musicPrintExposure', 'y')):
        source, count = re.subn(r'float ' + name + r'\(vec3 origin\)\s*\{.*?\n\}',
                                'float ' + name + '(vec3 origin)\n{\n    return '
                                + field + '.' + channel + ';\n}', source, flags=re.S)
        if count != 1:
            raise ValueError('Expected exactly one shared origin function: ' + name)
    return source


def tokens(text):
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
    return re.findall(TOKEN_PATTERN, text)


def read_shader(path, ancestors=()):
    path = Path(path).resolve()
    if path in ancestors:
        raise ValueError(f"Cyclic shader include: {path}")
    source = path.read_text(encoding="utf-8")
    def include(name):
        candidate = path.parent / name
        if not candidate.is_file():
            shader_root = Path(__file__).resolve().parents[1] / "resources/shaders"
            candidate = shader_root / "rhi" / name
        return read_shader(candidate, (*ancestors, path))
    return re.sub(r'^\s*#include "([^"\n]+)"\s*$',
                  lambda m: include(m[1]), source, flags=re.M)


def gaussian_kernel(source):
    center = re.search(r"const float GaussianCenterWeight\s*=\s*([0-9.eE+-]+)", source)
    table = re.search(r"const vec2 GaussianPairs\[\d+\]\s*=\s*vec2\[\]\((.*?)\);", source, re.S)
    pairs = re.findall(r"vec2\(\s*([0-9.eE+-]+)\s*,\s*([0-9.eE+-]+)\s*\)", table[1])
    return [float(center[1]), *[float(w) for _, w in pairs]], [float(x) for x, _ in pairs]
