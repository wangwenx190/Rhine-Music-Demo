"""Check sampling scope and reject incomplete or falsely promoted verification evidence."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
from types import SimpleNamespace
import tempfile
import sys
import unittest
from unittest.mock import patch

MODULE_PATH = Path(__file__).resolve().parents[2] / "tests/runners/verify-startup.py"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("startup_verification", MODULE_PATH)
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
import native_replay


class StartupVerificationTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.platform_patch = patch.object(VERIFY.platform, "system", return_value="Darwin")
        self.platform_patch.start()
        self.addCleanup(self.platform_patch.stop)

    def arguments(self, *options):
        return VERIFY.parse_arguments(["--backend", "metal", "--output", str(self.root / "result"), *options])

    def report(self, args):
        return dict(functionalPassed=True, samplingComplete=True, backend=6, dpr=args.dpr,
                    scenario=args.scenario,
                    samplingRequestedSeconds=args.seconds, samplingSeconds=args.seconds,
                    switches=0 if args.observe_only else 3,
                    rebuilds=0 if args.observe_only else 2,
                    stages={args.scenario: {"cpuSubmit": {"count": 100}}},
                    swapIntervalRequested=0 if args.mode == "uncapped" else 1,
                    qsgNoVsyncRequested=args.mode == "uncapped", nativeSamplingValid=True,
                    performanceGateEnforced=args.formal, performanceGatePassed=True,
                    observationOnly=args.observe_only, windowPolicy={"passed": True},
                    continuouslyExposedDuringSampling=True, samplingStartedNs=1, samplingEndedNs=3,
                    guiFrames=[{"atNs": 1, "clockFrame": 1}, {"atNs": 2, "clockFrame": 2}],
                    synchronizationTimestampsNs=[1, 2], qtPresentationTimestampsNs=[1, 2])

    def test_scopes_have_distinct_defaults_and_acceptance(self):
        for options, expected in [((), (30, 1)),
                                  (("--formal", "--mode", "vsync", "--scenario", "day-wave"), (120, 1)),
                                  (("--formal", "--mode", "vsync", "--scenario", "day-wave",
                                    "--runs", "3"), (120, 3))]:
            with self.subTest(options=options):
                args = self.arguments(*options)
                self.assertEqual((args.seconds, args.runs), expected)
                self.assertTrue(VERIFY.validate_report(self.report(args), args, 0))

    def test_invalid_scope_combinations_stop_before_execution(self):
        combinations = [("--seconds", "0"), ("--seconds", "121"), ("--formal",),
                        ("--formal", "--mode", "vsync", "--scenario", "day-wave", "--runs", "2"),
                        ("--runs", "10"),
                        ("--formal", "--mode", "uncapped", "--scenario", "day-wave"),
                        ("--formal", "--mode", "vsync", "--scenario", "day-wave", "--seconds", "30"),
                        ("--formal", "--mode", "vsync", "--scenario", "day-wave", "--dpr", "2")]
        for options in combinations:
            with self.subTest(options=options), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as caught:
                    self.arguments(*options)
                self.assertEqual(caught.exception.code, 2)

    def test_incomplete_or_wrong_run_cannot_pass(self):
        args = self.arguments()
        for field, value in [("functionalPassed", False), ("samplingComplete", False),
                             ("samplingSeconds", 12), ("stages", {}), ("backend", 4),
                             ("switches", 2), ("rebuilds", 1),
                             ("scenario", "day-wave"), ("samplingRequestedSeconds", 120)]:
            with self.subTest(field=field):
                report = self.report(args)
                report[field] = value
                self.assertFalse(VERIFY.validate_report(report, args, 0))
        self.assertFalse(VERIFY.validate_report(self.report(args), args, "timeout"))
        self.assertFalse(VERIFY.validate_report({}, args, 0))

    def test_native_diagnostic_requires_exposure_evidence_without_formal_claim(self):
        args = self.arguments("--mode", "vsync", "--scenario", "night-wave")
        report = self.report(args)
        report["performanceGatePassed"] = None
        self.assertTrue(VERIFY.validate_report(report, args, 0))
        report["nativeSamplingValid"] = False
        self.assertFalse(VERIFY.validate_report(report, args, 0))
        args = self.arguments("--formal", "--mode", "vsync", "--scenario", "night-wave")
        report = self.report(args)
        report["performanceGatePassed"] = False
        self.assertFalse(VERIFY.validate_report(report, args, 0))
        report["performanceGatePassed"] = True
        report["performanceGateEnforced"] = False
        self.assertFalse(VERIFY.validate_report(report, args, 0))

    def test_malformed_and_nonfinite_measurements_cannot_pass(self):
        args = self.arguments("--observe-only", "--mode", "vsync", "--scenario", "night-wave")
        for field, values in {
            "samplingSeconds": (float("inf"), float("nan"), "30", True),
            "dpr": (True, "1", float("nan")),
            "switches": (False,), "rebuilds": (False,), "stages": ([1], "timing"),
            "samplingStartedNs": (None, True, "1", float("nan")),
            "samplingEndedNs": (None, "3", float("inf")),
            "guiFrames": (None, [1], [{"atNs": True}, {"atNs": 2}]),
            "synchronizationTimestampsNs": (None, [1, float("inf")], [True, 2]),
            "windowPolicy": (None, True),
        }.items():
            for value in values:
                with self.subTest(field=field, value=value):
                    report = self.report(args)
                    report[field] = value
                    self.assertFalse(VERIFY.validate_report(report, args, 0))

    def test_observation_omits_replay_and_requires_all_timing_streams(self):
        args = self.arguments("--observe-only", "--mode", "vsync", "--scenario", "night-wave",
                              "--seconds", "12")
        self.assertTrue(VERIFY.validate_report(self.report(args), args, 0))
        for field, value in [("switches", 3), ("rebuilds", 2), ("observationOnly", False),
                             ("continuouslyExposedDuringSampling", False), ("guiFrames", []),
                             ("synchronizationTimestampsNs", []), ("qtPresentationTimestampsNs", []),
                             ("guiFrames", [{"atNs": 4}, {"atNs": 5}]),
                             ("synchronizationTimestampsNs", [2, 1]),
                             ("windowPolicy", {"passed": False}), ("performanceGateEnforced", True)]:
            with self.subTest(field=field):
                report = self.report(args)
                report[field] = value
                self.assertFalse(VERIFY.validate_report(report, args, 0))
        invalid = [("--observe-only",),
                   ("--observe-only", "--mode", "vsync", "--scenario", "night-detail"),
                   ("--observe-only", "--mode", "vsync", "--scenario", "night-wave", "--runs", "3"),
                   ("--observe-only", "--formal")]
        for options in invalid:
            with self.subTest(options=options), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    self.arguments(*options)

    def test_formal_runs_preserve_failure_without_implicit_retries(self):
        for runs in (1, 3):
            with self.subTest(runs=runs):
                self.check_formal_runs(runs)

    def test_qml_diagnostics_fail_successful_startup_reports(self):
        self.check_formal_runs(1, log_text="ReferenceError: fixture")

    def check_formal_runs(self, runs, log_text=""):
        script = self.root / "tests/runners/verify-startup.py"
        script.parent.mkdir(parents=True, exist_ok=True)
        script.write_text("# fixture\n", encoding="utf-8")
        cache = self.root / "build/release/CMakeCache.txt"
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_text(f"YALAHOO_SELECTED_QT:PATH={self.root}\nCMAKE_BUILD_TYPE:STRING=Release\n", encoding="utf-8")
        executable = cache.parent / "tests/ui-tests"
        executable.parent.mkdir(exist_ok=True)
        executable.write_text("fixture", encoding="utf-8")
        options = ["--backend", "metal", "--formal", "--mode", "vsync", "--scenario", "night-wave",
                   "--output", str(self.root / f"result-{runs}")]
        if runs == 3:
            options += ["--runs", "3"]
        args = VERIFY.parse_arguments(options)
        calls = []

        def sample(command, **kwargs):
            calls.append(command)
            self.assertNotIn("QSG_NO_VSYNC", kwargs["env"])
            report = self.report(args)
            report["performanceGatePassed"] = bool(log_text) or len(calls) != 1
            kwargs["stdout"].write(log_text)
            output = Path(command[2])
            output.mkdir()
            (output / "report.json").write_text(json.dumps(report), encoding="utf-8")
            return SimpleNamespace(returncode=0)

        with patch.object(VERIFY, "ROOT", self.root), patch.object(VERIFY, "__file__", str(script)), \
                patch.object(VERIFY.platform, "platform", return_value="test-platform"), \
                patch.object(native_replay.subprocess, "run", side_effect=sample), \
                patch.dict(os.environ, {"QSG_NO_VSYNC": ""}), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(VERIFY.main(options), 1)
        manifest = json.loads((self.root / f"result-{runs}/manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(len(calls), runs)
        self.assertEqual([run["passed"] for run in manifest["runs"]], [False] + [True] * (runs - 1))
        self.assertTrue(manifest["completed"])
        self.assertFalse(manifest["formalAcceptancePassed"])
        self.assertFalse(manifest["passed"])


if __name__ == "__main__":
    unittest.main()
