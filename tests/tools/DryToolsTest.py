"""Exercise common replay failure reporting and shader include/kernel boundaries."""
import argparse
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
sys.path.insert(0, str(ROOT / "tools"))
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


if __name__ == "__main__":
    unittest.main()
