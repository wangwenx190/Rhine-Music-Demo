"""Reject a successful render-tool result when a required PNG cannot be written."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class RequiredImageOutputTest(unittest.TestCase):
    tool = None

    def test_required_render_output_failure_reaches_the_exit_status(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo required output-") as directory:
            output = Path(directory)
            # The first ordinary capture succeeds on the GPU, but its PNG path is a directory.
            (output / "isolated-1-1.png").mkdir()
            result = subprocess.run([str(self.tool), str(output), "--isolated-only"],
                                    capture_output=True, text=True, timeout=30)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Cannot save required output:", result.stderr)
            report = output / "runtime.json"
            if report.exists():
                self.assertIs(json.loads(report.read_text()).get("passed"), False)


if __name__ == "__main__":
    RequiredImageOutputTest.tool = Path(sys.argv.pop(1)).resolve()
    unittest.main()
