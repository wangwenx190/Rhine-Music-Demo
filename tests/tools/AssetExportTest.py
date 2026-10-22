"""Run source asset exporters in an isolated tree and verify the committed input provenance.

Explicit developer check: needs Node and the locked Web dependencies; ordinary CTest does not.
"""
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AssetExportTest(unittest.TestCase):
    def test_source_commands_preserve_incomplete_output_directories(self):
        for name, options in (("export-title.mjs", []), ("verify-title.mjs", []),
                              ("export-browse-ui.mjs", ["--overview"]),
                              ("export-ruler.mjs", [])):
            with self.subTest(command=name), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "incomplete"
                output.mkdir()
                sentinel = output / "failed-run.log"
                sentinel.write_text("retain original failure", encoding="utf-8")
                result = subprocess.run(["node", str(ROOT / "tools/migration" / name),
                                         *options, "--output", str(output)],
                                        capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertRegex(result.stderr, "fresh|EEXIST")
                self.assertEqual(list(output.iterdir()), [sentinel])
                self.assertEqual(sentinel.read_text(), "retain original failure")

    def test_ruler_export_consumes_the_committed_operations(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo ruler space-") as directory:
            root = Path(directory)
            for name in ("tools/migration/export-ruler.mjs", "tools/migration/ruler-harness.js",
                         "tests/fixtures/navigation/ruler.json"):
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, root / name)
            (root / "tools/migration/frozen-git.mjs").write_text(
                "export const baselineRevision = 'fixture';\n"
                "export const execFrozenGit = () => '';\n", encoding="utf-8")
            # Execute the real exporter with a controlled page, without a browser or timing.
            (root / "tools/migration/browser-source.mjs").write_text("""
export async function wait() {}
export async function withSourcePage(root, html, profile, run) {
    const window = {resetRuler() {}, rulerOperation: (op, fps) => ({op, fps})};
    await run({page: {send: async () => {},
                     evaluate: async expression => Function('window', 'return ' + expression)(window)},
               version: 'fixture', origin: 'http://127.0.0.1'});
}
export async function rollingNumberSource() {
    return 'function L(a,b){return {target:b};}function fe(){}' +
           'function J(track){return {position:track.target,velocity:0};}function Q(){}';
}
""", encoding="utf-8")
            output = root / "output"
            result = subprocess.run(["node", str(root / "tools/migration/export-ruler.mjs"),
                                     "--output", str(output)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((output / "report.json").read_text())
            cases = json.loads((ROOT / "tests/fixtures/navigation/ruler.json").read_text())["cases"]
            expected = {(case["name"], dpr, narrow, night, fps)
                        for case in cases for dpr in (1, 1.5, 2) for narrow in (False, True)
                        for night in (False, True) for fps in (20, 30, 60, 120)}
            self.assertEqual({tuple(c[key] for key in ("name", "dpr", "narrow", "night", "fps"))
                              for c in report["captures"]}, expected)
            self.assertEqual(len(report["captures"]), len(expected))
            operations = {case["name"]: case["operations"] for case in cases}
            for capture in report["captures"]:
                self.assertEqual([state["op"] for state in capture["states"]], operations[capture["name"]])
                self.assertEqual([state["state"] for state in capture["states"]],
                                 [dict(op=op, fps=capture["fps"]) for op in operations[capture["name"]]])

    def assert_oracle_equal(self, actual, expected):
        if isinstance(expected, float):
            # V8 versions can round transcendental source math by a few ULPs.
            self.assertTrue(math.isfinite(actual) and math.isclose(actual, expected,
                                                                  rel_tol=1e-14, abs_tol=1e-12),
                            (actual, expected))
        elif isinstance(expected, dict):
            self.assertEqual(actual.keys(), expected.keys())
            for key in expected:
                self.assert_oracle_equal(actual[key], expected[key])
        elif isinstance(expected, list):
            self.assertEqual(len(actual), len(expected))
            for actual_value, expected_value in zip(actual, expected):
                self.assert_oracle_equal(actual_value, expected_value)
        else:
            self.assertEqual(actual, expected)

    def test_mathematical_exporters_keep_their_independent_oracles(self):
        exporters = {
            "motion": ["motion/kernel-golden.json"],
            "detail": ["motion/detail-golden.json"],
            "overview": ["overview/source-golden.json"],
            "real-columns": ["real-columns/trajectories.json", "real-columns/fields.json"],
            "archive-pool": ["archive-pool/golden.json"],
            "scene": ["scene/camera-golden.json"],
            "opening": ["opening/source-golden.json"],
            "lighting": ["lighting/golden.json"],
        }
        with tempfile.TemporaryDirectory(prefix="yalahoo oracle space-") as directory:
            root = Path(directory)
            shutil.copytree(ROOT / "tools/migration", root / "tools/migration")
            for name in ("resources/manifests/assets.json", "docs/contracts/history-rewrite.json"):
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, root / name)
            (root / "legacy-web").symlink_to(ROOT / "legacy-web", target_is_directory=True)
            (root / ".git").symlink_to(ROOT / ".git", target_is_directory=True)
            for name, outputs in exporters.items():
                with self.subTest(exporter=name):
                    result = subprocess.run(["node", str(root / "tools/migration" / f"export-{name}-golden.mjs")],
                                            cwd=root, capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    for output in outputs:
                        expected = json.loads((ROOT / "tests/fixtures" / output).read_text())
                        actual = json.loads((root / "tests/fixtures" / output).read_text())
                        self.assert_oracle_equal(actual, expected)

    def test_structure_export_requires_one_component(self):
        for arguments in ([], ["--overview", "--presentation"]):
            with self.subTest(arguments=arguments):
                result = subprocess.run(["node", str(ROOT / "tools/migration/export-browse-ui.mjs"), *arguments],
                                        capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Select exactly one source structure", result.stderr)

    def test_structure_export_rejects_ambiguous_arguments_before_capture(self):
        for arguments in (["--overview"], ["--overview", "--output"],
                          ["--overview", "--output", "--dpr-1"],
                          ["--overview", "--output", "a", "--output", "b"],
                          ["--overview", "--overview"], ["--overview", "--unknown"],
                          ["--presentation", "--overview-native", "report.json"]):
            with self.subTest(arguments=arguments):
                result = subprocess.run(["node", str(ROOT / "tools/migration/export-browse-ui.mjs"), *arguments],
                                        capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertRegex(result.stderr, "Specify|Duplicate|Unknown|requires")

    def test_material_export_preserves_finite_half_float_input(self):
        manifest = json.loads((ROOT / "resources/manifests/materials.json").read_text())
        with tempfile.TemporaryDirectory(prefix="yalahoo material space-") as directory:
            root = Path(directory)
            inputs = ["tools/assets/export-material-data.mjs", "docs/contracts/materials.json",
                      "resources/manifests/materials.json", "tests/fixtures/materials/single-case.json"]
            inputs += ["resources/" + asset["path"] for asset in manifest["assets"]]
            for name in inputs:
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, root / name)
            (root / "legacy-web").symlink_to(ROOT / "legacy-web", target_is_directory=True)
            result = subprocess.run(["node", str(root / "tools/assets/export-material-data.mjs")],
                                    cwd=root, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(json.loads((root / "resources/manifests/materials.json").read_text()), manifest)
            self.assertEqual((root / "resources/environment/dfg.rgba32f").read_bytes(),
                             (ROOT / "resources/environment/dfg.rgba32f").read_bytes())

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
