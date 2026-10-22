"""Validate the QML resource closure compiled into a native build."""
from pathlib import Path
import xml.etree.ElementTree as ET


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
