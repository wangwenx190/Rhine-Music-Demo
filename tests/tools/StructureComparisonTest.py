"""Reject incomplete or falsely successful source-layout audits with deterministic reports."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/migration/compare-ui-structure.py"
SPEC = importlib.util.spec_from_file_location("structure_comparison", SCRIPT)
COMPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPARE)


class StructureComparisonTest(unittest.TestCase):
    def reports(self, component):
        state = dict(width=390, height=844, night=False, dpr=1.5)
        fields = {part: dict.fromkeys(keys, 10) for part, keys in COMPARE.FIELDS[component].items()}
        source = dict(captures=[dict(state)])
        native = dict(passed=True, captures=[dict(state)])
        if component == "presentation":
            source["captures"][0]["fields"] = deepcopy(fields)
            native["captures"][0]["fields"] = deepcopy(fields)
        else:
            source["captures"][0]["variants"] = [dict(expanded=v, **deepcopy(fields)) for v in (False, True)]
            source["admissions"] = [dict(state, expanded=-1, backend="metal", lanes=[0])]
            native.update(retired=True, backend="metal")
            native["captures"][0].update(expanded=-1, structures=[dict(lane=0, expanded=False, **deepcopy(fields))])
        return native, source

    def captures(self, component, native):
        return native["captures"]

    def box(self, component, native):
        capture = self.captures(component, native)[0]
        return capture["fields"] if component == "presentation" else capture["structures"][0]

    def test_minimal_declared_scope_and_original_pixel_tolerance(self):
        for component in COMPARE.FIELDS:
            with self.subTest(component=component):
                native, source = self.reports(component)
                first_part = next(iter(COMPARE.FIELDS[component]))
                self.box(component, native)[first_part]["x"] += 1
                result = COMPARE.compare(component, native, source)
                self.assertTrue(result["passed"])
                self.assertEqual(result["states"], 1)
                self.box(component, native)[first_part]["x"] += .001
                self.assertFalse(COMPARE.compare(component, native, source)["passed"])

    def test_empty_duplicate_missing_or_nonboolean_success_cannot_pass(self):
        for component in COMPARE.FIELDS:
            for defect in ("empty", "duplicate-native", "duplicate-source", "missing", "truthy", "warnings"):
                with self.subTest(component=component, defect=defect):
                    native, source = self.reports(component)
                    if defect == "empty":
                        self.captures(component, native).clear()
                    elif defect == "duplicate-native":
                        self.captures(component, native).append(deepcopy(self.captures(component, native)[0]))
                    elif defect == "duplicate-source":
                        source["captures"].append(deepcopy(source["captures"][0]))
                    elif defect == "missing":
                        source["captures"].append(dict(deepcopy(source["captures"][0]), night=True))
                    elif defect == "truthy":
                        native["passed"] = "true"
                    else:
                        native["qmlWarnings"] = ["ReferenceError: fixture"]
                    with self.assertRaises(ValueError):
                        COMPARE.compare(component, native, source)

    def test_nonfinite_or_nonnumeric_geometry_and_dpr_fail(self):
        for component in COMPARE.FIELDS:
            for bad in (float("nan"), float("inf"), True, "10"):
                with self.subTest(component=component, bad=bad):
                    native, source = self.reports(component)
                    self.box(component, native)[next(iter(COMPARE.FIELDS[component]))]["x"] = bad
                    with self.assertRaises(ValueError):
                        COMPARE.compare(component, native, source)
                    native, source = self.reports(component)
                    self.captures(component, native)[0]["dpr"] = bad
                    with self.assertRaises(ValueError):
                        COMPARE.compare(component, native, source)

    def test_overview_requires_retirement_unique_admissions_and_real_boxes(self):
        for defect in ("retirement", "admission", "lanes", "boxes"):
            with self.subTest(defect=defect):
                native, source = self.reports("overview")
                if defect == "retirement":
                    native["retired"] = 1
                elif defect == "admission":
                    source["admissions"].append(deepcopy(source["admissions"][0]))
                elif defect == "lanes":
                    native["captures"][0]["structures"] *= 2
                else:
                    native["captures"][0]["structures"] = []
                    source["admissions"][0]["lanes"] = []
                with self.assertRaises(ValueError):
                    COMPARE.compare("overview", native, source)

    def test_overview_lane_types_and_expansion_must_match_the_declared_state(self):
        for bad in (False, -1, "0"):
            for target in ("native", "source"):
                with self.subTest(lane=bad, target=target):
                    native, source = self.reports("overview")
                    if target == "native":
                        native["captures"][0]["structures"][0]["lane"] = bad
                    else:
                        source["admissions"][0]["lanes"] = [bad]
                    with self.assertRaises(ValueError):
                        COMPARE.compare("overview", native, source)
        native, source = self.reports("overview")
        native["captures"][0]["structures"][0]["expanded"] = True
        with self.assertRaises(ValueError):
            COMPARE.compare("overview", native, source)

    def test_reports_require_objects(self):
        for bad in (None, [], True):
            native, source = self.reports("presentation")
            with self.subTest(report=bad), self.assertRaises(ValueError):
                COMPARE.compare("presentation", bad, source)
            with self.subTest(source=bad), self.assertRaises(ValueError):
                COMPARE.compare("presentation", native, bad)

    def test_cli_persists_failure_and_preserves_previous_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            native, source, output = (root / name for name in ("native.json", "source.json", "report.json"))
            native.write_text('{broken', encoding="utf-8")
            source.write_text('{}', encoding="utf-8")
            command = [sys.executable, str(SCRIPT), "overview", str(native), str(source), str(output)]
            result = subprocess.run(command, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 1)
            self.assertFalse(json.loads(output.read_text())["passed"])
            before = output.read_bytes()
            self.assertEqual(subprocess.run(command, capture_output=True, timeout=5).returncode, 2)
            self.assertEqual(output.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
