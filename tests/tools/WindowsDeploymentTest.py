"""Exercise PowerShell report, package inventory and Qt license validation with isolated fixtures."""
import json
import sys
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = Path(sys.argv.pop(1)) if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else None


@unittest.skipUnless(shutil.which("pwsh"), "Requires PowerShell 7.2+; target execution remains pending")
class WindowsDeploymentTest(unittest.TestCase):
    def test_public_scenarios_match_production_cli(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo scenario space-") as temporary:
            root = Path(temporary)
            script = root / "scenarios.ps1"
            helper = str(ROOT / "tools/deploy/product-report.ps1").replace("'", "''")
            script.write_text("$ErrorActionPreference = 'Stop'\n"
                              f". '{helper}'\n"
                              "$result = @{}\n"
                              "foreach ($scenario in @('help', 'demo', 'normal', 'deployment', "
                              "'browse', 'navigation', 'detail', 'overview')) {\n"
                              "$result[$scenario] = @(Get-ProductVerificationArguments "
                              "-Scenario $scenario -OutputDirectory $PSScriptRoot)\n}\n"
                              "$result | ConvertTo-Json\n")
            result = subprocess.run(['pwsh', '-NoProfile', '-File', str(script)],
                                    capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            scenarios = json.loads(result.stdout)
            self.assertEqual(scenarios['browse'], scenarios['deployment'])
            self.assertEqual(scenarios['browse'][0], '--verify-deployment')
            for scenario in ('navigation', 'detail', 'overview'):
                self.assertEqual(scenarios[scenario][0], '--verify-' + scenario)
                self.assertEqual(len(scenarios[scenario]), 2)
            self.assertTrue(Path(scenarios['browse'][1]).as_posix().endswith('product/report.json'))
            if PRODUCT:
                help_result = subprocess.run([str(PRODUCT), '--help'], capture_output=True,
                                             text=True, timeout=15)
                self.assertEqual(help_result.returncode, 0, help_result.stderr)
                for arguments in scenarios.values():
                    self.assertIn(arguments[0], help_result.stdout)

    def test_load_failure_retains_old_package_and_strict_process_protocol(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo staging space-") as temporary:
            root = Path(temporary)
            helper = str(ROOT / 'tools/deploy/product-report.ps1').replace("'", "''")
            for case in ('missing', 'malformed', 'false', 'string', 'exit', 'timeout', 'success'):
                with self.subTest(case=case):
                    directory = root / case
                    directory.mkdir()
                    package = directory / 'old package'
                    staging = directory / 'staging'
                    package.mkdir()
                    staging.mkdir()
                    (package / 'sentinel').write_bytes(b'old product')
                    (staging / 'sentinel').write_bytes(b'new product')
                    child = directory / 'child.ps1'
                    content = {'missing': '', 'malformed': '{broken', 'false': '{"passed":false}',
                               'string': '{"passed":"true"}'}.get(case, '{"passed":true}')
                    statements = []
                    if content:
                        statements.append("[IO.File]::WriteAllText((Join-Path $PSScriptRoot "
                                          f"'report.json'), '{content}')")
                    if case == 'exit':
                        statements.append('exit 9')
                    elif case == 'timeout':
                        statements.append('Start-Sleep -Seconds 60')
                    child.write_text('\n'.join(statements))
                    script = directory / 'publish.ps1'
                    timeout = 250 if case == 'timeout' else 15000
                    script.write_text("$ErrorActionPreference = 'Stop'\n"
                                      f". '{helper}'\n"
                                      "$start = [Diagnostics.ProcessStartInfo]::new()\n"
                                      "$start.FileName = (Get-Process -Id $PID).Path\n"
                                      "$start.ArgumentList.Add('-NoProfile')\n"
                                      "$start.ArgumentList.Add('-File')\n"
                                      "$start.ArgumentList.Add((Join-Path $PSScriptRoot 'child.ps1'))\n"
                                      "Publish-WindowsPackage -Staging (Join-Path $PSScriptRoot 'staging') "
                                      "-Output (Join-Path $PSScriptRoot 'old package') -Verify {\n"
                                      "Assert-ProductProbeProcess -StartInfo $start -OutputDirectory $PSScriptRoot "
                                      f"-ReportPath (Join-Path $PSScriptRoot 'report.json') -TimeoutMilliseconds {timeout}\n}}\n")
                    result = subprocess.run(['pwsh', '-NoProfile', '-File', str(script)],
                                            capture_output=True, text=True, timeout=25)
                    self.assertEqual(result.returncode == 0, case == 'success', result.stdout + result.stderr)
                    self.assertEqual((package / 'sentinel').read_bytes(),
                                     b'new product' if case == 'success' else b'old product')
                    self.assertEqual(staging.exists(), case != 'success')
                    self.assertFalse(list(directory.glob('old package-previous-*')))

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
