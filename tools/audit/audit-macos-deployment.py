#!/usr/bin/env python3
"""Bind the macOS deployment call-chain review and actual product-QML closure."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
# Bindings make the manually reviewed chains traceable; matching a symbol is not runtime proof.
CHECKS = [
    ('COMMON-01', 'CMakeLists.txt', 'CMAKE_CXX_STANDARD 23',
     'Root C++23 feature/ABI probes and QtKit numeric latest-native enumeration precede all targets. '
     'App/Scene/Rhi/Navigation/Text/Opening retain static qrc/qsb linkage; only APPLE adds Objective-C++/AppKit. '
     'Production Release has Review OFF, excludes research QML/assets and direct Quick3D links. '
     'Qt CanvasPainter may itself depend on Qt modules; deployment follows real Mach-O dependencies.'),
    ('COMMON-02', 'cmake/Deployment.cmake', 'set(deployment_qml',
     'Shared file(GENERATE) derives executable/Kit and the exact application QML list. macos.py compares '
     'that list to compiled qrc before copying it to an owned scan root. Module-relative qrc assets, embedded '
     'fonts/shaders/audio/branding do not depend on cwd. PlugIns and Resources/qml resolve within Contents. '
     'Unicode/space paths use pathlib/subprocess argument arrays; no shell interpolation or user data copy.'),
    ('COMMON-03', 'src/app/GraphicsBackend.h', 'QSGRendererInterface::Metal',
     'Ordinary macOS selects Metal; explicit OpenGL/CoreProfile/basic remains available for R36. '
     'Deployment changes no clip/Y/depth, source lighting/material, capture ordering, alpha, sampling or effect. '
     'Same-Kit qsb outputs are embedded. Actual Metal/Retina attachment checks use existing hidden probes; '
     'static cross-target translation does not establish other GPU execution.'),
    ('COMMON-04', 'src/app/main.cpp', 'QCoreApplication::aboutToQuit',
     'GUI-owned public PlaybackController/QMediaPlayer/QAudioOutput/QMediaDevices, separate BGM and bounded '
     'QSoundEffect lane outlive the QML engine. Hidden/minimized state gates visual FrameClock/SceneItem, '
     'never stops media. Last-window close and menu Quit reach Qt shutdown, stopping all lanes. Revision '
     'gates protect source/device/error/end/queued-next and teardown; no PCM engine or platform audio bridge.'),
    ('COMMON-05', 'tools/deploy/verify-macos.py', "workingDirectoryOutsidePackage=True",
     'Verifier checks immutable inventory/symlinks, every Mach-O dependency/architecture/signature, then '
     'starts the actual product from a fresh package-external cwd with Kit/QML/DYLD overrides removed and '
     'system-only PATH. Current process exit/timeout and fresh passed report are required. The hidden '
     'product probe instantiates real QML and loads qrc audio without output/play; loaded dyld images must '
     'be system or inside the bundle. This is loading evidence, not audible/physical or GPU evidence.'),
    ('MAC-01', 'src/app/CMakeLists.txt', 'MACOSX_BUNDLE_ICON_FILE yalahoo.icns',
     'APPLE packages the existing original ICNS as Resources/yalahoo.icns, sets bundle ID yalahoo.music '
     'and display name yalahOO!!!. Info.plist explicitly enables Retina and system GPU switching; '
     'package minimum macOS equals the maximum actual Mach-O slice requirement. QWindow/Quick use '
     'logical geometry and effective DPR; source assets remain independent of cwd.'),
    ('MAC-01', 'src/platform/macos/ApplicationMenu.mm', 'NSApp.mainMenu = m_state->bar',
     'Public AppKit menu has Command settings/quit/close/hide/minimize and native Edit/Window commands. '
     'Settings targets the existing PanelController and respects opening input lock. Edit validates the '
     'current focus editor and read-only/undo/selection/paste state, then invokes its existing Qt methods. '
     'QPointer rejects dead windows; no captured visual item; RAII restores the previous retained menu. '
     'QML controls remain self drawn; only OS menu chrome is native.'),
    ('MAC-01', 'tools/deploy/macos.py', 'closure = inspect_bundle(bundle)',
     'Native guard precedes writes. Isolated latest-Kit Release/Review OFF build uses that Kit macdeployqt '
     'and checked qtpaths; production-only scanner root excludes Review imports. Qt deploys required '
     'frameworks/plugins/media dependencies; package audit rejects external install names/rpaths, missing '
     'architectures/modules and research components. Project/font/audio/installed-Qt attribution is kept. '
     'Optional non-SQLite drivers from recursive QtQuick deployment are excluded because product stores '
     'are JSON and have no SQL service; SQLite and all required dylibs remain. Local ad-hoc signing is '
     'checked; no identity, notarization, DMG publication or upload is performed.'),
    ('MAC-01', 'tools/launchers/run-native.sh', 'package="$repository/out/macos"',
     'Root .command/.sh and demo/lab entries resolve their own location, prefer the deployed macOS '
     'bundle and exec with exact arguments/exit code. Development fallback requires a real Release '
     'executable and selected Kit plugins. Packaged children clear foreign Qt/QML/DYLD lookup overrides; '
     'the caller environment is untouched. Launchers neither build nor start a Web service.'),
    ('PERF-01', 'tools/deploy/macos_bundle.py', 'def inventory(root)',
     'Explicit offline package operations cost O(total bytes + load commands), with one streamed MiB '
     'hash buffer and O(files/dependencies) metadata. No deployment scan/hash runs on the application '
     'frame path. Product rendering still traverses <=432 array slots plus bounded owned copies.'),
    ('PERF-02', 'src/services/CoverCache.cpp', 'void CoverCache::pump',
     'GUI/render hot paths acquire no new IO, wait or worker queue. Existing <=3 decoders, request '
     'coalescing, generation cancellation and bounded CPU/cache/upload capacities remain. Synchronous '
     'CMake/deploy/sign/audit/child waits run only in explicit tools; verifier stdout/stderr go to files.'),
    ('PERF-03', 'src/platform/macos/ApplicationMenu.mm', 'struct ApplicationMenu::State',
     'One process menu and fixed actions are created after the QML root, outside frame publication. '
     'Actions inspect only current scalar editor properties and invoke existing controller/editor methods. '
     'No per-frame image copy, snapshot serialization or uniform upload is added.'),
    ('PERF-04', 'src/render/rhi/ResourcePlan.h', 'navigation-ruler-depth-upper-bound',
     'Metal/Retina target keys retain physical width/height, format, samples, resolves/mips and '
     'native physical output and explicit device capability checks. ResourcePlan includes shared attachments once, Canvas RGBA8 color4 + '
     'depth/stencil8 bytes and resize overlap; driver/compositor/font residency remains independent. '
     'Menus/deployment add no pass/texture/PSO. Existing optics/postprocess probes own actual descriptors.'),
    ('PERF-05', 'src/render/navigation/RulerInk.cpp', 'synchronize',
     'Production ruler and opening use C++ CanvasPainter, title uses glyph/Quick Text; no production '
     'Canvas2D import is required. Same-Kit CanvasPainter framework/shaders and Qt Quick composition '
     'are deployed. Render-thread owned painters receive value snapshots, transparent output is '
     'composited at existing QML order, and size/DPR/style/path cache dependencies remain unchanged.'),
    ('PERF-06', 'qml/Yalahoo/Music/pages/MusicStage.qml', 'readonly property bool visualActive',
     'Application hide/suspend and window visible/minimized state gate the single FrameClock, '
     'startup snapshot admission, SceneItem rendering and cover work. GUI controls keep existing '
     'virtualization/focus/input ownership. Native settings shares the existing panel, and native Edit '
     'targets only the current Qt editor; no hidden polling or input timer is added.'),
    ('PERF-07', 'tools/deploy/macos.py', 'backup = ROOT',
     'Staging/scan trees have explicit ownership; build/deploy/dependency/probe failure cannot replace '
     'an existing package. Rename failure restores the previous package. Runtime menu dies before '
     'engine/controllers, clears native targets and restores retained system menu. Existing render '
     'epoch/DPR/resize/rebuild ownership remains; physical hide/restore, devices, VSYNC and full '
     'product performance stay in R31-P/R28-P rather than being inferred from hidden loading.'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-directory', type=Path, default=ROOT / 'build/macos-release')
    args = parser.parse_args()
    build = args.build_directory.resolve()
    metadata = json.loads((build / 'deployment.json').read_text())
    if metadata['system'] != 'Darwin' or metadata['review'] or metadata['configuration'] != 'Release':
        raise SystemExit('Audit the real macOS production Release build')
    qml = [(ROOT / 'qml' / value).resolve() for value in metadata['qmlFiles']]
    if not qml or len(qml) != len(set(qml)) or any(
            not path.is_file() or not path.is_relative_to(ROOT / 'qml') for path in qml):
        raise SystemExit('Invalid production QML paths')
    compiled = {Path(node.text).resolve() for node in ET.parse(
        build / 'src/app/.qt/rcc/Yalahoo_raw_qml_0.qrc').iter('file') if node.text.endswith('.qml')}
    if set(qml) != compiled:
        raise SystemExit('Production metadata diverges from compiled QML resources')
    kit = Path(metadata['qtKit'])
    scanner = next((kit / folder / 'qmlimportscanner' for folder in ('libexec', 'bin')
                    if (kit / folder / 'qmlimportscanner').is_file()), None)
    if scanner is None:
        raise SystemExit('Selected Kit import scanner is missing')
    with tempfile.TemporaryDirectory(prefix='yalahoo-macos-scan-') as directory:
        for source in qml:
            destination = Path(directory) / source.relative_to(ROOT / 'qml')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
        scan = subprocess.run([str(scanner), '-rootPath', directory, '-importPath', str(kit / 'qml')],
                              text=True, capture_output=True, check=True)
        imports = json.loads(scan.stdout)
    modules = sorted({entry['name'] for entry in imports if entry.get('type') == 'module'})
    if not {'QtQuick', 'QtQuick.Templates', 'QtQuick.Dialogs', 'QtMultimedia'} <= set(modules) or any(
            name.startswith(('QtQuick3D', 'QtCanvas2D')) for name in modules):
        raise SystemExit('Unexpected production QML module closure')
    sources = {ROOT / path for _, path, _, _ in CHECKS} | set(qml)
    for folder in ('src', 'cmake', 'tools/deploy', 'tools/launchers'):
        sources.update(path for path in (ROOT / folder).rglob('*') if path.is_file()
                       and '__pycache__' not in path.parts)
    sources.update(ROOT / path for path in ('cmake/Info.plist.in', 'yalahoo.command', 'yalahoo.sh',
                   'tools/build/build.sh', 'tools/build/macos.sh', 'docs/platforms/macos.md',
                   'tests/integration/MacApplicationMenuTest.mm'))
    hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(sources)}
    for platform in ('macos', 'windows', 'linux'):
        checks = []
        for check_id, path, symbol, requirement in CHECKS:
            source = (ROOT / path).read_text()
            if symbol not in source:
                raise SystemExit(f'Review binding changed: {path}::{symbol}')
            checks.append(dict(checkId=check_id, requirement=requirement, platform=platform,
                               sourcePath=path, lineOrSymbol=symbol,
                               line=source[:source.index(symbol)].count('\n') + 1,
                               sourceSha256=hashes[path], evidence=requirement, result='source-reviewed',
                               limitations=('Native macOS execution reported separately; '
                                            'physical/device/presentation: R31-P/R36; OpenGL: R36.'
                                            if platform == 'macos' else
                                            'macOS bundle/menu branch is N/A on this platform; '
                                            'shared metadata/probe/launcher source impact reviewed. '
                                            'Target execution remains R30-P/R32-P and R36.')))
        output = ROOT / 'verification' / f'{platform}-static' / 'macos-deployment'
        output.mkdir(parents=True, exist_ok=True)
        (output / 'source-matrix.json').write_text(json.dumps(dict(checks=checks, sourceSha256=hashes,
            productionQml=dict(qtVersion=metadata['qtVersion'], modules=modules, stderr=scan.stderr,
                               scannerSha256=hashlib.sha256(scanner.read_bytes()).hexdigest())), indent=2) + '\n')
    print('R31 source bindings, compiled product QML and same-Kit imports verified; runtime evidence is separate')


if __name__ == '__main__':
    main()
