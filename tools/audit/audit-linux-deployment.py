#!/usr/bin/env python3
"""Trace Linux deployment and shared source chains; native target evidence stays separate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ('COMMON-01', 'cmake/QtKit.cmake', 'set(system Linux)',
     'Root C++23 required/no extensions and actual to_underlying/optional transform probes precede '
     'target setup. Latest native numeric Kit selection validates qmake spec/version, architecture, '
     'gcc-compatible Linux ABI and all modules without older fallback. Platform adds real warning '
     'flags; static app libraries/moc/qrc/qsb resolve via their source lists. Production Review OFF '
     'alone enables Linux install/deploy; Quick3D is not a direct product link.'),
    ('COMMON-02', 'src/library/DirectoryScanner.cpp', 'Qt::CaseSensitive',
     'Non-Windows paths use UTF-8 filesystem conversion and case-sensitive identity/containment; '
     'stable IDs hash native absolute paths. QDir/QUrl and subprocess argv preserve Unicode/spaces. '
     'Application identity precedes QStandardPaths XDG data lookup. Preferences/cache use atomic '
     'QSaveFile and surface permission failures; read-only import never writes music. Fonts, icons, '
     'GLB, audio and qsb are qrc resources, independent of cwd. No source/compatibility field is added.'),
    ('COMMON-03', 'src/render/rhi/BackendCapabilities.cpp', 'QRhi::create(QRhi::Vulkan',
     'Linux defaults to Vulkan. Default or explicit Vulkan is probed once after QGuiApplication and before any '
     'Quick window with QVulkanInstance/QRhi device and production RGBA16F/R16F/D32F render formats; '
     'Only an implicit default falls back with warning and actual backend environment updated; explicit requests fail on unavailable Kit/headers/device/formats. '
     'Probe device dies before instance, production Qt owns its own render device. Actual swapchain, '
     'format/MSAA/resolve, clip/Y and GPU efficiency remain target runtime checks, never inferred.'),
    ('COMMON-03', 'src/render/rhi/RhiItem.cpp', 'clipSpaceCorrMatrix',
     'Non-Windows QRhi path retains native VP/Y conversion, format/size/sample capability checks, '
     'shared geometry, D32F shadow/R32F diagnostics, RGBA16F color and RGBA8 normal/depth, independent '
     'single-sample resolves, transmission mips, AO R16F and full DOF depth. GL odd-width readback uses '
     'actual PACK_ALIGNMENT/stride; no D3D native-transfer evidence is fabricated on Linux. qsb '
     'SPIR-V/GLSL/HLSL/MSL cross generation is static translation, not target GPU execution.'),
    ('COMMON-04', 'src/media/PlaybackController.cpp', 'QMediaPlayer::mediaStatusChanged',
     'Public QMediaPlayer/QAudioOutput/QMediaDevices retain source intent/revision, default versus '
     'explicit device selection/disappearance, queued end/next, unknown duration/unseekable/error '
     'handling and cancellation. Separate BGM/QSoundEffect lanes stay independent. Media metadata '
     'has an event-loop worker; scanning/decoding are bounded workers. Hidden visual state never '
     'stops songs; aboutToQuit stops all lanes. No Linux PCM/FFmpeg process or audio/driver reconfiguration.'),
    ('COMMON-05', 'tools/deploy/verify-linux.py', 'workingDirectoryOutsidePackage=True',
     'Before publishing a local package, immutable inventory/mode/relative symlinks, all ELF '
     'machine/rpaths/ldd dependencies and a real product hidden QML/qrc/media load must pass. '
     'Probe uses offscreen QPA with no shown window/GPU context/audio output, an external fresh cwd '
     'and isolated XDG roots, no developer PATH/Qt/LD overrides. Current exit/timeout, nonempty '
     'loaded ELF images, actual Kit version/platform and fresh report are required. This is no '
     'Linux GPU, actual media playback/device, physical input, deployment-on-other-system or VSYNC claim.'),
    ('LNX-01', 'cmake/Deployment.cmake', 'qt_generate_deploy_qml_app_script',
     'Native Linux-only Qt deployment follows the exact compiled product import list, not the source '
     'tree; Templates/Dialogs/Multimedia and CanvasPainter dynamic dependency closure remain. '
     'qt.conf and every ELF RPATH use package-relative paths. xcb/Wayland/offscreen, GL integrations, '
     'image formats, TLS and Qt FFmpeg plugin are explicit; recursive ldd adds only selected-Kit '
     'runtime dependencies, never system drivers/glibc. Missing libraries/Wayland/attribution fail '
     'packaging. Desktop/icon assets are staged but never installed into the user/system automatically.'),
    ('LNX-01', 'tools/launchers/run-native.sh', 'package="$repository/out/linux"',
     'Executable LF shell entry resolves its own directory, prefers a complete Linux package, '
     'preserves each argv and exec exit code; development fallback requires Release and selected '
     'Kit plugins/QML/libraries. Package launcher clears foreign Qt/import/LD preload overrides '
     'while preserving XDG, display/session, GPU choice and audio route. No sudo, system config, '
     'shell argument re-evaluation or auto build. Native platform guard runs before package writes.'),
    ('LNX-01', 'src/render/navigation/InputRouter.cpp', 'event->pixelDelta()',
     'pixelDelta has priority over angleDelta; the existing router bounds/coalesces latest navigation '
     'intent and commits through the same source motion/frame. Wayland/xcb physical touchpad/input '
     'and IME/focus remain native tests. TextLayout keeps full shaping/fallback and embedded MiSans '
     'physical face table validation; only macOS applies its proven integral half-leading adjustment.'),
    ('PERF-01', 'tools/deploy/linux_package.py', 'def inventory(root)',
     'Deployment is explicit offline O(package bytes + ELF dependencies), streamed 1MiB hashes '
     'and O(files/dependency edges) metadata. No scanning/hash/ELF probing runs per product frame. '
     'Production frame still visits bounded <=432 array slots and finite selected/returning copies.'),
    ('PERF-02', 'src/services/CoverCache.cpp', 'void CoverCache::pump',
     'No added GUI/render IO, decoder wait, readback or polling. Existing three decoders, generation '
     'rejection, request coalescing and independent queue/cache/upload limits remain. Qt deploy, '
     'ELF tools and the bounded child-process wait execute only in developer tools. Vulkan capability '
     'probe is one startup operation on GUI, before any production render-thread resources.'),
    ('PERF-03', 'src/render/rhi/SceneItem.cpp', 'void SceneItem::updatePolish',
     'GUI publishes immutable SceneFrame and changed revisions; render owns QRhi resources and '
     'never reads LibraryStore/QML/IO. Uniform active prefixes and dirty instance/tile ranges retain '
     'existing upload budgets. Platform selection adds no per-frame copy, serialization or upload.'),
    ('PERF-04', 'src/render/rhi/ResourcePlan.h', 'navigation-ruler-depth-upper-bound',
     'GL/Vulkan use the same complete pass graph/physical size/sample/resolve/mip keys and '
     '8294400-pixel guard. RGBA16F eight bytes, RGBA8 four, R16F two, D32F four; MSAA and resolves '
     'are separate, shared textures count once. Canvas RGBA8 color4 + depth/stencil8 upper bound '
     'includes resize old+new overlap. Driver/compositor/font/buffer padding stays unknown, not zero; '
     'no GPU peak or D3D efficiency is claimed for either X11 or Wayland.'),
    ('PERF-05', 'src/render/navigation/RulerInk.cpp', 'synchronize',
     'Production ruler/opening use C++ CanvasPainter; title uses shaped glyph/Quick Text. No '
     'production QtCanvas2D import is needed. Painter synchronization transfers values, render owns '
     'paths/font resources, transparent single-sample RGBA8 composes in the same Quick order. '
     'Path/style/font/size/effective DPR cache dependencies remain; deployment retains Qt module '
     'shaders. Three API Review imports remain in Review, excluded from product scanning.'),
    ('PERF-06', 'qml/Yalahoo/Music/pages/MusicStage.qml', 'readonly property bool visualActive',
     'Same single FrameClock and virtualized delegates/letters/song rows on xcb/Wayland. '
     'Invisible/minimized/suspended visual layers stop tick/paint/cover admission; latest state '
     'is coalesced on restore while media continues. No frame timer, alternate input or QML control '
     'is introduced by platform deployment.'),
    ('PERF-07', 'tools/deploy/linux.py', 'backup = ROOT',
     'Owned staging precedes complete closure and real product probe; build/install/ELF/child failure '
     'cannot replace an old package, rename failure restores it. Qt production SG/render owns '
     'GL/Vulkan resources, DPR/surface changes invalidate compatible keys, stale callbacks use epochs. '
     'No native restore, normal VSYNC, physical latency or target memory result is inferred; '
     'R32-P retains non-OpenGL build/media/deploy in R34 and xcb/Wayland OpenGL dynamics in R35.'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-directory', type=Path, default=ROOT / 'build/linux-release')
    args = parser.parse_args()
    build = args.build_directory.resolve()
    metadata = json.loads((build / 'deployment.json').read_text())
    if metadata['review'] or metadata['configuration'] != 'Release':
        raise SystemExit('A real production Review OFF Release build is required')
    qml = [(ROOT / 'qml' / value).resolve() for value in metadata['qmlFiles']]
    if not qml or len(qml) != len(set(qml)) or any(
            not path.is_file() or not path.is_relative_to(ROOT / 'qml') for path in qml):
        raise SystemExit('Invalid production QML paths')
    compiled = {Path(node.text).resolve() for node in ET.parse(
        build / 'src/app/.qt/rcc/Yalahoo_raw_qml_0.qrc').iter('file') if node.text.endswith('.qml')}
    if set(qml) != compiled:
        raise SystemExit('Production metadata differs from compiled QML')
    kit = Path(metadata['qtKit'])
    scanner = next((kit / part / 'qmlimportscanner' for part in ('libexec', 'bin')
                    if (kit / part / 'qmlimportscanner').is_file()), None)
    if scanner is None:
        raise SystemExit('Selected Kit QML scanner is missing')
    with tempfile.TemporaryDirectory(prefix='yalahoo-linux-scan-') as scan_root:
        for source in qml:
            destination = Path(scan_root) / source.relative_to(ROOT / 'qml')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
        scan = subprocess.run([str(scanner), '-rootPath', scan_root, '-importPath', str(kit / 'qml')],
                              text=True, capture_output=True, check=True)
        modules = sorted({entry['name'] for entry in json.loads(scan.stdout)
                          if entry.get('type') == 'module'})
    if not {'QtQuick', 'QtQuick.Templates', 'QtQuick.Dialogs', 'QtMultimedia'} <= set(modules) or any(
            name.startswith(('QtQuick3D', 'QtCanvas2D')) for name in modules):
        raise SystemExit('Unexpected product QML import closure')
    qsb = next((kit / part / 'qsb' for part in ('bin', 'libexec')
                if (kit / part / 'qsb').is_file()), None)
    if qsb is None:
        raise SystemExit('Selected Kit shader tool is missing')
    shaders = []
    for folder in ('src/app/.qsb', 'src/render/rhi/.qsb'):
        for path in sorted((build / folder).glob('*.qsb')):
            dump = subprocess.run([str(qsb), '-d', str(path)], text=True,
                                  capture_output=True, check=True).stdout
            variants = re.findall(r'^  Shader \d+: (.+)$', dump, re.M)
            if not all(any(value.startswith(kind) for value in variants)
                       for kind in ('SPIR-V', 'GLSL', 'HLSL', 'MSL')):
                raise SystemExit(f'Missing cross-target shader variant: {path}')
            shaders.append(dict(path=str(path.relative_to(build)), variants=variants,
                                sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    if not {'geometry.vert.qsb', 'geometry-instanced.vert.qsb', 'shell.frag.qsb',
            'bokeh.frag.qsb', 'smaa-blend.frag.qsb', 'opening-paper.frag.qsb'} <= {
            Path(value['path']).name for value in shaders}:
        raise SystemExit('Compiled production shader closure is incomplete')
    sources = set(qml) | {ROOT / value for _, value, _, _ in CHECKS}
    for folder in ('src', 'cmake', 'tools/deploy', 'tools/launchers'):
        sources.update(path for path in (ROOT / folder).rglob('*') if path.is_file()
                       and '__pycache__' not in path.parts)
    sources.update(ROOT / value for value in ('CMakeLists.txt', 'CMakePresets.json',
                   'tools/build/linux.sh', 'tools/build/build.sh', 'yalahoo.sh',
                   'resources/branding/yalahoo.desktop', 'docs/platforms/linux.md',
                   'docs/contracts/render-backend.md', 'docs/contracts/native-bootstrap.md',
                   'docs/contracts/source-fidelity-review.md', 'tests/tools/LinuxDeploymentTest.py'))
    hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(sources)}
    # Include every conditional, including non-Windows else branches, in the
    # source scope. The call-chain conclusions above are a manual semantic audit.
    branches = []
    for path in sorted((ROOT / 'src').rglob('*')):
        if path.suffix in ('.h', '.cpp', '.mm'):
            branches.extend(dict(sourcePath=str(path.relative_to(ROOT)), line=index, directive=line)
                            for index, line in enumerate(path.read_text().splitlines(), 1)
                            if line.startswith(('#if', '#elif', '#else')))
    for platform in ('linux', 'windows', 'macos'):
        checks = []
        for check_id, path, symbol, conclusion in CHECKS:
            source = (ROOT / path).read_text()
            if symbol not in source:
                raise SystemExit(f'Review binding changed: {path}::{symbol}')
            checks.append(dict(checkId=check_id, requirement=conclusion, platform=platform,
                               sourcePath=path, lineOrSymbol=symbol,
                               line=source[:source.index(symbol)].count('\n') + 1,
                               sourceSha256=hashes[path], evidence=conclusion, result='source-reviewed',
                               limitations=('Linux native compilation/deployment/media and optional '
                                            'Vulkan: R32-P/R34; OpenGL xcb/Wayland: R32-P/R35.'
                                            if platform == 'linux' else
                                            'Linux-only ELF/install/probe branches N/A; shared startup/launcher '
                                            'and import closure reviewed. Actual host results recorded separately; '
                                            'target GPU/physical facts not established.')))
        output = ROOT / 'verification' / f'{platform}-static' / 'linux-deployment'
        output.mkdir(parents=True, exist_ok=True)
        (output / 'source-matrix.json').write_text(json.dumps(dict(checks=checks,
            sourceSha256=hashes, preprocessorScope=branches, bakedShaderVariants=shaders, productionQml=dict(
                buildSystem=metadata['system'], qtVersion=metadata['qtVersion'], modules=modules,
                scannerSha256=hashlib.sha256(scanner.read_bytes()).hexdigest(), stderr=scan.stderr),
            targetExecution='Not established by this source audit'), indent=2) + '\n')
    print('R32 source bindings and compiled production imports reviewed; native target evidence is separate')


if __name__ == '__main__':
    main()
