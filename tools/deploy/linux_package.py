#!/usr/bin/env python3
"""Inspect a relocatable Linux product; system ABI/driver libraries remain external."""
import json
import os
from pathlib import Path
import re
import subprocess
from package_support import digest


def run(*args, env=None):
    return subprocess.run([str(value) for value in args], env=env, check=True,
                          capture_output=True, text=True).stdout.strip()


def inventory(root):
    root = root.resolve()
    files = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            target = os.readlink(path)
            if Path(target).is_absolute() or not path.resolve(strict=True).is_relative_to(root):
                raise RuntimeError(f'External package symlink: {path}')
            files[str(path.relative_to(root))] = dict(symlink=target)
        elif path.is_file() and path != root / 'package.json':
            files[str(path.relative_to(root))] = dict(sha256=digest(path), bytes=path.stat().st_size,
                                                    mode=path.stat().st_mode & 0o777)
    return files


def verify_inventory(root):
    metadata = json.loads((root / 'package.json').read_text())
    if metadata['files'] != inventory(root):
        raise RuntimeError('Package inventory, permissions or symlinks changed')
    if metadata['system'] != 'Linux' or metadata['configuration'] != 'Release' or metadata['review']:
        raise RuntimeError('Expected native production Linux Release package')
    return metadata


def elf_files(root):
    for path in sorted(root.rglob('*')):
        if path.is_file() and not path.is_symlink():
            with path.open('rb') as stream:
                if stream.read(4) == b'\x7fELF':
                    yield path


def clean_environment(root):
    environment = {key: value for key, value in os.environ.items() if not key.startswith(
        ('QT_', 'QML', 'QSG_', 'LD_', 'YALAHOO_', 'QCPAINTER_'))}
    environment.update(PATH='/usr/bin:/bin', LD_LIBRARY_PATH=str(root / 'lib'))
    return environment


def dependencies(path, environment):
    output = run('ldd', path, env=environment)
    if 'not found' in output:
        raise RuntimeError(f'Missing ELF dependency of {path}:\n{output}')
    paths = []
    for line in output.splitlines():
        # Paths may contain spaces. The loader address is the only delimiter.
        match = re.search(r'(?:=>\s+)?(/.+?)\s+\(0x[0-9a-fA-F]+\)', line)
        if match:
            paths.append(Path(match.group(1).strip()).resolve())
    if not paths:
        raise RuntimeError(f'No dynamic dependency evidence for {path}: {output}')
    return paths


def is_system(path):
    return any(path.is_relative_to(Path(prefix).resolve())
               for prefix in ('/lib', '/lib64', '/usr/lib', '/usr/lib64'))


def requires_bundle(path):
    # Driver/loader/session/Fontconfig/FreeType/audio ABI remain OS dependencies.
    # Qt and the media libraries distributed with Qt must never come from host Qt.
    return path.name.startswith(('libQt6', 'libavcodec', 'libavformat', 'libavutil',
                                 'libswresample', 'libswscale', 'libavfilter', 'libavdevice'))


def inspect_package(root):
    root = root.resolve()
    required = ['bin/yalahoo', 'bin/qt.conf', 'yalahoo.sh', 'plugins/platforms/libqxcb.so',
                'plugins/multimedia/libffmpegmediaplugin.so', 'plugins/platforms/libqoffscreen.so',
                'qml/QtQuick/qmldir', 'qml/QtQuick/Templates/qmldir',
                'qml/QtQuick/Dialogs/qmldir', 'qml/QtMultimedia/qmldir',
                'share/applications/yalahoo.desktop',
                'share/icons/hicolor/scalable/apps/yalahoo.svg', 'licenses']
    if any(not (root / value).exists() for value in required):
        raise RuntimeError('Missing product component: ' + ', '.join(
            value for value in required if not (root / value).exists()))
    if not list((root / 'plugins/platforms').glob('libqwayland*.so')):
        raise RuntimeError('The selected Kit must provide Wayland as well as xcb')
    if not list((root / 'lib').glob('libQt6CanvasPainter.so*')):
        raise RuntimeError('Qt CanvasPainter runtime is missing')
    expected = '[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n'
    if (root / 'bin/qt.conf').read_text() != expected:
        raise RuntimeError('Non-relocatable Qt search paths')
    environment = clean_environment(root)
    evidence = []
    system = set()
    machines = set()
    for path in elf_files(root):
        header = run('readelf', '-h', path)
        machine = re.search(r'^\s*Machine:\s*(.+)$', header, re.M)
        if not machine:
            raise RuntimeError(f'ELF architecture not reported: {path}')
        machines.add(machine.group(1))
        dynamic = run('readelf', '-d', path)
        for entry in re.findall(r'\((?:RPATH|RUNPATH)\).*?\[(.*?)\]', dynamic):
            for search in entry.split(':'):
                if not search.startswith('$ORIGIN'):
                    raise RuntimeError(f'External ELF runtime path: {path}: {search}')
                resolved = Path(search.replace('$ORIGIN', str(path.parent))).resolve()
                if not resolved.is_relative_to(root):
                    raise RuntimeError(f'Escaping ELF runtime path: {path}: {search}')
        linked = dependencies(path, environment)
        for dependency in linked:
            if not dependency.is_relative_to(root):
                if requires_bundle(dependency) or not is_system(dependency):
                    raise RuntimeError(f'External runtime dependency: {path}: {dependency}')
                system.add(str(dependency))
        evidence.append(dict(path=str(path.relative_to(root)), machine=machine.group(1),
                             dependencies=[str(value) for value in linked]))
    if len(machines) != 1:
        raise RuntimeError(f'ELF architecture mismatch: {machines}')
    return dict(elf=evidence, architectures=sorted(machines), systemDependencies=sorted(system),
                systemAbiScope='Native build distribution/ABI; no universal Linux binary claim')
