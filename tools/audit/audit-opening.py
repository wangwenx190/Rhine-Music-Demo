"""Audit the production opening's source, state, bounded drawing and RHI handoff."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ('COMMON-01', 'src/render/CMakeLists.txt', 'YalahooOpening', 'Root CMake adds Scene/Opening before app/Review; C++23 and latest Kit remain shared; app and ui-audit link the same production painter and qrc paths.'),
    ('COMMON-02', 'src/render/opening/OpeningInk.cpp', ':/assets/branding/reveal-map.json', 'GUI constructor loads immutable authored SVG/map once. No cwd, external assets, font copy or private song input; Qt qrc parser and Unicode source slogan are shared.'),
    ('COMMON-03', 'resources/shaders/opening-paper.frag', 'vec2(0.51, 0.44)', 'Source radial-gradient uses normalized elliptical farthest-corner geometry. Same-Kit CMake bakes all shader variants; no new 3D pass, projection convention or target format.'),
    ('COMMON-04', 'src/render/opening/OpeningController.cpp', 'm_motion->clock()->startOpening()', 'Opening only changes scene/motion/input state. No media source, play, PCM, network, output-device or song writes; existing playback IDs remain stable in production replay.'),
    ('COMMON-05', 'tools/review/OpeningAudit.cpp', 'first-3d-frame-after-opaque-rebuild', 'Actual MusicStage/SceneItem readiness and ten scene graph rebuilds check resumed 3D work. Own output quality, source math, state, resource and platform evidence remain separate.'),
    ('PERF-01', 'src/render/opening/OpeningInk.cpp', 'paths.size() != 19', 'Exactly 17 reveal centerlines plus two final decorations; constant dash ring and at most 432+4 scene instances. Preparation and film never scan the full library.'),
    ('PERF-02', 'src/render/rhi/SceneItem.cpp', 'token != guard->m_preparationToken', 'Async preparation receipts reject stale token/submission and require current cover readiness and drained uploads. GUI/render hot paths perform no IO/decode/wait/readback; prepare updates only continue a nonempty bounded atlas queue.'),
    ('PERF-03', 'src/render/rhi/SceneItem.cpp', 'if (m_sceneOccluded)', 'Opaque film stops pool preparation and GUI submission. Immutable RHI values and cached paths persist; normal frame uploads retain existing tile budget. No model/material/texture rebuild from film time.'),
    ('PERF-04', 'src/render/rhi/ResourcePlan.h', 'opening-canvas-color-upper-bound', 'Bounded Canvas RGBA8+depth/stencil and two skip blur crops join the resource descriptor plan; scene targets remain owned by existing renderer. Opening plan changes only recalculate descriptors, not PSO/targets; unknown driver VRAM stays null.'),
    ('PERF-05', 'src/render/opening/OpeningInk.cpp', 'brandTransform.map', 'Paths/round caps are mapped to logical coordinates once per size; no cached stroke state scale, preserving authored width and device AA. Arc-length trimming uses cached QPainterPath, no tessellated polyline approximation.'),
    ('PERF-06', 'qml/Yalahoo/Music/pages/MusicStage.qml', 'FrameAnimation', 'One production FrameAnimation; Opening consumes unscaled dt. Same Header brand remains; hidden chrome and scene input disable during film, actual 720ms stagger endpoints unlock once and restore a valid item.'),
    ('PERF-07', 'src/render/rhi/RhiItem.cpp', 'if (input.sceneOccluded)', 'Render bypass occurs before 3D resource/command work and clears transparent output only. After opaque SG rebuild, fresh complete input reinitializes resources; QObject callbacks are guarded, replay tokens reset and Canvas Loader destruction releases paths/attachments.'),
]

# Source relationships are checked as well as symbol anchors. Runtime tests independently
# establish actual behavior; these checks intentionally do not label target runtime passed.
controller = (ROOT / 'src/render/opening/OpeningController.cpp').read_text()
for forbidden in ('QTimer', 'QMediaPlayer', 'setSource(', 'processEvents', 'QNetwork'):
    if forbidden in controller:
        raise SystemExit(f'Opening introduced forbidden work: {forbidden}')
stage = (ROOT / 'qml/Yalahoo/Music/pages/MusicStage.qml').read_text()
app = (ROOT / 'qml/Yalahoo/Music/App.qml').read_text()
if stage.count('FrameAnimation {') != 1 or 'FrameAnimation {' in app:
    raise SystemExit('Production stage must own exactly one frame source')
if 'MusicStage {' not in app or 'MusicStage.qml' not in (ROOT / 'tools/review/OpeningAudit.cpp').read_text():
    raise SystemExit('Review must instantiate the actual production stage')
scene_item = (ROOT / 'src/render/rhi/SceneItem.cpp').read_text()
polish = scene_item[scene_item.index('void SceneItem::updatePolish()'):]
occluded = polish.index('if (m_sceneOccluded)')
resumed = polish.index('auto &adapter = *m_adapter;')
opaque = polish[occluded:resumed]
if (polish.index('!m_renderingActive') >= occluded or 'return;' not in opaque
        or 'next.sceneOccluded = true;' not in opaque or 'preparePool(' in opaque):
    raise SystemExit('Hidden/opaque polish must return before pool preparation with a clear-only input')
renderer = (ROOT / 'src/render/rhi/RhiItem.cpp').read_text()
start = renderer.index('    void render(QRhiCommandBuffer *cb) override')
end = renderer.index('\nprivate:', start)
render = renderer[start:end]
if render.index('if (input.sceneOccluded)') >= render.index('renderFrame(cb)'):
    raise SystemExit('Opaque guard is after 3D submission')
if 'input.cover.isNull() && !input.atlasGeneration' not in renderer:
    raise SystemExit('Opaque reconstruction must initialize RHI from resumed input')
blur = (ROOT / 'resources/shaders/opening-blur.frag').read_text()
weights = [float(value) for value in re.findall(r'\)\s*\* ([0-9.]+);', blur)]
if len(weights) != 16 or abs(weights[0] + 2 * sum(weights[1:]) - 1) > 1e-12:
    raise SystemExit('Skip blur Gaussian is not normalized or complete')
subprocess.run(['node', 'tools/review/export-opening-golden.mjs', '--check'], cwd=ROOT, check=True)
golden = json.loads((ROOT / 'tests/fixtures/opening/source-golden.json').read_text())
parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=Path, help='Explicitly authorized REFDB-02 source root')
arguments = parser.parse_args()
reference = arguments.reference
reference_info = {'decision': 'Adopted the render-entry opaque early-return idea only; no copied code, geometry, timing, assets or scheduler.', 'author': 'LBEILC', 'license': 'MIT'}
for path in ('Qt/src/RhiScene.cpp', 'LICENSE', 'NOTICE'):
    file = reference / path if reference else None
    if file and file.exists():
        reference_info[path] = hashlib.sha256(file.read_bytes()).hexdigest()
reference_info['symbol'] = 'ItemRenderer::render, opaque early return before renderer->render'
for platform, marker in [('windows', 'WIN32'), ('macos', 'YalahooVerification'), ('linux', 'UNIX AND NOT APPLE')]:
    rows = []
    entries = CHECKS + [(dict(windows='WIN-01', macos='MAC-01', linux='LNX-01')[platform], 'cmake/Platform.cmake', marker, 'Same native shared implementation and platform branch; target compiler/deployment/runtime remain separate. Remaining target/backend runtime gates are assigned to R36; this audit creates no contexts.')]
    for identity, path, symbol, evidence in entries:
        data = (ROOT / path).read_bytes()
        if ' '.join(symbol.split()) not in ' '.join(data.decode().split()):
            raise SystemExit(f'Opening source drift: {path}::{symbol}')
        rows.append(dict(checkId=identity, requirement=evidence, platform=platform, sourcePath=path, lineOrSymbol=symbol, evidence=evidence, sourceSha256=hashlib.sha256(data).hexdigest(), result='static-audit-passed', limitations='Dynamic output/physical display/normal VSYNC/performance are separate; missing target/backend evidence remains R20-P/R36.'))
    output = ROOT / 'verification' / f'{platform}-static' / 'opening'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'source-matrix.json').write_text(json.dumps(dict(checks=rows, frozen=golden['sources'], reference=reference_info), indent=2) + '\n')
print('Opening COMMON/PERF/platform source audit passed; target runtime is recorded separately')
