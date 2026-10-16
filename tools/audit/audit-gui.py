#!/usr/bin/env python3
"""R29 production GUI source closure. Runtime and output quality are independent facts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--report', type=Path, action='append', required=True)
args = parser.parse_args()
reports = [json.loads(path.read_text(encoding='utf-8')) for path in args.report]
if not all(report.get('passed') for report in reports):
    raise SystemExit('Every supplied GUI report must pass; preserve failures separately')
report = reports[0]
if any(any(item.get(key) != report.get(key) for key in
               ('backend', 'qtVersion', 'os', 'architecture', 'driver')) for item in reports):
    raise SystemExit('Do not merge different target environments')
# Later reports replace only their exact state/phase keys; unrelated states retain valid evidence.
merged = {}
for item in reports:
    for capture in item.get('captures', []):
        key = tuple(capture[name] for name in ('width', 'height', 'night', 'realistic', 'phase'))
        merged[key] = capture
sizes = ((1920,1080), (1280,720), (995,858), (960,900), (930,900),
         (390,844), (320,720), (2560,1080), (3440,1440))
expected = {(w,h,night,real,phase) for w,h in sizes for night in (False,True)
            for real in (False,True) for phase in
            (('browse','overview','detail','search') if real else
             ('browse','overview','detail','search','opening-mark','opening-scene'))}
if set(merged) != expected:
    raise SystemExit('A complete integrated GUI matrix is required, including any narrowed regressions')
# Component audits own the individual source equations, constants and resource graphs.
# Re-evaluate their current call paths rather than copy those requirements into this tool.
components = ('browse-navigation', 'title-source', 'detail-overview-source',
              'material-parameters', 'rhi-opaque', 'rhi-optics', 'rhi-flicker', 'rhi-ao',
              'rhi-fog', 'rhi-shadow', 'rhi-floor', 'rhi-bokeh', 'rhi-smaa', 'rhi-resolve',
              'rhi-scale', 'opening', 'controls', 'startup')
for name in components:
    subprocess.run([sys.executable, str(ROOT / f'tools/audit/audit-{name}.py')],
                   cwd=ROOT, check=True)
checks = [
    ('COMMON-01', 'tools/review/CMakeLists.txt', 'StartupAudit.cpp',
     'Review shares the production MusicStage/QML module, YalahooScene/Navigation/Text/Opening/Rhi and generated resources. Root Release C++23/latest-Kit and official lint remain separate build gates; no runtime module or product dependency was added.'),
    ('COMMON-02', 'tools/review/StartupAudit.cpp', 'reviewGui',
     'The GUI fixture uses invented IDs and only embedded portrait/landscape/square covers. Temporary JPEG generation belongs to the shared anonymous fixture and is removed with its QTemporaryDir; playback is never commanded. QDir/QFile/QString preserve Unicode/space paths. Output is restricted to the explicitly supplied fresh directory.'),
    ('COMMON-03', 'qml/Yalahoo/Music/pages/MusicStage.qml', 'anchors.fill: parent',
     'The same SceneItem logical viewport, source camera, instance transforms, palette, optics and postprocess produce all checkpoints. Explicit hidden DPR rotates 1/1.5/2 across sizes; it is not a physical screen event. Component source/math/format audits retain their original target gates.'),
    ('COMMON-04', 'tools/review/StartupAudit.cpp', 'browse-does-not-play',
     'Navigation, overview, detail selection, search and opening reuse production controllers and assert an empty playback source/unrequested intent. Qt mouse/key events target only this window. Metadata/codec support and physical audio are outside GUI fidelity.'),
    ('COMMON-05', 'tools/build/verify-gui.py', 'imageSha256',
     'One fresh serialized run retains source/assets/binary/image hashes, exit status and exact Cartesian checkpoint coverage; unavailable target/presentation/physical data cannot become a pass. Actual visual inspection is separate from successful structural assertions.'),
    ('PERF-01', 'src/render/SelectionController.cpp', 'void SelectionController::publish',
     'The shared production bounded 432-instance pool and four owned case slots remain independent of the 576-album library. Navigation wrapping adds constant-size width arithmetic and one eliding label invalidated by text/font/width changes, with no library traversal or new render pass. Detailed indexed lookup/real-column/overview complexity remains under component audits.'),
    ('PERF-02', 'tools/review/StartupAudit.cpp', 'd.surface.grab()',
     'Explicit checkpoint readback is Review-only and occurs after rendered-frame acknowledgement, outside measured frame timings. Existing three-worker cover pipeline/backpressure and immutable transfer remain unchanged; no capture or synchronous IO enters production.'),
    ('PERF-03', 'src/render/rhi/SceneItem.cpp', 'void SceneItem::updatePolish',
     'The driver shares the actual GUI polish -> immutable SceneFrame -> render synchronization chain. Per-state captures check the current frame revision, viewport and finite transforms. The source fixture contains no mutable external image handoff.'),
    ('PERF-04', 'src/render/rhi/RhiItem.cpp', 'resizeOldPlusNewUpperBoundBytes',
     'Actual per-checkpoint descriptor inventory and renderer generations are retained. UI transitions preserve the main target generation; local panel/overview/opening layers are declared in ResourcePlan and retire with their Loaders. Descriptor overlap is an estimate, driver residency is null.'),
    ('PERF-05', 'src/render/OverviewController.cpp', 'void OverviewController::synchronize',
     'Source component caches/compatibility keys and fixed glyph/ruler paths remain authoritative. Theme, case transforms and focus use existing uniforms/snapshots. Replaying UI states adds no product cache, resource or alternative algorithm.'),
    ('PERF-06', 'tools/review/StartupAudit.cpp', 'navigation-groups-fit',
     'Actual navigation group bounds must fit the viewport without intersection (SDE-02). Real Qt open/back/overview navigation is interleaved with three replacement selections, search modal isolation and full opening handoff. Component numeric trajectories, virtual list bounds, font shaping and CSS anchors retain independent checks; checkpoint advancement is not a continuous timing benchmark.'),
    ('PERF-07', 'tools/review/StartupAudit.cpp', 'released-owned-renderers',
     'All 36 states share the existing driver; final root/control invalidation requires zero scene/Canvas/text renderers. Default native window policy is checked. R28 owns repeated root/lifetime pressure; native same-window, physical input/VSYNC and missing targets stay in R29-P/R34/R35.'),
]
mapping = json.loads((ROOT / 'docs/contracts/history-rewrite.json').read_text(encoding='utf-8'))['revisions']
baseline = mapping['57f4f845c20cce646a15dbf4008c2a162aaaf4e2']
source_path = 'src/music-overview.css'
frozen = subprocess.check_output(['git', 'show', f'{baseline}:{source_path}'], cwd=ROOT)
if '.music-app[data-mode="detail"] .music-view-toggle' not in frozen.decode():
    raise SystemExit('Frozen detail-mode overview visibility source missing')
checks.append(('COMMON-05', 'qml/Yalahoo/Music/pages/Browse.qml', 'visible: root.contentEnabled',
               'Frozen final music-overview.css hides .music-view-toggle in detail mode. Browse now binds its visibility to the same archive interaction admission as its enabled state, including interrupted open/return; no new clock or animation is introduced.'))
checks.append(('COMMON-05', 'tools/review/compare-presentation-structure.py', "'back': ['x', 'y', 'fontSize']",
               'The independent frozen DOM comparison includes final back-button position and SDE-01 responsive font intent. The full stage additionally checks that the portrait back button starts below the actual header navigation.'))
for target, marker in (('windows', 'WIN32'), ('macos', 'YalahooVerification'), ('linux', 'UNIX AND NOT APPLE')):
    rows = []
    for identity, path, symbol, evidence in checks + [
            (dict(windows='WIN-01', macos='MAC-01', linux='LNX-01')[target],
             'cmake/Platform.cmake', marker,
             'The actual native platform backend/build branch is retained; deployment is owned by R30/R31/R32. This host source audit cannot prove another target system, compiler, GPU or display session.')]:
        data = (ROOT / path).read_bytes()
        if ' '.join(symbol.split()) not in ' '.join(data.decode().split()):
            raise SystemExit(f'GUI audit source drift: {path}::{symbol}')
        rows.append(dict(checkId=identity, requirement=evidence, platform=target, sourcePath=path,
                         lineOrSymbol=symbol, evidence=evidence,
                         sourceSha256=hashlib.sha256(data).hexdigest(), result='static-audit-passed',
                         limitations='Current runtime scope is recorded separately; R29-P retains missing target/physical/presentation requirements.'))
    output = ROOT / f'verification/{target}-static/gui'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'source-matrix.json').write_text(json.dumps(dict(
        componentAudits=list(components), checks=rows,
        frozenVisibility=dict(revision=baseline, sourcePath=source_path,
                              sourceSha256=hashlib.sha256(frozen).hexdigest()),
        runtimeReports=[dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                        for path in args.report], verifiedCheckpoints=len(merged),
        runtimeBackend=report['backend'], visualQualityAutomaticallyScored=False), ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('R29 integrated GUI COMMON/PERF/platform source closure passed')
