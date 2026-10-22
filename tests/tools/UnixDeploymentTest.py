#!/usr/bin/env python3
"""Exercise strip and relocatable runtime imports on the native UNIX object format."""
import os
import json
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/deploy'))
import linux as linux_deploy
import linux_package
import macos_bundle
import package_support


def run(*arguments, **kwargs):
    return subprocess.check_output([str(value) for value in arguments], text=True, **kwargs)


class PackageSupportTest(unittest.TestCase):
    def test_environment_isolation_is_shared_without_mutating_the_parent(self):
        polluted = dict(QT_PLUGIN_PATH='foreign', QML_IMPORT_PATH='foreign', QSG_RHI_BACKEND='foreign',
                        YALAHOO_VERIFY_FOREGROUND='1', QCPAINTER_DEBUG_COLLECT='1',
                        LD_PRELOAD='foreign', DYLD_INSERT_LIBRARIES='foreign', KEEP='original')
        with patch.dict(os.environ, polluted, clear=True):
            for prefix in ('LD_', 'DYLD_'):
                environment = package_support.isolated_environment(prefix)
                self.assertEqual(environment['KEEP'], 'original')
                self.assertFalse(any(key.startswith(('QT_', 'QML', 'QSG_', 'YALAHOO_',
                                                     'QCPAINTER_', prefix)) for key in environment))
            self.assertEqual(dict(os.environ), polluted)

    def test_inventory_requires_a_boolean_production_marker_on_both_platforms(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'player').write_bytes(b'anonymous product')
            for module in (linux_package, macos_bundle):
                for marker in (None, 0, '', [], 'false', True):
                    with self.subTest(module=module.__name__, marker=marker):
                        metadata = dict(configuration='Release', system='Linux', review=marker,
                                        files=module.inventory(root))
                        (root / 'package.json').write_text(json.dumps(metadata))
                        with self.assertRaises(RuntimeError):
                            module.verify_inventory(root)

    def test_probe_records_launch_failure_without_losing_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            probe = package_support.run_probe([output / 'missing-player'], output, os.environ)
            self.assertFalse(probe.passed)
            self.assertFalse(probe.timed_out)
            self.assertNotEqual(probe.exit_code, 0)
            self.assertEqual(probe.report, {})
            self.assertIn('missing-player', (output / 'stderr.txt').read_text())

    def test_inventory_rejects_nonrelocatable_and_dangling_links_on_both_platforms(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            (root / "player").write_bytes(b"anonymous product")
            link = root / "link"
            for module in (linux_package, macos_bundle):
                for target in (str(root / "player"), "missing", "../outside"):
                    with self.subTest(module=module.__name__, target=target):
                        link.symlink_to(target)
                        try:
                            with self.assertRaises((RuntimeError, FileNotFoundError)):
                                module.inventory(root)
                        finally:
                            link.unlink()
                link.symlink_to("player")
                module.inventory(root)
                link.unlink()

    def test_inventory_excludes_only_root_metadata_and_keeps_platform_permissions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            binary = root / "player"
            binary.write_bytes(b"anonymous product")
            binary.chmod(0o755)
            (root / "qml").mkdir()
            (root / "qml/package.json").write_text("{}")
            (root / "package.json").write_text("{}")
            linux = linux_package.inventory(root)
            macos = {entry['path']: entry for entry in macos_bundle.inventory(root)}
            self.assertEqual(set(linux), {"player", "qml/package.json"})
            self.assertEqual(set(macos), set(linux))
            self.assertEqual(linux['player']['mode'], 0o755)
            self.assertTrue(macos['player']['executable'])
            self.assertEqual(linux['player']['sha256'], macos['player']['sha256'])
            metadata = dict(configuration='Release', review=False, files=list(macos.values()))
            (root / 'package.json').write_text(json.dumps(metadata))
            macos_bundle.verify_inventory(root)
            binary.chmod(0o644)
            with self.assertRaises(RuntimeError):
                macos_bundle.verify_inventory(root)

    def test_license_copy_uses_kit_or_ancestor_and_preserves_all_attribution(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ('licenses/NOTICE', 'fonts/NOTICE.txt', 'fonts/MiSans-license.pdf', 'audio/README.md'):
                path = root / 'resources' / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            kit = root / 'qt/kit'
            kit.mkdir(parents=True)
            for index, location in enumerate((kit, kit.parent)):
                licenses = location / 'Licenses'
                licenses.mkdir()
                (licenses / 'Qt.txt').write_text('Qt attribution')
                destination = root / f'output-{index}'
                package_support.copy_licenses(root, kit, destination)
                self.assertEqual({str(p.relative_to(destination)) for p in destination.rglob('*') if p.is_file()},
                                 {'project/NOTICE', 'NOTICE.txt', 'MiSans-license.pdf', 'audio.md', 'Qt/Qt.txt'})
                self.assertEqual((destination / 'Qt/Qt.txt').read_text(), 'Qt attribution')
                shutil.rmtree(licenses)
            with self.assertRaisesRegex(RuntimeError, 'license directory is missing'):
                package_support.copy_licenses(root, kit, root / 'missing')

    def test_verification_output_preserves_package_and_previous_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = root / 'package'
            package.mkdir()
            for path in (package, package / 'new-result'):
                with self.assertRaisesRegex(RuntimeError, 'outside the package'):
                    package_support.verification_output(package, path, 'unused-')
            output = package_support.verification_output(package, root / 'evidence', 'unused-')
            (output / 'prior').write_text('preserve')
            with self.assertRaises(FileExistsError):
                package_support.verification_output(package, output, 'unused-')
            self.assertEqual((output / 'prior').read_text(), 'preserve')
            self.assertEqual(list(package.iterdir()), [])

    def test_install_replaces_whole_package_and_restores_on_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            output, staging = base / "product", base / "staging"
            output.mkdir()
            (output / "old").write_text("old")
            staging.mkdir()
            (staging / "new").write_text("new")
            rename = Path.rename

            def fail_install(path, target):
                if path == staging:
                    raise OSError("fixture installation failure")
                return rename(path, target)

            with patch.object(Path, "rename", fail_install), self.assertRaises(OSError):
                package_support.install_package(staging, output)
            self.assertEqual((output / "old").read_text(), "old")
            self.assertTrue((staging / "new").is_file())
            self.assertEqual(set(base.iterdir()), {staging, output})
            package_support.install_package(staging, output)
            self.assertEqual(set(output.iterdir()), {output / "new"})
            self.assertEqual(set(base.iterdir()), {output})

    def test_linux_package_requires_native_media_without_an_unused_qml_module(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            files = ["bin/yalahoo", "yalahoo.sh", "plugins/platforms/libqxcb.so",
                     "plugins/platforms/libqwayland-generic.so", "plugins/platforms/libqoffscreen.so",
                     "plugins/multimedia/libffmpegmediaplugin.so", "lib/libQt6CanvasPainter.so.6",
                     "qml/QtQuick/qmldir", "qml/QtQuick/Templates/qmldir", "qml/QtQuick/Dialogs/qmldir",
                     "share/applications/yalahoo.desktop", "share/icons/hicolor/scalable/apps/yalahoo.svg"]
            for name in files:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture component")
            (root / "licenses").mkdir()
            (root / "bin/qt.conf").write_text(
                "[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n")
            with patch.object(linux_package, "elf_files", return_value=[root / "bin/yalahoo"]), \
                    patch.object(linux_package, "dependencies", return_value=[]), \
                    patch.object(linux_package, "run", side_effect=lambda *a: "Machine: AArch64" if "-h" in a else ""):
                report = linux_package.inspect_package(root)
                self.assertEqual(report["architectures"], ["AArch64"])
                self.assertFalse((root / "qml/QtMultimedia").exists())
                (root / "plugins/multimedia/libffmpegmediaplugin.so").unlink()
                with self.assertRaisesRegex(RuntimeError, "Missing product component"):
                    linux_package.inspect_package(root)

    def test_macos_package_requires_native_media_without_an_unused_qml_module(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory).resolve() / "yalahoo.app"
            contents = bundle / "Contents"
            files = ["MacOS/yalahoo", "Resources/yalahoo.icns", "PlugIns/platforms/libqcocoa.dylib",
                     "PlugIns/imageformats/libqsvg.dylib", "PlugIns/iconengines/libqsvgicon.dylib",
                     "PlugIns/multimedia/libffmpegmediaplugin.dylib", "Resources/qml/QtQuick/qmldir",
                     "Resources/qml/QtQuick/Templates/qmldir", "Resources/qml/QtQuick/Dialogs/qmldir"]
            files += [f"Frameworks/Qt{name}.framework/Qt{name}"
                      for name in ("Core", "Gui", "Qml", "Quick", "Multimedia", "CanvasPainter")]
            for name in files:
                path = contents / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture component")
            identity = dict(CFBundleExecutable="yalahoo", CFBundleIdentifier="yalahoo.music",
                            CFBundleDisplayName="yalahOO!!!", CFBundleName="yalahOO!!!",
                            CFBundlePackageType="APPL", CFBundleIconFile="yalahoo.icns",
                            NSHighResolutionCapable=True, LSMinimumSystemVersion="13.0")
            (contents / "Info.plist").write_bytes(plistlib.dumps(identity))
            with patch.object(macos_bundle, "macho", side_effect=lambda p: p == contents / "MacOS/yalahoo"), \
                    patch.object(macos_bundle, "load_commands", return_value=[
                        dict(minimumMacOS="13.0", rpaths=[], dependencies=[])]), \
                    patch.object(macos_bundle, "run", return_value="arm64"):
                report = macos_bundle.inspect_bundle(bundle)
                self.assertEqual(report["architectures"], ["arm64"])
                self.assertFalse((contents / "Resources/qml/QtMultimedia").exists())
                (contents / "Frameworks/QtMultimedia.framework/QtMultimedia").unlink()
                with self.assertRaisesRegex(RuntimeError, "Missing deployed component"):
                    macos_bundle.inspect_bundle(bundle)

    def test_probe_records_process_failure_timeout_and_invalid_report(self):
        with tempfile.TemporaryDirectory(prefix="yalahoo probe space-") as temporary:
            base = Path(temporary)
            script = base / "probe.py"
            cases = [('{"passed": true}', 0, False), ('{broken', 0, False),
                     ('[]', 0, False), ('{"passed": true}', 7, False), ('', 0, True),
                     ('{"passed": "true"}', 0, False), ('{"passed": 1}', 0, False)]
            for index, (report, code, timeout) in enumerate(cases):
                output = base / str(index)
                output.mkdir()
                script.write_text("import pathlib, sys, time\n"
                                  f"pathlib.Path(sys.argv[2]).write_text({report!r})\n"
                                  + ("time.sleep(30)\n" if timeout else f"sys.exit({code})\n"))
                probe = package_support.run_probe([sys.executable, script], output, os.environ,
                                                   timeout=.1 if timeout else 5)
                self.assertEqual(probe.timed_out, timeout)
                self.assertEqual(probe.passed, report == '{"passed": true}' and code == 0 and not timeout)
                if not timeout:
                    self.assertEqual(probe.exit_code, code)
                if report.startswith('{"passed"'):
                    self.assertTrue(probe.report["passed"])
                else:
                    self.assertEqual(probe.report, {})


class UnixDeploymentTest(unittest.TestCase):
    @unittest.skipUnless(platform.system() == 'Darwin', 'requires native Mach-O tools')
    def test_macho_strip_preserves_dynamic_exports_after_relocation(self):
        with tempfile.TemporaryDirectory(prefix='yalahoo-unix-') as temporary:
            base = Path(temporary)
            bundle = base / 'stage/yalahoo.app'
            libraries = bundle / 'Contents/Frameworks'
            libraries.mkdir(parents=True)
            executable = bundle / 'Contents/MacOS/yalahoo'
            executable.parent.mkdir()
            library = libraries / 'libexport-probe.dylib'
            source = base / 'library.cpp'
            source.write_text('extern "C" int exported_probe() { return 42; }\n')
            main = base / 'main.cpp'
            main.write_text('extern "C" int exported_probe();\n'
                            'int main() { return exported_probe() == 42 ? 0 : 1; }\n')
            compiler = run('xcrun', '--find', 'clang++').strip()
            sdk = run('xcrun', '--show-sdk-path').strip()
            run(compiler, '-isysroot', sdk, '-g', '-dynamiclib', source, '-o', library,
                '-Wl,-install_name,@rpath/libexport-probe.dylib')
            run(compiler, '-isysroot', sdk, '-g', main, library, '-o', executable,
                '-Wl,-rpath,@executable_path/../Frameworks', '-Wl,-rpath,' + str(base / 'old-kit'))
            (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps({}))
            normalization = macos_bundle.normalize_bundle(bundle)
            self.assertEqual(len(normalization['removedExternalRpaths']), 1)
            macos_bundle.strip_bundle(bundle)
            self.assertFalse(list(bundle.rglob('*.dSYM')))
            self.assertIn('_exported_probe', run('/usr/bin/nm', '-gU', library))
            run('/usr/bin/codesign', '--force', '--deep', '--sign', '-', bundle)
            relocated = base / 'relocated \u4e2d\u6587 space' / bundle.name
            relocated.parent.mkdir()
            bundle.rename(relocated)
            environment = {key: value for key, value in os.environ.items()
                           if not key.startswith('DYLD_')}
            run(relocated / 'Contents/MacOS/yalahoo', cwd=base, env=environment)
            run('/usr/bin/codesign', '--verify', '--deep', '--strict', relocated)

    @unittest.skipUnless(platform.system() == 'Linux', 'requires native ELF tools')
    def test_elf_strip_preserves_exports_and_origin_imports_without_kit(self):
        for tool in ('c++', 'patchelf', 'readelf', 'strip', 'ldd'):
            self.assertIsNotNone(shutil.which(tool), f'missing native deployment tool: {tool}')
        with tempfile.TemporaryDirectory(prefix='yalahoo-unix-') as temporary:
            base = Path(temporary).resolve()
            root, kit = base / 'stage', base / 'kit'
            (root / 'bin').mkdir(parents=True)
            (root / 'lib').mkdir()
            (kit / 'lib').mkdir(parents=True)
            source = base / 'library.cpp'
            source.write_text('extern "C" int exported_probe() { return 42; }\n')
            main = base / 'main.cpp'
            main.write_text('extern "C" int exported_probe();\n'
                            'int main() { return exported_probe() == 42 ? 0 : 1; }\n')
            library = kit / 'lib/libQt6ExportProbe.so.6'
            run('c++', '-g', '-shared', '-fPIC', source, '-o', library,
                '-Wl,-soname,' + library.name)
            run('c++', '-g', main, library, '-o', root / 'bin/yalahoo',
                '-Wl,-rpath,' + str(kit / 'lib'))
            linux_deploy.complete_runtime(root, kit)
            for binary in (root / 'bin/yalahoo', root / 'lib' / library.name):
                sections = run('readelf', '--sections', '--wide', binary)
                self.assertNotIn('.symtab', sections)
                self.assertNotIn('.debug_info', sections)
                self.assertIn('.dynsym', sections)
                self.assertTrue(run('patchelf', '--print-rpath', binary).startswith('$ORIGIN/'))
            self.assertIn('exported_probe', run('readelf', '--dyn-syms', root / 'lib' / library.name))
            kit.rename(base / 'unavailable-kit')
            relocated = base / 'relocated \u4e2d\u6587 space'
            root.rename(relocated)
            environment = {key: value for key, value in os.environ.items()
                           if not key.startswith(('LD_', 'QT_', 'QML'))}
            run(relocated / 'bin/yalahoo', cwd=base, env=environment)


if __name__ == '__main__':
    unittest.main()
