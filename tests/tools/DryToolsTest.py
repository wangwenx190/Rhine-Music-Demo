"""Exercise common replay failure reporting and shader include/kernel boundaries."""
import argparse
import contextlib
import io
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/runners"))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/deploy"))
import qml_closure
import native_replay
from shader_source import gaussian_kernel, read_shader


class ReplayTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.executable = self.root / "player"
        self.executable.write_bytes(b"anonymous executable fixture")
        chain = self.root / "build/release/toolchain.json"
        chain.parent.mkdir(parents=True)
        chain.write_text(json.dumps(dict(configuration="Release", qtKit=str(self.root / "kit"))))
        self.args = SimpleNamespace(backend="metal", dpr=1, runs=1, foreground=False,
                                    lifecycle=False, night=False, output=self.root / "result")
        for context in [patch.object(native_replay, "ROOT", self.root),
                        patch.object(native_replay, "executable_path", return_value=self.executable),
                        patch.object(native_replay.subprocess, "check_output",
                                     side_effect=lambda *a, **kw: "revision" if kw.get("text") else b"diff")]:
            context.start()
            self.addCleanup(context.stop)

    def run_replay(self, report=None, log_text="", timeout=False):
        def execute(command, **options):
            if timeout:
                raise subprocess.TimeoutExpired(command, 1)
            if report is not None:
                path = Path(command[2]) / "report.json"
                path.parent.mkdir(parents=True)
                path.write_text(report)
            options["stdout"].write(log_text)
            return SimpleNamespace(returncode=0)
        with patch.object(native_replay.subprocess, "run", side_effect=execute), contextlib.redirect_stdout(io.StringIO()):
            result = native_replay.run_replays(argparse.ArgumentParser(), self.args,
                                               "--verify-test", lambda report, args: report.get("dpr") == args.dpr)
        return result, json.loads((self.args.output / "manifest.json").read_text())

    def test_replay_environment_does_not_inherit_visible_or_stress_authorization(self):
        flags = ("YALAHOO_VERIFY_VISIBLE", "YALAHOO_VERIFY_FOREGROUND", "YALAHOO_VERIFY_STRESS",
                 "YALAHOO_VERIFY_LIFECYCLE", "YALAHOO_VERIFY_WINDOW_LIFECYCLE")
        with patch.dict(os.environ, {**dict.fromkeys(flags, "1"), "QSG_NO_VSYNC": "1"}):
            environment = native_replay.replay_environment(self.root / "kit", "metal", 1)
            self.assertTrue(all(environment[flag] == "0" for flag in flags))
            self.assertNotIn("QSG_NO_VSYNC", environment)
            self.assertEqual(native_replay.replay_environment(self.root, "metal", 1, True)
                             ["YALAHOO_VERIFY_FOREGROUND"], "1")

    def test_success_requires_matching_report(self):
        result, manifest = self.run_replay(json.dumps(dict(passed=True, backend="metal", dpr=1)))
        self.assertEqual(result, 0)
        self.assertTrue(manifest["completed"])
        self.assertTrue(manifest["passed"])

    def test_missing_malformed_and_mismatched_reports_fail(self):
        reports = [None, "{broken", "[]", json.dumps(dict(passed=True, backend="opengl", dpr=1)),
                   json.dumps(dict(passed=True, backend="metal", dpr=2))]
        for index, report in enumerate(reports):
            with self.subTest(report=report):
                self.args.output = self.root / f"result-{index}"
                result, manifest = self.run_replay(report)
                self.assertEqual(result, 1)
                self.assertFalse(manifest["passed"])

    def test_qml_errors_fail_and_stop_batch(self):
        self.args.runs = 2
        result, manifest = self.run_replay(json.dumps(dict(passed=True, backend="metal", dpr=1)), "TypeError: fixture")
        self.assertEqual(result, 1)
        self.assertFalse(manifest["completed"])
        self.assertEqual(len(manifest["runs"]), 1)

    def test_timeout_is_recorded_as_failure(self):
        result, manifest = self.run_replay(timeout=True)
        self.assertEqual(result, 1)
        self.assertEqual(manifest["runs"][0]["exitCode"], "timeout")
        self.assertFalse(manifest["passed"])

    def test_existing_evidence_is_preserved(self):
        self.args.output.mkdir()
        sentinel = self.args.output / "prior.json"
        sentinel.write_text("prior")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.run_replay()
        self.assertEqual(sentinel.read_text(), "prior")


class ShaderSourceTest(unittest.TestCase):
    def test_relative_include_and_cycle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            parent, child = root / "parent.frag", root / "child.glsl"
            parent.write_text('#include "child.glsl"\nvoid main() {}\n')
            child.write_text("const float Fixture = 3;\n")
            self.assertIn("const float Fixture = 3;", read_shader(parent))
            child.write_text('#include "parent.frag"\n')
            with self.assertRaisesRegex(ValueError, "Cyclic"):
                read_shader(parent)

    def test_kernel_ignores_unrelated_vectors(self):
        source = """vec2 unrelated = vec2(8.0, 9.0);
const float GaussianCenterWeight = 0.5;
const vec2 GaussianPairs[1] = vec2[](vec2(1.5, 0.25));
"""
        self.assertEqual(gaussian_kernel(source), ([0.5, 0.25], [1.5]))

    def test_production_blur_kernels_match_the_authored_gaussians(self):
        for filename, sigma in (("opening-blur.frag", 10), ("overview-blur.frag", 12),
                                ("detail-tabs-blur.frag", 18)):
            with self.subTest(shader=filename):
                actual_weights, actual_offsets = gaussian_kernel(
                    read_shader(ROOT / "resources/shaders" / filename))
                weights = [math.exp(-i * i / (2 * sigma * sigma)) for i in range(3 * sigma + 1)]
                total = weights[0] + 2 * sum(weights[1:])
                weights = [value / total for value in weights]
                expected_weights = [weights[0]] + [weights[i] + weights[i + 1]
                                                   for i in range(1, len(weights), 2)]
                expected_offsets = [i + weights[i + 1] / (weights[i] + weights[i + 1])
                                    for i in range(1, len(weights), 2)]
                self.assertEqual(len(actual_weights), len(expected_weights))
                self.assertEqual(len(actual_offsets), len(expected_offsets))
                for actual, expected in zip(actual_weights + actual_offsets,
                                            expected_weights + expected_offsets):
                    self.assertAlmostEqual(actual, expected, delta=1e-10)
                self.assertAlmostEqual(actual_weights[0] + 2 * sum(actual_weights[1:]), 1,
                                       delta=1e-10)


@unittest.skipUnless(shutil.which("sh"), "POSIX asset entry tests require sh; Windows uses PowerShell entries")
class AssetEntryTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="yalahoo asset space-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()
        self.kit = self.root / "selected kit"
        self.trace = self.root / "trace.jsonl"
        for name in ("tools/build/run-asset-tool.sh", "tools/assets/export-branding.sh",
                     "tools/assets/prepare-assets.sh"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        build = self.root / "build/release"
        (build / "tools").mkdir(parents=True)
        (build / "CMakeCache.txt").write_text(f"YALAHOO_SELECTED_QT:PATH={self.kit}\n")
        binary = self.root / "test-bin"
        binary.mkdir()
        stub = f"#!{sys.executable}\n" + r"""import json, os, pathlib, sys
name = pathlib.Path(sys.argv[0]).name
if name == 'uname':
    print(os.environ.get('YALAHOO_FIXTURE_SYSTEM', 'Linux'))
    sys.exit(0)
record = dict(name=name, arguments=sys.argv[1:], plugins=os.environ.get('QT_PLUGIN_PATH'),
              library=os.environ.get('LD_LIBRARY_PATH'), frameworks=os.environ.get('DYLD_LIBRARY_PATH'))
with open(os.environ['YALAHOO_FIXTURE_TRACE'], 'a') as trace:
    trace.write(json.dumps(record) + '\n')
stage = ('build' if '--build' in sys.argv else 'configure') if name == 'cmake' else 'tool'
sys.exit(7 if os.environ.get('YALAHOO_FIXTURE_FAIL') == stage else 0)
"""
        for path in (binary / "cmake", binary / "uname", build / "tools/export-branding",
                     build / "tools/prepare-assets"):
            path.write_text(stub)
            path.chmod(0o755)
        self.environment = {**os.environ, "PATH": str(binary) + os.pathsep + os.environ["PATH"],
                            "YALAHOO_FIXTURE_TRACE": str(self.trace), "LD_LIBRARY_PATH": "original",
                            "DYLD_LIBRARY_PATH": "original"}

    def run_entry(self, name, *arguments, **environment):
        self.trace.unlink(missing_ok=True)
        result = subprocess.run(["sh", str(self.root / "tools/assets" / (name + ".sh")), *arguments],
                                env={**self.environment, **environment}, capture_output=True,
                                text=True, timeout=5)
        records = [json.loads(line) for line in self.trace.read_text().splitlines()] if self.trace.exists() else []
        return result, records

    def test_both_entries_preserve_arguments_and_use_the_selected_kit(self):
        for system, key in (("Linux", "library"), ("Darwin", "frameworks")):
            with self.subTest(system=system):
                result, records = self.run_entry("export-branding", "qt-release", "--window-preview", "space argument",
                                                 YALAHOO_FIXTURE_SYSTEM=system)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(records[0]["arguments"], ["--preset", "qt-release"])
                self.assertEqual(records[1]["arguments"][-2:], ["--target", "export-branding"])
                self.assertEqual(records[2]["arguments"], [str(self.root), "--window-preview", "space argument"])
                self.assertEqual(records[2]["plugins"], str(self.kit / "plugins"))
                self.assertEqual(records[2][key].split(":")[0], str(self.kit / "lib"))
                # Protected macOS shells strip inherited DYLD_* before the script starts.
                if key == "library" or sys.platform != "darwin":
                    self.assertEqual(records[2][key], str(self.kit / "lib") + ":original")
        result, records = self.run_entry("prepare-assets")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(records[2]["arguments"], [str(self.root), str(self.kit)])

    def test_invalid_preset_and_each_native_failure_stop_the_pipeline(self):
        result, records = self.run_entry("export-branding", "invalid-preset")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(records, [])
        for stage, expected_calls in (("configure", 1), ("build", 2), ("tool", 3)):
            with self.subTest(stage=stage):
                result, records = self.run_entry("prepare-assets", YALAHOO_FIXTURE_FAIL=stage)
                self.assertEqual(result.returncode, 7)
                self.assertEqual(len(records), expected_calls)


class DeploymentQmlTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()
        self.qml = self.root / "qml"
        self.qml.mkdir()
        self.source = self.qml / "App.qml"
        self.source.write_text("import QtQuick\nItem {}\n")
        self.extra = self.qml / "Research.qml"
        self.extra.write_text("import QtQuick3D\nNode {}\n")
        self.build = self.root / "build"
        self.qrc = self.build / "src/app/.qt/rcc/Yalahoo_raw_qml_0.qrc"
        self.qrc.parent.mkdir(parents=True)
        self.metadata = dict(qmlFiles=["App.qml"], review=False)
        self.write_qrc(self.source)

    def write_qrc(self, *paths):
        self.qrc.write_text("<RCC><qresource>" + "".join(f"<file>{p}</file>" for p in paths)
                            + "</qresource></RCC>")

    def test_exact_compiled_closure_and_review_subset(self):
        self.assertEqual(qml_closure.production_qml(self.root, self.build, self.metadata),
                         [self.source])
        self.write_qrc(self.source, self.extra)
        with self.assertRaisesRegex(RuntimeError, "compiled QML"):
            qml_closure.production_qml(self.root, self.build, self.metadata)
        self.metadata["review"] = True
        self.assertEqual(qml_closure.production_qml(self.root, self.build, self.metadata),
                         [self.source])
        self.write_qrc(self.extra)
        with self.assertRaisesRegex(RuntimeError, "compiled QML"):
            qml_closure.production_qml(self.root, self.build, self.metadata)

    def test_empty_duplicate_absolute_traversal_missing_and_symlink_paths_fail(self):
        outside = self.root / "Outside.qml"
        outside.write_text("import QtQuick\nItem {}")
        (self.qml / "Link.qml").symlink_to(outside)
        for paths in ([], ["App.qml", "App.qml"], [str(self.source)],
                      ["../Outside.qml"], ["Missing.qml"], ["Link.qml"]):
            with self.subTest(paths=paths), self.assertRaises(RuntimeError):
                qml_closure.production_qml(self.root, self.build,
                                              dict(qmlFiles=paths, review=False))


if __name__ == "__main__":
    unittest.main()
