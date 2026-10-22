"""Normalize deployment paths/metadata and inspect a self-contained macOS bundle."""
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
from package_support import package_inventory


MACHO = {bytes.fromhex(value) for value in
         ('feedface', 'cefaedfe', 'feedfacf', 'cffaedfe', 'cafebabe', 'bebafeca', 'cafebabf')}


def run(*arguments):
    return subprocess.check_output([str(value) for value in arguments], text=True).strip()


def inventory(root):
    return [dict(path=relative, **entry) for relative, entry in package_inventory(root).items()]


def macho(path):
    if not path.is_file() or path.is_symlink():
        return False
    with path.open('rb') as stream:
        return stream.read(4) in MACHO


def load_commands(path):
    # otool -l includes dylib load/ID and LC_RPATH separately for every architecture.
    output = run('/usr/bin/otool', '-l', path)
    architectures = []
    section = dict(rpaths=[], dependencies=[])
    for block in re.split(r'Load command \d+\n', output):
        if re.search(r'\bLC_SEGMENT(?:_64)?\b', block) and section.get('complete'):
            architectures.append(section)
            section = dict(rpaths=[], dependencies=[])
        command = re.search(r'^\s*cmd (LC_\w+)', block, re.M)
        if not command:
            continue
        kind = command[1]
        if kind == 'LC_RPATH':
            section['rpaths'].append(re.search(r'\bpath (.*?) \(offset', block)[1])
        elif kind in ('LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB', 'LC_REEXPORT_DYLIB',
                      'LC_LOAD_UPWARD_DYLIB'):
            section['dependencies'].append(re.search(r'\bname (.*?) \(offset', block)[1])
        elif kind in ('LC_BUILD_VERSION', 'LC_VERSION_MIN_MACOSX'):
            field = 'minos' if kind == 'LC_BUILD_VERSION' else 'version'
            minimum = re.search(r'^\s*' + field + r' (\d+(?:\.\d+)*)', block, re.M)
            if minimum:
                section['minimumMacOS'] = minimum[1]
        elif kind == 'LC_CODE_SIGNATURE':
            section['complete'] = True
    architectures.append(section)
    return architectures


def normalize_bundle(bundle):
    # Relocated Qt QML plugins can retain their original Kit-relative search path.
    # Their actual dylib imports must still resolve inside the bundle after removal.
    bundle = bundle.resolve()
    executable = bundle / 'Contents/MacOS/yalahoo'
    removed = []
    minimum_versions = []
    for binary in bundle.rglob('*'):
        if not macho(binary):
            continue
        sections = load_commands(binary)
        minimum_versions.extend(section['minimumMacOS'] for section in sections
                                if 'minimumMacOS' in section)
        rpaths = {value for section in sections for value in section['rpaths']}
        for value in sorted(rpaths):
            expanded = Path(value.replace('@loader_path', str(binary.parent))
                            .replace('@executable_path', str(executable.parent))).resolve()
            if not expanded.is_relative_to(bundle):
                run('/usr/bin/install_name_tool', '-delete_rpath', value, binary)
                removed.append(dict(binary=str(binary.relative_to(bundle)), rpath=value))
    if not minimum_versions:
        raise RuntimeError('Mach-O minimum macOS versions are missing')
    minimum = max(minimum_versions, key=lambda value: tuple(map(int, value.split('.'))))
    plist_path = bundle / 'Contents/Info.plist'
    info = plistlib.loads(plist_path.read_bytes())
    # Declare the real compiler/runtime boundary, including all shipped slices.
    info['LSMinimumSystemVersion'] = minimum
    plist_path.write_bytes(plistlib.dumps(info))
    return dict(removedExternalRpaths=removed, minimumMacOS=minimum)


def strip_bundle(bundle):
    # Preserve exported/dynamic symbols; modify only the staged copies before signing.
    for symbols in sorted(bundle.rglob('*.dSYM'), reverse=True):
        if symbols.is_dir() and not symbols.is_symlink():
            shutil.rmtree(symbols)
    for binary in sorted(bundle.rglob('*')):
        if macho(binary):
            run('/usr/bin/strip', '-S', '-x', binary)


def inspect_bundle(bundle):
    bundle = bundle.resolve()
    contents = bundle / 'Contents'
    executable = contents / 'MacOS/yalahoo'
    info = plistlib.loads((contents / 'Info.plist').read_bytes())
    expected = dict(CFBundleExecutable='yalahoo', CFBundleIdentifier='yalahoo.music',
                    CFBundleDisplayName='yalahOO!!!', CFBundleName='yalahOO!!!',
                    CFBundlePackageType='APPL', CFBundleIconFile='yalahoo.icns',
                    NSHighResolutionCapable=True)
    if any(info.get(key) != value for key, value in expected.items()):
        raise RuntimeError('Invalid production bundle identity/Retina/icon metadata')
    required = ['Resources/yalahoo.icns', 'PlugIns/platforms/libqcocoa.dylib',
                'PlugIns/imageformats/libqsvg.dylib', 'PlugIns/iconengines/libqsvgicon.dylib',
                'Resources/qml/QtQuick/qmldir', 'Resources/qml/QtQuick/Templates/qmldir',
                'Resources/qml/QtQuick/Dialogs/qmldir']
    for name in ('Core', 'Gui', 'Qml', 'Quick', 'Multimedia', 'CanvasPainter'):
        required.append(f'Frameworks/Qt{name}.framework/Qt{name}')
    for relative in required:
        if not (contents / relative).is_file():
            raise RuntimeError(f'Missing deployed component: {relative}')
    if not list((contents / 'PlugIns/multimedia').glob('*.dylib')):
        raise RuntimeError('No deployed Qt media backend')
    paths = list(contents.rglob('*'))
    for path in paths:
        if any(part in ('QtQuick3D', 'qmltooling') for part in path.parts) or re.search(
                r'Qt(?:Quick3D|Test|QuickTest)|\.dSYM$', path.name):
            raise RuntimeError(f'Development component in production bundle: {path}')
        if path.is_symlink() and not path.resolve().is_relative_to(bundle):
            raise RuntimeError(f'External bundle symlink: {path}')
    architectures = set(run('/usr/bin/lipo', '-archs', executable).split())
    commands = {path: load_commands(path) for path in paths if macho(path)}
    minimum_versions = [section['minimumMacOS'] for sections in commands.values()
                        for section in sections if 'minimumMacOS' in section]
    minimum = max(minimum_versions, key=lambda value: tuple(map(int, value.split('.'))))
    if info.get('LSMinimumSystemVersion') != minimum:
        raise RuntimeError('Info.plist minimum macOS differs from actual Mach-O requirements')
    executable_rpaths = [entry for section in commands[executable] for entry in section['rpaths']]

    def expand(value, loader):
        return Path(value.replace('@executable_path', str(executable.parent))
                    .replace('@loader_path', str(loader.parent))).resolve()

    dependencies = []
    for binary, sections in commands.items():
        if not architectures <= set(run('/usr/bin/lipo', '-archs', binary).split()):
            raise RuntimeError(f'Architecture mismatch: {binary}')
        for section in sections:
            rpaths = [(value, binary) for value in section['rpaths']] + [
                (value, executable) for value in executable_rpaths]
            for value, loader in rpaths:
                expanded = expand(value, loader)
                if not expanded.is_relative_to(bundle):
                    raise RuntimeError(f'External LC_RPATH: {binary}: {value}')
            for value in section['dependencies']:
                if value.startswith(('/System/Library/', '/usr/lib/')):
                    resolution = 'macOS system'
                else:
                    if value.startswith('@rpath/'):
                        candidates = [expand(base, loader) / value[len('@rpath/'):] for base, loader in rpaths]
                    elif value.startswith(('@loader_path/', '@executable_path/')):
                        candidates = [expand(value, binary)]
                    else:
                        raise RuntimeError(f'External dylib reference: {binary}: {value}')
                    resolved = next((path.resolve() for path in candidates if path.is_file()), None)
                    if resolved is None or not resolved.is_relative_to(bundle):
                        raise RuntimeError(f'Unresolved bundle dylib: {binary}: {value}')
                    resolution = str(resolved.relative_to(bundle))
                dependencies.append(dict(binary=str(binary.relative_to(bundle)),
                                         dependency=value, resolution=resolution))
    run('/usr/bin/codesign', '--verify', '--deep', '--strict', bundle)
    return dict(architectures=sorted(architectures), minimumMacOS=minimum, binaries=len(commands),
                dependencies=dependencies, signature='ad-hoc; local execution only',
                scope='Mach-O/plugin/QML closure and signature; physical device/presentation independent')


def verify_inventory(package):
    metadata = json.loads((package / 'package.json').read_text())
    # package.json describes every other file, including the verifier and symlinks.
    actual = inventory(package)
    if (metadata['files'] != actual or metadata.get('review') is not False
            or metadata.get('configuration') != 'Release'):
        raise RuntimeError('Production package inventory or configuration changed')
    return metadata
