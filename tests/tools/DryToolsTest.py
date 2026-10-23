"""Exercise common replay failure reporting and shader include/kernel boundaries."""
import argparse
import contextlib
import io
import json
import math
import os
import runpy
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
import LoggingTerminalTest as terminal
from shader_source import gaussian_kernel, read_shader, replace_identifiers


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
                                    night=False, output=self.root / "result")
        for context in [patch.object(native_replay, "ROOT", self.root),
                        patch.object(native_replay, "executable_path", return_value=self.executable),
                        patch.object(native_replay.subprocess, "check_output",
                                     side_effect=lambda *a, **kw: "revision" if kw.get("text") else b"diff")]:
            context.start()
            self.addCleanup(context.stop)

    def run_replay(self, report=None, log_text="", timeout=False, launch_error=False,
                   validation_error=False):
        def execute(command, **options):
            if timeout:
                raise subprocess.TimeoutExpired(command, 1)
            if launch_error:
                raise PermissionError("fixture executable permission denied")
            if report is not None:
                path = Path(command[2]) / "report.json"
                path.parent.mkdir(parents=True)
                path.write_text(report)
            options["stdout"].write(log_text)
            return SimpleNamespace(returncode=0)
        def validate(report, args, output):
            if validation_error:
                raise PermissionError("fixture output permission denied")
            return report.get("dpr") == args.dpr

        with patch.object(native_replay.subprocess, "run", side_effect=execute), contextlib.redirect_stdout(io.StringIO()):
            result = native_replay.run_replays(argparse.ArgumentParser(), self.args,
                                               "--verify-test", validate)
        return result, json.loads((self.args.output / "manifest.json").read_text())

    def run_gui(self, output, report, log_text=""):
        def execute(command, **options):
            options['stdout'].write(log_text)
            destination = Path(command[2]) / 'report.json'
            destination.parent.mkdir(parents=True)
            if report is not None:
                destination.write_text(report)
                try:
                    data = json.loads(report)
                except ValueError:
                    data = {}
                captures = data.get('captures', []) if isinstance(data, dict) else []
                if not isinstance(captures, list):
                    captures = []
                for capture in captures:
                    if isinstance(capture, dict) and isinstance(capture.get('image'), str):
                        (destination.parent / capture['image']).write_bytes(b'anonymous output fixture')
            return SimpleNamespace(returncode=0)
        with patch.object(sys, 'argv', ['verify-gui.py', '--backend', 'metal', '--representative',
                                       '--output', str(output)]), \
                patch.object(native_replay, 'cmake_cache', return_value={'YALAHOO_SELECTED_QT:PATH': str(self.root),
                                                                       'CMAKE_BUILD_TYPE:STRING': 'Release'}), \
                patch.object(native_replay.subprocess, 'run', side_effect=execute), \
                contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as result:
            runpy.run_path(str(ROOT / 'tests/runners/verify-gui.py'), run_name='__main__')
        return result.exception.code, json.loads((output / 'manifest.json').read_text())

    def test_gui_persists_failure_for_invalid_reports(self):
        for index, report in enumerate((None, '{broken', '[]', '{"captures": null}', '{"captures": [1]}',
                                        '{"captures": [{"width": []}]}')):
            with self.subTest(report=report):
                code, manifest = self.run_gui(self.root / f'gui-{index}', report)
                self.assertEqual(code, 1)
                self.assertFalse(manifest['passed'])

    @staticmethod
    def gui_captures():
        return [dict(width=w, height=h, night=night, realistic=real, phase=phase,
                         image=f'{w}-{h}-{night}-{real}-{phase}.png')
                    for w, h in ((1280, 720), (930, 900), (390, 844))
                    for night in (False, True) for real in (False, True)
                    for phase in (('browse', 'overview', 'detail', 'search') if real else
                                  ('browse', 'overview', 'detail', 'search', 'opening-mark', 'opening-scene'))]

    def test_gui_requires_complete_captures_and_boolean_success(self):
        captures = self.gui_captures()
        for index, marker in enumerate((True, 'true', 1)):
            with self.subTest(marker=marker):
                report = json.dumps(dict(passed=marker, backend=6, captures=captures))
                code, manifest = self.run_gui(self.root / f'gui-valid-{index}', report)
                self.assertEqual(code, 0 if marker is True else 1)
                self.assertEqual(manifest['passed'], marker is True)
                self.assertEqual(manifest['capturedCheckpoints'], 60)

    def test_gui_rejects_qml_diagnostics_even_when_report_claims_success(self):
        captures = self.gui_captures()
        code, manifest = self.run_gui(self.root / "gui-warning",
                                      json.dumps(dict(passed=True, backend=6, captures=captures)),
                                      "ReferenceError: fixture")
        self.assertEqual(code, 1)
        self.assertFalse(manifest["passed"])

    def test_gui_requires_distinct_images_inside_its_output(self):
        for index, filename in enumerate(("../foreign.png", "duplicate.png")):
            with self.subTest(filename=filename):
                captures = self.gui_captures()
                captures[0]["image"] = filename
                if filename == "duplicate.png":
                    captures[1]["image"] = filename
                code, manifest = self.run_gui(self.root / f"gui-path-{index}",
                                              json.dumps(dict(passed=True, backend=6, captures=captures)))
                self.assertEqual(code, 1)
                self.assertFalse(manifest["passed"])
                self.assertTrue(manifest["imageSha256"])
                self.assertNotIn("../foreign.png", manifest["imageSha256"])

    def test_report_reader_rejects_nonfinite_json_values(self):
        path = self.root / "report.json"
        for token in ("NaN", "Infinity", "-Infinity", "1e999", "-1e999",
                      str(10 ** 400), str(-(10 ** 400))):
            with self.subTest(token=token):
                path.write_text('{"passed": true, "value": ' + token + '}')
                self.assertEqual(native_replay.read_report(path), {})
        path.write_text('{"passed": true, "value": 1.5}')
        self.assertEqual(native_replay.read_report(path), dict(passed=True, value=1.5))

    def test_replay_environment_does_not_inherit_visible_or_lifecycle_authorization(self):
        flags = ("YALAHOO_VERIFY_VISIBLE", "YALAHOO_VERIFY_FOREGROUND", "YALAHOO_VERIFY_WINDOW_LIFECYCLE")
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
                   json.dumps(dict(passed=True, backend="metal", dpr=2)),
                   json.dumps(dict(passed="true", backend="metal", dpr=1)),
                   json.dumps(dict(passed=1, backend="metal", dpr=1))]
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

    def test_launch_failure_is_recorded_without_abandoning_the_manifest(self):
        result, manifest = self.run_replay(launch_error=True)
        self.assertEqual(result, 1)
        self.assertEqual(manifest["runs"][0]["exitCode"], "launch-error")
        self.assertFalse(manifest["passed"])
        self.assertIn("permission denied", (self.args.output / "run-1.log").read_text())

    def test_component_reports_reject_duplicate_states_and_missing_output(self):
        overview = runpy.run_path(str(ROOT / "tests/runners/verify-overview.py"))
        detail = runpy.run_path(str(ROOT / "tests/runners/verify-detail.py"))
        args = SimpleNamespace(visual=True, dpr=1, window_lifecycle=False)
        captures = [dict(width=w, height=h, night=night, variant=variant, expanded=variant - 1, dpr=1,
                         file=f"{w}-{h}-{night}-{variant}.png")
                    for w, h in overview["VIEWPORTS"] for night in (False, True) for variant in (0, 1)]
        for capture in captures:
            (self.root / capture["file"]).write_bytes(b"anonymous capture")
        validate = overview["validate_report"]
        self.assertTrue(validate(dict(captures=captures), args, self.root))
        for change in (dict(width=True), dict(dpr=True), dict(night=1), dict(expanded=True), dict(variant=True),
                       dict(variant=1), dict(expanded=-2),
                       dict(file="missing.png"), dict(file="../foreign.png"),
                       dict(file=captures[1]["file"])):
            invalid = [dict(c) for c in captures]
            invalid[0].update(change)
            with self.subTest(change=change):
                self.assertFalse(validate(dict(captures=invalid), args, self.root))
        repeated = [captures[0]] * len(captures)
        self.assertFalse(validate(dict(captures=repeated), args, self.root))
        self.assertFalse(validate(dict(captures=None), args, self.root))
        (self.root / captures[0]["file"]).write_bytes(b"")
        self.assertFalse(validate(dict(captures=captures), args, self.root))
        report = dict(dpr=1, windowLifecycleRequested=False, windowTransitions=[])
        self.assertTrue(detail["validate_report"](report, args, self.root))
        for change in (dict(dpr=True), dict(windowLifecycleRequested=0), dict(windowTransitions=None)):
            with self.subTest(change=change):
                self.assertFalse(detail["validate_report"]({**report, **change}, args, self.root))

    def test_validation_io_failure_keeps_a_failed_manifest(self):
        result, manifest = self.run_replay(json.dumps(dict(passed=True, backend="metal", dpr=1)),
                                           validation_error=True)
        self.assertEqual(result, 1)
        self.assertFalse(manifest["passed"])
        self.assertFalse(manifest["runs"][0]["passed"])

    def test_existing_evidence_is_preserved(self):
        self.args.output.mkdir()
        sentinel = self.args.output / "prior.json"
        sentinel.write_text("prior")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.run_replay()
        self.assertEqual(sentinel.read_text(), "prior")


@unittest.skipIf(os.name == "nt", "PTY is a POSIX terminal boundary")
class TerminalCaptureTest(unittest.TestCase):
    def capture(self, source, timeout):
        import pty
        master, slave = pty.openpty()
        process = subprocess.Popen([sys.executable, "-c", source], stderr=slave)
        os.close(slave)
        try:
            return terminal.read_terminal(master, process, timeout)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            os.close(master)

    def test_terminal_is_drained_while_the_child_writes(self):
        self.assertEqual(self.capture("import os; os.write(2, b'x' * 100000)", 5), b'x' * 100000)

    def test_stalled_terminal_read_is_bounded(self):
        with self.assertRaises(subprocess.TimeoutExpired):
            self.capture("import os, time; os.write(2, b'fixture'); time.sleep(30)", .1)


class ShaderSourceTest(unittest.TestCase):
    def test_identifier_replacements_do_not_cascade_or_depend_on_mapping_order(self):
        source = "origin + world + originLight + worldPosition"
        entries = [("origin", "world"), ("world", "f.camera.xyz"),
                   ("worldPosition", "world")]
        expected = "world + f.camera.xyz + originLight + world"
        self.assertEqual(replace_identifiers(source, dict(entries)), expected)
        self.assertEqual(replace_identifiers(source, dict(reversed(entries))), expected)
        self.assertEqual(replace_identifiers(source, {}), source)

    def test_shell_generator_rejects_missing_origin_function(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('tools/assets/prepare-rhi-shell.py', 'tools/shader_source.py',
                         'resources/shaders/case-glass.frag'):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, target)
            (root / 'resources/shaders/rhi').mkdir()
            source = root / 'resources/shaders/case-glass.frag'
            source.write_text(source.read_text().replace('float musicRowField(', 'float missingRowField('))
            result = subprocess.run([sys.executable, str(root / 'tools/assets/prepare-rhi-shell.py')],
                                    capture_output=True, text=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Expected exactly one shared origin function: musicRowField', result.stderr)

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


class CompilerAuditTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="yalahoo audit space-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()
        self.build = self.root / "build/release"
        self.build.mkdir(parents=True)
        paths = ("src/one.cpp", "tests/two.cpp", "build/generated.cpp")
        for name in paths:
            path = self.root / name
            path.parent.mkdir(exist_ok=True)
            path.write_text("// fixture\n")
        commands = [dict(directory=str(self.root), file=name) for name in (*paths, paths[0])]
        (self.build / "compile_commands.json").write_text(json.dumps(commands))
        (self.build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n")
        (self.build / "toolchain.json").write_text('{}')
        self.main = runpy.run_path(str(ROOT / "tools/audit/audit-best-practices.py"))["main"]
        self.main.__globals__["ROOT"] = self.root

    def execute(self, *options, outcome=0):
        calls = []

        def analyze(command, **kwargs):
            calls.append(Path(command[3]).relative_to(self.root).as_posix())
            self.assertEqual(kwargs["timeout"], 2)
            if isinstance(outcome, Exception):
                raise outcome
            if outcome == "diagnostic":
                kwargs["stdout"].write("fixture.cpp:1:1: warning: fixture safety defect\n")
            return SimpleNamespace(returncode=0)

        output = self.root / "result"
        with patch.object(subprocess, "run", side_effect=analyze), \
                patch.object(subprocess, "check_output", return_value="fixture-tool"), \
                contextlib.redirect_stdout(io.StringIO()):
            result = self.main(["--build-directory", str(self.build), "--output", str(output),
                                "--clang-tidy", "fixture-tool", "--jobs", "1", "--timeout", "2", *options])
        return result, calls, json.loads((output / "report.json").read_text())

    def test_compiled_units_are_deduplicated_and_generated_units_are_excluded(self):
        result, calls, report = self.execute()
        self.assertEqual(result, 0)
        self.assertEqual(calls, ["src/one.cpp", "tests/two.cpp"])
        self.assertTrue(report["compilerChecksClean"])

    def test_explicit_scope_runs_only_requested_compiled_sources(self):
        result, calls, report = self.execute("--source", "tests/two.cpp", "--source", "tests/two.cpp")
        self.assertEqual(result, 0)
        self.assertEqual(calls, ["tests/two.cpp"])
        self.assertEqual(len(report["translationUnits"]), 1)

    def test_diagnostics_timeout_and_launch_errors_preserve_failure_reports(self):
        for index, outcome in enumerate(("diagnostic", subprocess.TimeoutExpired("fixture", 2),
                                         PermissionError("fixture denied"))):
            with self.subTest(outcome=outcome):
                result, calls, report = self.execute("--source", "src/one.cpp", outcome=outcome)
                self.assertEqual(result, 1)
                self.assertFalse(report["compilerChecksClean"])
                self.assertEqual(report["translationUnits"][0]["result"], "review-required")
                (self.root / "result").rename(self.root / f"failure-{index}")

    def test_uncompiled_scope_fails_before_launching_analysis(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
            self.execute("--source", "src/missing.cpp")
        self.assertEqual(caught.exception.code, 2)


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
