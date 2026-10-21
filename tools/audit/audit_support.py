"""Common source provenance and marker checks for static audits; runtime remains separate."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shader_source import read_shader

ROOT = Path(__file__).resolve().parents[2]
BASELINE = json.loads((ROOT / "resources/manifests/assets.json").read_text())["baselineCommit"]


def historical(revision, path, reason):
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    return dict(revision=revision, sourcePath=path,
                sourceSha256=hashlib.sha256(data).hexdigest(), evidence=reason)


def source_check(root, path, symbol, **fields):
    file = root / path
    data = file.read_bytes()
    source = read_shader(file) if file.suffix in (".frag", ".vert", ".glsl") else data.decode()
    if " ".join(symbol.split()) not in " ".join(source.split()):
        raise SystemExit(f"Audit source drift: {path}::{symbol}")
    if source != data.decode():
        fields["expandedSourceSha256"] = hashlib.sha256(source.encode()).hexdigest()
    return dict(sourcePath=path, lineOrSymbol=symbol,
                sourceSha256=hashlib.sha256(data).hexdigest(), **fields)
