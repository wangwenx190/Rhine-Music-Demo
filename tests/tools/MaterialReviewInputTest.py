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


if __name__ == "__main__":
    unittest.main()
