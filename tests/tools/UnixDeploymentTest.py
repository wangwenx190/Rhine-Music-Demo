#!/usr/bin/env python3
"""Exercise strip and relocatable runtime imports on the native UNIX object format."""
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/deploy'))
import linux as linux_deploy
import macos_bundle


def run(*arguments, **kwargs):
    return subprocess.check_output([str(value) for value in arguments], text=True, **kwargs)


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
