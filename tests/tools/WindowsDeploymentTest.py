"""Exercise PowerShell report, package inventory and Qt license validation with isolated fixtures."""
import json
import hashlib
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

    def test_inventory_requires_complete_unique_paths_sizes_and_hashes(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo inventory space-") as temporary:
            root = Path(temporary)
            shutil.copyfile(ROOT / "tools/deploy/product-report.ps1", root / "product-report.ps1")
            script = root / "verify.ps1"
            script.write_text("$ErrorActionPreference = 'Stop'\n"
                              ". (Join-Path $PSScriptRoot 'product-report.ps1')\n"
                              "$null = Assert-PackageInventory -PackageDirectory "
                              "(Join-Path $PSScriptRoot 'package')\n")
            package = root / 'package'
            for name in ('yalahoo.exe', 'plugins/media.dll', 'qml/package.json'):
                path = package / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'anonymous product')
            entries = [dict(path=str(path.relative_to(package)).replace('\\', '/'),
                            bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                       for path in package.rglob('*') if path.is_file()]
            valid = dict(configuration='Release', review=False, files=entries)
            cases = {'valid': valid, 'null': None, 'array': [], 'empty': dict(valid, files=[]),
                     'not-array': dict(valid, files=entries[0]),
                     'missing-entry': dict(valid, files=entries[:-1]),
                     'duplicate': dict(valid, files=[entries[0], entries[0], entries[2]]),
                     'case-duplicate': dict(valid, files=[*entries, dict(entries[0], path=entries[0]['path'].upper())]),
                     'review-string': dict(valid, review='false')}
            for field, values in {'path': ('../outside', '/absolute', 'plugins/../yalahoo.exe',
                                          './yalahoo.exe', 'plugins//media.dll', 'plugins/media.dll/',
                                          'C:/outside', 'plugins\\media.dll', ''),
                                  'bytes': (-1, 1.5, '17', 0),
                                  'sha256': ('broken', '0' * 64, 0)}.items():
                for index, value in enumerate(values):
                    cases[f'{field}-{index}'] = dict(valid, files=[dict(entries[0], **{field: value}), *entries[1:]])
            for name, manifest in cases.items():
                with self.subTest(case=name):
                    (package / 'package.json').write_text(json.dumps(manifest))
                    result = subprocess.run(['pwsh', '-NoProfile', '-File', str(script)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode == 0, name == 'valid', result.stdout + result.stderr)
            (package / 'package.json').write_text(json.dumps(valid))
            for name in ('extra.dll', '.hidden.dll'):
                with self.subTest(extra=name):
                    extra = package / name
                    extra.write_bytes(b'extra plugin')
                    result = subprocess.run(['pwsh', '-NoProfile', '-File', str(script)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                    extra.unlink()
            (package / 'yalahoo.exe').unlink()
            result = subprocess.run(['pwsh', '-NoProfile', '-File', str(script)],
                                    capture_output=True, text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

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
