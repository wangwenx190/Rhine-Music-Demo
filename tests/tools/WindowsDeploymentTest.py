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
    def test_selected_kit_license_bundle(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo licenses space-") as temporary:
            root = Path(temporary)
            shutil.copyfile(ROOT / "tools/deploy/qt-licenses.ps1", root / "qt-licenses.ps1")
            script = root / "copy.ps1"
            script.write_text("$ErrorActionPreference = 'Stop'\n"
                              ". (Join-Path $PSScriptRoot 'qt-licenses.ps1')\n"
                              "Copy-QtLicenses -Kit (Join-Path $PSScriptRoot 'Qt/6/kit') "
                              "-Destination (Join-Path $PSScriptRoot 'package/licenses/Qt')\n")
            kit = root / "Qt/6/kit"
            kit.mkdir(parents=True)
            destination = root / "package/licenses/Qt"
            destination.parent.mkdir(parents=True)
            for state in ("missing", "empty", "populated"):
                with self.subTest(state=state):
                    licenses = root / "Qt/Licenses"
                    if state != "missing":
                        licenses.mkdir(exist_ok=True)
                    if state == "populated":
                        (licenses / "media").mkdir()
                        (licenses / "LICENSE").write_bytes(b"Qt license")
                        (licenses / "media/FFmpeg.txt").write_bytes(b"Media license")
                    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(script)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode == 0, state == "populated",
                                     result.stdout + result.stderr)
                    if state == "populated":
                        self.assertEqual((destination / "LICENSE").read_bytes(), b"Qt license")
                        self.assertEqual((destination / "media/FFmpeg.txt").read_bytes(), b"Media license")
                    else:
                        self.assertFalse(destination.exists())

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
