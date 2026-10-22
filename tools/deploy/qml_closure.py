"""Validate and scan exactly the QML resource closure compiled into a native build."""
from dataclasses import dataclass
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

# Audio is owned by C++; package inspectors validate native Multimedia libraries/plugins.
REQUIRED_IMPORTS = {"QtQuick", "QtQuick.Templates", "QtQuick.Dialogs"}


def production_qml(root, build, metadata):
    source_root = (root / "qml").resolve()
    paths = []
    for value in metadata["qmlFiles"]:
        relative = Path(value)
        path = (source_root / relative).resolve()
        if (relative.is_absolute() or ".." in relative.parts
                or not path.is_relative_to(source_root) or not path.is_file()):
            raise RuntimeError(f"Invalid production QML path: {value}")
        paths.append(path)
    if not paths or len(paths) != len(set(paths)):
        raise RuntimeError("Empty or duplicate production QML paths")
    compiled = {Path(node.text).resolve() for node in ET.parse(
        build / "src/app/.qt/rcc/Yalahoo_raw_qml_0.qrc").iter("file")
        if node.text and node.text.endswith(".qml")}
    expected = set(paths)
    if not expected <= compiled or (not metadata["review"] and expected != compiled):
        raise RuntimeError("Production metadata differs from compiled QML resources")
    return paths


@dataclass(frozen=True)
class QmlScan:
    modules: list[str]
    scanner: Path
    stderr: str


def scan_production_qml(root, paths, kit):
    scanner = next((file for folder in ("libexec", "bin") for name in
                    ("qmlimportscanner", "qmlimportscanner.exe")
                    if (file := kit / folder / name).is_file()), None)
    if scanner is None:
        raise RuntimeError("Selected Kit import scanner is missing")
    with tempfile.TemporaryDirectory(prefix="yalahoo-production-qml-") as directory:
        for source in paths:
            destination = Path(directory) / source.relative_to((root / "qml").resolve())
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
        scan = subprocess.run([str(scanner), "-rootPath", directory,
                               "-importPath", str(kit / "qml")],
                              text=True, encoding="utf-8", capture_output=True, check=True)
    modules = sorted({entry["name"] for entry in json.loads(scan.stdout)
                      if entry.get("type") == "module"})
    if not REQUIRED_IMPORTS <= set(modules) or any(
            name.startswith(("QtQuick3D", "QtCanvas2D")) for name in modules):
        raise RuntimeError("Unexpected production QML import closure")
    return QmlScan(modules, scanner, scan.stderr)
