"""Run source asset exporters in an isolated tree and verify the committed input provenance.

Explicit developer check: needs Node and the locked Web dependencies; ordinary CTest does not.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AssetExportTest(unittest.TestCase):
    def test_structure_export_requires_one_component(self):
        for arguments in ([], ["--overview", "--presentation"]):
            with self.subTest(arguments=arguments):
                result = subprocess.run(["node", str(ROOT / "tools/migration/export-browse-ui.mjs"), *arguments],
                                        capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Select exactly one source structure", result.stderr)

    def test_exporters_reproduce_the_declared_shader_inputs(self):
        for script, manifest in (("export-ssao-data.mjs", "occlusion.json"),
                                 ("export-postprocess.mjs", "postprocess.json")):
            with self.subTest(script=script), tempfile.TemporaryDirectory(prefix="yalahoo source space-") as directory:
                root = Path(directory)
                for folder in ("tools/assets", "tools/migration", "resources/manifests", "docs/contracts"):
                    (root / folder).mkdir(parents=True)
                for name in (f"tools/assets/{script}", "tools/migration/frozen-git.mjs",
                             "resources/manifests/assets.json", "docs/contracts/history-rewrite.json"):
                    shutil.copyfile(ROOT / name, root / name)
                (root / "legacy-web").symlink_to(ROOT / "legacy-web", target_is_directory=True)
                (root / ".git").symlink_to(ROOT / ".git", target_is_directory=True)
                result = subprocess.run(["node", str(root / "tools/assets" / script)], cwd=root,
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                expected = json.loads((ROOT / "resources/manifests" / manifest).read_text())
                actual = json.loads((root / "resources/manifests" / manifest).read_text())
                self.assertEqual(actual, expected)
                for asset in expected["assets"]:
                    payload = (root / "resources" / asset["path"]).read_bytes()
                    self.assertEqual(len(payload), asset["bytes"])
                    self.assertEqual(hashlib.sha256(payload).hexdigest(), asset["sha256"])


if __name__ == "__main__":
    unittest.main()
