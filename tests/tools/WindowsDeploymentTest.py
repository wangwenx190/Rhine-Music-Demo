"""Execute the deployed PowerShell report validator against strict success/failure fixtures."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("pwsh"), "Requires PowerShell 7.2+; target execution remains pending")
class WindowsDeploymentTest(unittest.TestCase):
    def test_only_an_object_with_boolean_true_passes(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo report space-") as temporary:
            root = Path(temporary)
            shutil.copyfile(ROOT / "tools/deploy/product-report.ps1", root / "product-report.ps1")
            script = root / "verify.ps1"
            script.write_text("$ErrorActionPreference = 'Stop'\n"
                              ". (Join-Path $PSScriptRoot 'product-report.ps1')\n"
                              "Assert-ProductVerificationReport -Path (Join-Path $PSScriptRoot 'report.json')\n")
            report = root / "report.json"
            cases = [None, "{broken", "null", "[]", "{}", '[{"passed": true}]',
                     *[json.dumps(dict(passed=value)) for value in
                       (False, "false", "true", 0, 1, None, [True], dict(value=True))],
                     '{"passed": true}']
            for value in cases:
                with self.subTest(report=value):
                    report.unlink(missing_ok=True)
                    if value is not None:
                        report.write_text(value)
                    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(script)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode == 0, value == '{"passed": true}',
                                     result.stdout + result.stderr)


if __name__ == "__main__":
    if not shutil.which("pwsh"):
        print("PowerShell unavailable: deployment report regression remains pending")
        raise SystemExit(77)
    unittest.main()
