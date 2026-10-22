"""Common source provenance and marker checks for static audits; runtime remains separate."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shader_source import read_shader

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "deploy"))
from qml_closure import production_qml, scan_production_qml

ROOT = Path(__file__).resolve().parents[2]
BASELINE = json.loads((ROOT / "resources/manifests/assets.json").read_text())["baselineCommit"]


def historical(revision, path, reason=None, *, symbol=None):
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    record = dict(revision=revision, sourcePath=path,
                  sourceSha256=hashlib.sha256(data).hexdigest())
    if reason is not None:
        record["evidence"] = reason
    if symbol is not None:
        if symbol not in data.decode("utf-8"):
            raise SystemExit(f"Frozen source drift: {path}::{symbol}")
        record["lineOrSymbol"] = symbol
    return record


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


def source_rows(root, entries, *, platform, requirement=False, **fields):
    """Bind component assertions to current sources without claiming target execution."""
    rows = []
    for identity, path, symbol, evidence in entries:
        details = dict(checkId=identity, platform=platform, evidence=evidence, **fields)
        if requirement:
            details["requirement"] = evidence
        rows.append(source_check(root, path, symbol, **details))
    return rows


def file_dependencies(root, paths):
    return [dict(sourcePath=path,
                 sourceSha256=hashlib.sha256((root / path).read_bytes()).hexdigest())
            for path in paths]


def write_source_report(root, platform, component, payload, name="source-matrix.json"):
    output = root / "verification" / f"{platform}-static" / component
    output.mkdir(parents=True, exist_ok=True)
    (output / name).write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")
    return output


def occluded_polish_guarded(source):
    """Follow the clear-only helper as well as the polish early-return branch."""
    try:
        polish = source[source.index("void SceneItem::updatePolish()") :]
        occluded = polish.index("if (m_sceneOccluded)")
        resumed = polish.index("auto &adapter = *m_adapter;")
        branch = polish[occluded:resumed]
        helper_start = source.index("void SceneItem::submitOccluded()")
        helper_end = source.index("void SceneItem::requestDemoCover()", helper_start)
        helper = source[helper_start:helper_end]
        return (polish.index("!m_renderingActive") < occluded < resumed
                and "submitOccluded();" in branch and "return;" in branch
                and "next.sceneOccluded = true;" in helper
                and "next.preparationToken = 0;" in helper
                and "preparePool(" not in branch + helper)
    except ValueError:
        return False
