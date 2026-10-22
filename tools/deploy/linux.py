#!/usr/bin/env python3
"""Build and stage the latest native Qt Linux production product without system writes."""
import argparse
from collections import deque
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from qml_closure import production_qml
from package_support import copy_licenses, install_package
from linux_package import (clean_environment, dependencies, elf_files, inspect_package,
                           inventory, is_system, requires_bundle, run)

ROOT = Path(__file__).resolve().parents[2]


def complete_runtime(root, kit):
    # QML is deployed from the exact target imports by Qt's CMake API. Session
    # integrations are loaded dynamically and need explicit xcb/Wayland closure.
    categories = ('platforms', 'xcbglintegrations', 'wayland-graphics-integration-client',
                  'wayland-shell-integration', 'wayland-decoration-client',
                  'wayland-inputdevice-integration', 'platforminputcontexts',
                  'imageformats', 'iconengines', 'tls', 'multimedia')
    for category in categories:
        source = kit / 'plugins' / category
        if not source.is_dir():
            continue
        destination = root / 'plugins' / category
        destination.mkdir(parents=True, exist_ok=True)
        for path in source.glob('*.so'):
            if category == 'platforms' and path.name != 'libqxcb.so' and not path.name.startswith(
                    ('libqwayland', 'libqoffscreen')):
                continue
            if category == 'multimedia' and path.name != 'libffmpegmediaplugin.so':
                continue
            shutil.copy2(path, destination / path.name)
    # Qt may deploy optional host integrations. Keep only the two supported
    # desktop sessions and offscreen QPA for the non-GPU product loading probe.
    for path in (root / 'plugins/platforms').glob('*.so'):
        if path.name not in ('libqxcb.so', 'libqoffscreen.so') and not path.name.startswith('libqwayland'):
            path.unlink()
    environment = clean_environment(root)
    environment['LD_LIBRARY_PATH'] = str(root / 'lib') + ':' + str(kit / 'lib')
    visited = set()
    pending = deque(elf_files(root))
    while pending:
        path = pending.popleft()
        if path in visited:
            continue
        visited.add(path)
        for dependency in dependencies(path, environment):
            if dependency.is_relative_to(root):
                continue
            if requires_bundle(dependency) or (dependency.is_relative_to(kit)
                                               and not is_system(dependency)):
                if not dependency.is_relative_to(kit):
                    raise RuntimeError(f'Qt/media library resolved outside selected Kit: {dependency}')
                # Preserve SONAME links without copying the whole Kit directory.
                soname = run('patchelf', '--print-soname', dependency) or dependency.name
                if '/' in soname:
                    raise RuntimeError(f'Invalid library SONAME: {soname}')
                target = root / 'lib' / soname
                if not target.exists():
                    shutil.copy2(dependency, target)
                    pending.append(target)
    for path in elf_files(root):
        relative = os.path.relpath(root / 'lib', path.parent)
        run('patchelf', '--set-rpath', '$ORIGIN/' + relative, path)
        # Keep the dynamic symbol table required by Qt plugins and runtime loading.
        run('strip', '--strip-unneeded', path)


def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    if platform.system() != 'Linux':
        raise SystemExit('Linux deployment requires native Linux and its latest complete Qt Kit.')
    for tool in ('cmake', 'ninja', 'patchelf', 'readelf', 'ldd', 'strip'):
        if not shutil.which(tool):
            raise RuntimeError(f'Install the development tool first: {tool}')
    build = ROOT / 'build/linux-release'
    output = ROOT / 'out/linux'
    subprocess.run(['cmake', '--preset', 'qt-release', '-B', str(build),
                    '-DYALAHOO_REVIEW=OFF', '-DCMAKE_INSTALL_LIBDIR=lib'], cwd=ROOT, check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'Yalahoo'], check=True)
    metadata = json.loads((build / 'deployment.json').read_text())
    if metadata['system'] != 'Linux' or metadata['configuration'] != 'Release' or metadata['review']:
        raise RuntimeError('Only the native Linux production Release application may be packaged')
    kit = Path(metadata['qtKit']).resolve()
    if run(kit / 'bin/qmake', '-query', 'QT_VERSION') != metadata['qtVersion']:
        raise RuntimeError('Deployment must use the selected Qt Kit')
    qml = [str(path.relative_to(ROOT / 'qml'))
           for path in production_qml(ROOT, build, metadata)]
    (ROOT / 'out').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='linux-staging-', dir=ROOT / 'out') as temporary:
        staging = Path(temporary)
        environment = clean_environment(staging)
        # DESTDIR from a caller must not redirect the owned staging install.
        environment.pop('DESTDIR', None)
        subprocess.run([shutil.which('cmake'), '--install', str(build), '--prefix', str(staging)],
                       env=environment, check=True)
        (staging / 'lib').mkdir(exist_ok=True)
        complete_runtime(staging, kit)
        (staging / 'bin/qt.conf').write_text(
            '[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n')
        shutil.copy2(ROOT / 'tools/deploy/linux-launch.sh', staging / 'yalahoo.sh')
        copy_licenses(ROOT, kit, staging / 'licenses')
        for name in ('verify-linux.py', 'linux_package.py', 'package_support.py'):
            shutil.copyfile(ROOT / 'tools/deploy' / name, staging / name)
        closure = inspect_package(staging)
        (build / 'deployment-dependencies.json').write_text(json.dumps(closure, indent=2) + '\n')
        (staging / 'system-dependencies.json').write_text(json.dumps(
            dict(buildSystem=platform.platform(), dependencies=closure['systemDependencies']), indent=2) + '\n')
        (staging / 'README.md').write_text('''# yalahOO!!!

Run ./yalahoo.sh, or ./yalahoo.sh --demo. Copy the whole directory intact.
No Python, Node, browser, development Qt, or external ffmpeg executable is needed
by the player. Python/readelf/ldd are developer verification tools only.
Use python3 verify-linux.py --output /absolute/new/directory for hidden product
QML/resource/media loading. This does not render OpenGL or play audible audio.
System ABI, GPU drivers, X11/Wayland, Fontconfig and audio services remain OS
requirements; see system-dependencies.json and docs/platforms/linux.md in source.
No desktop registration, system package installation or driver/audio change is made.
''')
        (staging / 'package.json').write_text(json.dumps(dict(displayName='yalahOO!!!',
            system='Linux', qtVersion=metadata['qtVersion'], architecture=closure['architectures'],
            configuration='Release', review=False, files=inventory(staging)), indent=2) + '\n')
        subprocess.run([sys.executable, str(staging / 'verify-linux.py'),
                        '--package', str(staging)], check=True)
        install_package(staging, output)
    print(f'Linux production package: {output}')


if __name__ == '__main__':
    main()
