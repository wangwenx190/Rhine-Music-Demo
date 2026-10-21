"""Read local shader include closures and tokenize equations without evaluating source code."""
from pathlib import Path
import re

TOKEN_PATTERN = r"[A-Za-z_][A-Za-z_0-9]*|(?:\d*\.\d+|\d+\.?\d*)(?:[eE][+-]?\d+)?|\S"


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
