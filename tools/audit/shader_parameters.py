"""Expose named literal shader coefficients to independent source-math audits."""
import re


def numeric_source(source):
    """Expand only literal scalar/vector tables; never evaluate arbitrary shader code."""
    scalars = {}
    pairs = {}
    scalar = re.compile(r"const float (\w+)\s*=\s*([0-9.eE+-]+)\s*;")
    table = re.compile(r"const vec2 (\w+)\[\d+\]\s*=\s*vec2\[\]\((.*?)\);", re.S)
    for match in scalar.finditer(source):
        scalars[match[1]] = match[2]
    for match in table.finditer(source):
        pairs[match[1]] = re.findall(
            r"vec2\(\s*([0-9.eE+-]+)\s*,\s*([0-9.eE+-]+)\s*\)", match[2])
    source = scalar.sub("", table.sub("", source))
    for name, values in pairs.items():
        for index, pair in enumerate(values):
            for component, value in zip("xy", pair):
                reference = r"\b" + name + r"\[" + str(index) + r"\]\." + component + r"\b"
                source = re.sub(reference, value, source)
    for name, value in scalars.items():
        source = re.sub(r"\b" + name + r"\b", value, source)
    return source
