#!/usr/bin/env python3
"""Build and atomically package the selected native Qt macOS Release product."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import uuid
import xml.etree.ElementTree as ET
from macos_bundle import digest, inspect_bundle, inventory, normalize_bundle, run

ROOT = Path(__file__).resolve().parents[2]


def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    if platform.system() != 'Darwin':
        raise SystemExit('macOS deployment requires native macOS and its latest complete Qt Kit.')
    build = ROOT / 'build/macos-release'
    output = ROOT / 'out/macos'
    subprocess.run(['cmake', '--preset', 'qt-release', '-B', str(build),
                    '-DYALAHOO_REVIEW=OFF', '-DYALAHOO_MACOS_VISIBLE_TESTS=OFF'], cwd=ROOT, check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'Yalahoo'], check=True)
    metadata = json.loads((build / 'deployment.json').read_text())
    if metadata['configuration'] != 'Release' or metadata['review'] or metadata['system'] != 'Darwin':
        raise RuntimeError('Only the native production Release bundle may be packaged')
    kit = Path(metadata['qtKit'])
    qtpaths = kit / 'bin/qtpaths'
    deploy = kit / 'bin/macdeployqt'
    if run(qtpaths, '--query', 'QT_VERSION') != metadata['qtVersion'] or not deploy.is_file():
        raise RuntimeError('Deployment tools must belong to the selected Qt Kit')
    qml = metadata['qmlFiles']
    compiled = {str(Path(node.text).resolve().relative_to(ROOT / 'qml')) for node in ET.parse(
        build / 'src/app/.qt/rcc/Yalahoo_raw_qml_0.qrc').iter('file') if node.text.endswith('.qml')}
    if not qml or len(qml) != len(set(qml)) or compiled != set(qml):
        raise RuntimeError('Deployment QML does not match compiled production resources')
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('QT_', 'QML', 'QSG_', 'DYLD_', 'YALAHOO_'))}
    environment['PATH'] = str(kit / 'bin') + ':/usr/bin:/bin:/usr/sbin:/sbin'
    (ROOT / 'out').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='macos-staging-', dir=ROOT / 'out') as temporary:
        staging = Path(temporary)
        bundle = staging / 'yalahoo.app'
        source = Path(metadata['executable']).parents[2]
        shutil.copytree(source, bundle, symlinks=True)
        with tempfile.TemporaryDirectory(prefix='deploy-qml-', dir=build) as scan:
            for relative in qml:
                path = Path(relative)
                if path.is_absolute() or '..' in path.parts:
                    raise RuntimeError(f'Invalid production QML path: {path}')
                destination = Path(scan) / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / 'qml' / path, destination)
            # The selected macdeployqt resolves frameworks/plugins and bundled media dependencies.
            subprocess.run([str(deploy), str(bundle), '-qmldir=' + scan,
                            '-qmlimport=' + str(kit / 'qml'), '-verbose=2', '-no-codesign'],
                           env=environment, check=True)
        # macdeployqt recursively copies QtQuick's optional LocalStorage module,
        # then every SQL driver. The product uses JSON stores, no external SQL
        # service; retain Qt's self-contained SQLite backend only.
        sql_plugins = bundle / 'Contents/PlugIns/sqldrivers'
        if sql_plugins.is_dir():
            for plugin in sql_plugins.glob('*.dylib'):
                if plugin.name != 'libqsqlite.dylib':
                    plugin.unlink()
        resources = bundle / 'Contents/Resources'
        # Qt's macOS prefix is Contents; every override remains relative to it.
        (resources / 'qt.conf').write_text('[Paths]\nPlugins=PlugIns\nQmlImports=Resources/qml\n')
        licenses = resources / 'licenses'
        shutil.copytree(ROOT / 'resources/licenses', licenses / 'project')
        for name in ('NOTICE.txt', 'MiSans-license.pdf'):
            shutil.copyfile(ROOT / 'resources/fonts' / name, licenses / name)
        shutil.copyfile(ROOT / 'resources/audio/README.md', licenses / 'audio.md')
        # Preserve the installed Qt attribution texts without copying development modules.
        qt_licenses = next((path / 'Licenses' for path in kit.parents if (path / 'Licenses').is_dir()), None)
        if qt_licenses is None:
            raise RuntimeError('Selected Qt installation license directory is missing')
        shutil.copytree(qt_licenses, licenses / 'Qt')
        normalization = normalize_bundle(bundle)
        subprocess.run(['/usr/bin/codesign', '--force', '--deep', '--sign', '-', str(bundle)], check=True)
        closure = inspect_bundle(bundle)
        closure.update(normalization)
        (build / 'deployment-dependencies.json').write_text(json.dumps(closure, indent=2) + '\n')
        for name in ('verify-macos.py', 'macos_bundle.py'):
            shutil.copyfile(ROOT / 'tools/deploy' / name, staging / name)
        (staging / 'README.md').write_text('''# yalahOO!!!

Open yalahoo.app for your local library, or run its Contents/MacOS/yalahoo --demo.
Keep the bundle intact when copying it. It includes Qt frameworks, QML, plugins,
media dependencies, fonts, shaders, branding and attribution in Contents/Resources/licenses.
This local package is ad-hoc signed; it is not a notarized distribution.

Run python3 verify-macos.py for hidden product/QML/resource/media loading checks.
The verifier requires Python only as a developer tool; the application does not.
Use a fresh output directory outside the package. Reports do not establish physical
input, audible playback, device changes, normal VSYNC or screen presentation.
Complete manual checks and source build steps: docs/platforms/macos.md in the repository.
''')
        package = dict(displayName='yalahOO!!!', qtVersion=metadata['qtVersion'],
                       architecture=closure['architectures'], minimumMacOS=closure['minimumMacOS'],
                       configuration='Release', review=False,
                       deployToolSha256=digest(deploy), files=inventory(staging))
        (staging / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
        # A real clean child probe must pass before replacing an existing package.
        subprocess.run(['python3', str(staging / 'verify-macos.py'), '--package', str(staging)], check=True)
        backup = ROOT / 'out' / ('macos-previous-' + uuid.uuid4().hex)
        if output.exists():
            output.rename(backup)
        try:
            staging.rename(output)
        except BaseException:
            if backup.exists() and not output.exists():
                backup.rename(output)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    print(f'macOS production package: {output}')


if __name__ == '__main__':
    main()
