#!/usr/bin/env python3
"""Host-independent package integrity, dependency parsing and shell argument checks."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/deploy'))
import linux_package as package
import linux as deploy


class LinuxDeploymentTest(unittest.TestCase):
    def test_inventory_rejects_tampering_and_permission_loss(self):
        with tempfile.TemporaryDirectory(prefix='yalahoo-中文 space-') as directory:
            root = Path(directory)
            binary = root / 'player'
            binary.write_bytes(b'product payload')
            binary.chmod(0o755)
            asset = root / 'qml/module/package.json'
            asset.parent.mkdir(parents=True)
            asset.write_text('{}')
            (root / 'player-link').symlink_to('player')
            (root / 'package.json').write_text(json.dumps(dict(system='Linux',
                configuration='Release', review=False, files=package.inventory(root))))
            package.verify_inventory(root)
            asset.write_text('{"modified":true}')
            with self.assertRaises(RuntimeError):
                package.verify_inventory(root)
            asset.write_text('{}')
            package.verify_inventory(root)
            binary.chmod(0o644)
            with self.assertRaises(RuntimeError):
                package.verify_inventory(root)
            binary.chmod(0o755)
            binary.write_bytes(b'changed payload')
            with self.assertRaises(RuntimeError):
                package.verify_inventory(root)

    def test_external_and_broken_symlinks_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            link = root / 'external'
            link.symlink_to('/etc/passwd')
            with self.assertRaises(RuntimeError):
                package.inventory(root)
            link.unlink()
            link.symlink_to('missing')
            with self.assertRaises(FileNotFoundError):
                package.inventory(root)

    def test_dependency_paths_with_spaces_and_missing_libraries(self):
        evidence = '''linux-vdso.so.1 (0x1234)
libQt6Core.so.6 => /tmp/中文 Qt/lib/libQt6Core.so.6 (0x1234)
/lib64/ld-linux-x86-64.so.2 (0x5678)'''
        with patch.object(package, 'run', return_value=evidence):
            paths = package.dependencies(Path('/tmp/player'), {})
        self.assertEqual(paths[0], Path('/tmp/中文 Qt/lib/libQt6Core.so.6').resolve())
        self.assertEqual(len(paths), 2)
        with patch.object(package, 'run', return_value='libQt6Gui.so.6 => not found'):
            with self.assertRaises(RuntimeError):
                package.dependencies(Path('/tmp/player'), {})
        with patch.object(package, 'run', return_value='not a dynamic executable'):
            with self.assertRaises(RuntimeError):
                package.dependencies(Path('/tmp/player'), {})

    def test_transitive_kit_closure_excludes_system_and_foreign_qt(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory).resolve()
            root, kit = base / 'package', base / 'kit'
            (root / 'bin').mkdir(parents=True)
            (root / 'lib').mkdir()
            (kit / 'lib').mkdir(parents=True)
            binary = root / 'bin/yalahoo'
            gui = kit / 'lib/libQt6Gui.so.6'
            support = kit / 'lib/libsupport.so.1'
            for path in (binary, gui, support):
                path.write_bytes(b'\x7fELFtest payload')

            def linked(path, environment):
                if path.name == 'yalahoo':
                    return [gui, Path('/usr/lib/libc.so.6')]
                if path.name == gui.name:
                    return [support]
                return []

            def tool(*args):
                return args[-1].name if args[1] == '--print-soname' else ''

            with patch.object(deploy, 'dependencies', side_effect=linked), \
                    patch.object(deploy, 'run', side_effect=tool):
                deploy.complete_runtime(root, kit)
            self.assertTrue((root / 'lib/libQt6Gui.so.6').is_file())
            self.assertTrue((root / 'lib/libsupport.so.1').is_file())
            self.assertFalse((root / 'lib/libc.so.6').exists())
            with patch.object(deploy, 'dependencies', return_value=[base / 'foreign/libQt6Core.so.6']):
                with self.assertRaises(RuntimeError):
                    deploy.complete_runtime(root, kit)

    def test_launcher_arguments_clean_paths_and_exec_status(self):
        with tempfile.TemporaryDirectory(prefix='yalahoo-中文 space-') as directory:
            root = Path(directory)
            (root / 'bin').mkdir()
            (root / 'lib').mkdir()
            (root / 'package.json').write_text('{}')
            (root / 'bin/qt.conf').write_text('[Paths]\nPrefix=..\n')
            launcher = root / 'yalahoo.sh'
            launcher.write_bytes((ROOT / 'tools/deploy/linux-launch.sh').read_bytes())
            launcher.chmod(0o755)
            binary = root / 'bin/yalahoo'
            binary.write_text('''#!/bin/sh
[ -z "${QT_PLUGIN_PATH+x}" ] || exit 90
[ -z "${QML_IMPORT_PATH+x}" ] || exit 91
[ "$XDG_DATA_HOME" = '/tmp/用户 data' ] || exit 92
[ "$QT_QPA_PLATFORM" = wayland ] || exit 93
[ "$QSG_RHI_BACKEND" = vulkan ] || exit 94
printf '%s\\n' "$LD_LIBRARY_PATH" "$@"
exit 37
''')
            binary.chmod(0o755)
            arguments = ['--music-root', '中文 music', 'literal;$HOME', '$(exit 99)', '', 'a"b']
            environment = dict(os.environ, QT_PLUGIN_PATH='/foreign/qt', QML_IMPORT_PATH='/foreign/qml',
                               XDG_DATA_HOME='/tmp/用户 data', QT_QPA_PLATFORM='wayland',
                               QSG_RHI_BACKEND='vulkan')
            result = subprocess.run([str(launcher), *arguments], cwd='/tmp', env=environment,
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 37, result.stderr)
            self.assertEqual(result.stdout.splitlines(), [str(root / 'lib'), *arguments])
            (root / 'package.json').unlink()
            result = subprocess.run([str(launcher)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)


if __name__ == '__main__':
    unittest.main()
