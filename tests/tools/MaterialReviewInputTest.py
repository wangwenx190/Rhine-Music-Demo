"""Malformed fixtures must fail through the Review exit protocol before creating a window."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXECUTABLE = sys.argv.pop(1)


class MaterialReviewInputTest(unittest.TestCase):
    def test_invalid_dimensions(self):
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                           YALAHOO_VERIFY_VISIBLE="0", YALAHOO_VERIFY_FOREGROUND="0")
        with tempfile.TemporaryDirectory(prefix="material input ") as directory:
            fixture = Path(directory) / "fixture.json"
            report = Path(directory) / "report.json"
            for key, value in [("width", 2147483647), ("height", "720"),
                               ("width", 0), ("height", 1.5)]:
                with self.subTest(key=key, value=value):
                    fixture.write_text(json.dumps({"lighting": True, "cases": [
                        {"position": [0, 0, 1], "parameters": [0, 0, 0],
                         "projections": [0, 0, 0, 0, 0], key: value}]}), encoding="utf-8")
                    result = subprocess.run([EXECUTABLE, "--verify-materials", str(fixture),
                                             "--material-report", str(report)], env=environment,
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 12, result.stderr.decode())
                    self.assertFalse(report.exists())

    def test_schema_surfaces_projection_and_resource_limits(self):
        source = json.loads((Path(__file__).resolve().parents[1] /
                             'fixtures/lighting/visual.json').read_text())
        original = source['cases'][0]
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                           YALAHOO_VERIFY_VISIBLE="0", YALAHOO_VERIFY_FOREGROUND="0")
        with tempfile.TemporaryDirectory(prefix="material schema ") as directory:
            fixture = Path(directory) / 'fixture.json'
            report = Path(directory) / 'report.json'
            for mutation in ('duplicate', 'unknown', 'missing', 'projection', 'cases', 'bytes'):
                with self.subTest(mutation=mutation):
                    state = json.loads(json.dumps(original))
                    if mutation == 'duplicate':
                        state['parameters'][1] = state['parameters'][0]
                    elif mutation == 'unknown':
                        state['parameters'][0]['surface'] = 'Unknown'
                    elif mutation == 'missing':
                        del state['parameters'][0]['roughness']
                    elif mutation == 'projection':
                        state['projections'][0]['world'][0] = 1e300
                    fixture.write_text(json.dumps(dict(lighting=True, cases=[state] *
                                                       (257 if mutation == 'cases' else 1))))
                    if mutation == 'bytes':
                        with fixture.open('r+b') as stream:
                            stream.truncate(4 * 1024 * 1024 + 1)
                    result = subprocess.run([EXECUTABLE, '--verify-materials', str(fixture),
                                             '--material-report', str(report)], env=environment,
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 12, result.stderr.decode())
                    self.assertFalse(report.exists())


if __name__ == "__main__":
    unittest.main()
