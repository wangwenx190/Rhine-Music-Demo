"""Verify native resource preparation and transactional publication in isolated input trees."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AssetExportTest(unittest.TestCase):
    def setUp(self):
        build = ROOT / "build/release"
        self.tool = build / "tools" / ("prepare-assets.exe" if os.name == "nt" else "prepare-assets")
        self.kit = Path(json.loads((build / "toolchain.json").read_text())["qtKit"])
        self.assertTrue(self.tool.is_file(), "Build the Release prepare-assets target first")
        directory = tempfile.TemporaryDirectory(prefix="yalahoo native assets space-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        shutil.copytree(ROOT / "resources", self.root / "resources")
        self.environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                                QT_PLUGIN_PATH=str(self.kit / "plugins"))
        self.environment["PATH"] = str(self.kit / "bin") + os.pathsep + os.environ.get("PATH", "")

    def run_tool(self, *arguments):
        return subprocess.run([str(self.tool), str(self.root), str(self.kit), *arguments],
                              cwd=self.root, env=self.environment, capture_output=True,
                              text=True, timeout=90)

    def contents(self):
        return {str(path.relative_to(self.root)): path.read_bytes()
                for path in (self.root / "resources").rglob("*") if path.is_file()}

    def refresh_inventory(self):
        path = self.root / "resources/manifests/assets.json"
        manifest = json.loads(path.read_text())
        for entry in manifest["entries"]:
            payload = (self.root / "resources" / entry["path"]).read_bytes()
            entry.update(bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest())
        path.write_text(json.dumps(manifest))

    def test_native_inputs_reimport_models_and_preserve_maintained_assets(self):
        notices = ("audio/README.md", "demo-covers/README.md", "licenses/NOTICE.md")
        for name in notices:
            path = self.root / "resources" / name
            path.write_bytes(path.read_bytes() + b"\nMaintained native attribution.\n")
        self.refresh_inventory()
        original = self.contents()
        verified = self.run_tool("--verify-inputs")
        self.assertEqual(verified.returncode, 0, verified.stdout + verified.stderr)
        self.assertEqual(self.contents(), original)
        generated = self.run_tool()
        self.assertEqual(generated.returncode, 0, generated.stdout + generated.stderr)
        derived = {"resources/manifests/assets.json", "resources/manifests/geometry.json",
                   "resources/models/Music_case_v040.qml",
                   *(f"resources/models/meshes/meshes_{index}__mesh.mesh" for index in range(3))}
        actual = self.contents()
        self.assertEqual(set(actual), set(original))
        for name, content in original.items():
            if name not in derived:
                self.assertEqual(actual[name], content, name)
        manifest = json.loads(actual["resources/manifests/assets.json"])
        for entry in manifest["entries"]:
            payload = actual["resources/" + entry["path"]]
            self.assertEqual(entry["bytes"], len(payload))
            self.assertEqual(entry["sha256"], hashlib.sha256(payload).hexdigest())
        self.assertEqual(list(self.root.glob(".asset-transaction-*")), [])

    def test_invalid_inventory_or_model_keeps_all_original_files(self):
        manifest_path = self.root / "resources/manifests/assets.json"
        original_manifest = manifest_path.read_bytes()
        original = json.loads(original_manifest)
        for malformed in ([], {"schemaVersion": 1, "entries": []},
                          {**original, "entries": [original["entries"][0]] * 2},
                          {**original, "entries": [{"path": "../outside"}]}):
            with self.subTest(manifest=type(malformed).__name__):
                manifest_path.write_text(json.dumps(malformed))
                before = self.contents()
                result = self.run_tool()
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(self.contents(), before)
                self.assertEqual(list(self.root.glob(".asset-transaction-*")), [])
        manifest_path.write_bytes(original_manifest)
        model = self.root / "resources/models/music-case-v040.glb"
        model.write_bytes(b"invalid native model")
        # An intentional source edit updates its inventory first; bad geometry still fails.
        self.refresh_inventory()
        before = self.contents()
        result = self.run_tool()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.contents(), before)
        self.assertEqual(list(self.root.glob(".asset-transaction-*")), [])

    def test_linked_native_input_is_rejected_without_touching_the_target(self):
        path = self.root / "resources/models/music-case-v040.glb"
        outside = self.root / "external.glb"
        outside.write_bytes(path.read_bytes())
        path.unlink()
        try:
            path.symlink_to(outside)
        except OSError as error:
            self.skipTest(f"Symbolic links unavailable: {error}")
        before = outside.read_bytes()
        result = self.run_tool("--verify-inputs")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(outside.read_bytes(), before)
        self.assertEqual(list(self.root.glob(".asset-transaction-*")), [])


if __name__ == "__main__":
    unittest.main()
