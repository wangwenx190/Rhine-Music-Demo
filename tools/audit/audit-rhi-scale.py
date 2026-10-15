"""Trace viewport units, cover/DOF filtering and D1-D4 history without source-image scores."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPPING = json.loads((ROOT / 'docs/contracts/history-rewrite.json').read_text())['revisions']
BASELINE = MAPPING['57f4f845c20cce646a15dbf4008c2a162aaaf4e2']

def historical(revision, path):
    data = subprocess.check_output(['git', 'show', f'{revision}:{path}'], cwd=ROOT)
    return data, dict(revision=revision, sourcePath=path, sourceSha256=hashlib.sha256(data).hexdigest())

sources = []
for path, symbol in [
    ('src/main.ts', 'void import("./music-app")'),
    ('src/music-app.ts', 'import "./music.css"'),
    ('src/music.css', 'transform: none;'),
    ('src/quality-renderer.ts', 'host.getBoundingClientRect().width / width'),
    ('src/render-quality.ts', 'Math.min(deviceRatio, quality.pixelRatio) * stageScale'),
    ('src/scene.ts', 'resizeQuality('),
    ('src/cover-atlas.ts', 'generateMipmaps'),
]:
    data, row = historical(BASELINE, path)
    if symbol not in data.decode():
        raise SystemExit(f'Frozen viewport/cover source drift: {path}::{symbol}')
    row['lineOrSymbol'] = symbol
    sources.append(row)
history = []
for revision, path, conclusion in [
    ('3fc6a39', 'src/render/RenderQuality.cpp', 'R09: retain dimension normalization, pixel guard and AO scale; caller must supply displayed units once.'),
    ('522a3f3', 'qml/Yalahoo/Music/scene/GeometryScene.qml', 'R09B: remove duplicate reference-stage scale in direct logical View3D; preserve effects and Review role.'),
    ('1f2352a', 'src/render/rhi/RhiItem.cpp', 'R10C: preserve explicit physical targets, resolve and full DOF attachments.'),
    ('75775b3', 'src/render/rhi/SceneItem.cpp', 'R10D: duplicate min(width/1920,height/1080) was not present in the final production music CSS caller; remove it.'),
    ('b7f73d4', 'src/render/rhi/RhiItem.cpp', 'R10D followup: retain bounded pools, incremental uploads and render ownership.'),
    ('e98c798', 'src/render/rhi/SceneItem.cpp', 'macOS fix: retain geometry diagnostic native-DPR protection, use one displayed-unit scale for product.'),
    ('4ab1e73', 'src/render/rhi/Postprocess.cpp', 'R19D1: preserve source AO math/half scalar and partial constant uploads.'),
    ('2f50812', 'resources/shaders/rhi/geometry.vert', 'R19D2: preserve view-space fog and one final color conversion.'),
    ('f45024d', 'src/render/rhi/RhiItem.cpp', 'R19D3A: preserve source shadow cast/bias/filter and invalidation ownership.'),
    ('e9360b7', 'src/render/rhi/RhiItem.cpp', 'R19D3A followup: inspect actual intermediate source, no target-scale exception.'),
    ('f4e1610', 'src/render/rhi/RhiItem.cpp', 'R19D3B: keep full ground in capture/AO/DOF, no artificial sharpness compensation.'),
    ('704a99d', 'src/render/rhi/Postprocess.cpp', 'R19D4A: keep 41-tap signed UV CoC, full RGB24 depth and focus in view units.'),
    ('5e333ec', 'src/render/rhi/Postprocess.cpp', 'R19D4B1: keep source SMAA at actual target size and original lookup.'),
    ('4e86136', 'src/render/rhi/RhiItem.cpp', 'R19D4B2: keep actual MSAA/resolve, combination graph and retired image gates.'),
]:
    _, row = historical(revision, path)
    row['evidence'] = conclusion
    history.append(row)

CHECKS = [
    ('COMMON-01', 'tools/review/CMakeLists.txt', 'RhiScaleAudit.cpp', 'Review-only executable links actual YalahooRhi; ordinary app uses the same production SceneItem, no alternative rendering algorithm.'),
    ('COMMON-02', 'tools/review/RhiProductionFixture.h', 'row < 64', 'Same anonymous nine-column 576 album snapshot, existing square/portrait/landscape qrc assets; no private paths or user song writes.'),
    ('COMMON-03', 'src/render/rhi/SceneItem.cpp', 'quality.resize(width(), height(), 1,', 'Direct viewport item has no outer CSS scale; source host bounding/client ratio is one. DPR cap, quality scale, floor and 8294400 guard remain RenderQuality responsibility.'),
    ('COMMON-04', 'tools/review/RhiScaleAudit.cpp', 'sourceTarget', 'Independent source dimension math checked against actual attachments, explicit RenderControl target DPR, final image dimensions and source focus/CoC. Media has no changed calls or commands: N/A.'),
    ('COMMON-05', 'tools/review/PostprocessChecks.h', 'Postprocess color/depth filtering differs from source', 'Actual SRB chain and sampler descriptors, full-size nearest RGB24 depth, linear color and isolated transmission mips; synthetic source formulas are separate from own-image visual inspection.'),
    ('PERF-01', 'src/render/rhi/SceneItem.cpp', 'const int count = int(in.scene.instances.size())', 'Unchanged bounded 432+4 production pool. Dimension fix changes pixel workload to the source value; it does not expand entity count or library traversal.'),
    ('PERF-02', 'tools/review/RhiScaleAudit.cpp', 'waitForCovers', 'Cover workers remain globally bounded and asynchronous. Review waits on actual frame/decode callbacks with deadline. Explicit captures only; no product readback or polling.'),
    ('PERF-03', 'src/render/rhi/SceneItem.cpp', 'm_coverRevision == m_publishedCoverRevision', 'Existing immutable snapshots and revision coalescing remain. Target resize reuses atlas identities/leases; numerical state retains 80-byte per-pass prefixes.'),
    ('PERF-04', 'src/render/rhi/RhiItem.cpp', 'coverLinearClampNoMip', 'Actual descriptors inventory includes physical item, geometry, full DOF, AO, resolve/mips/MSAA and fixed atlas. Source cover tiles have no cross-tile mip; transmission has its own mip chain.'),
    ('PERF-05', 'tools/review/RhiScaleAudit.cpp', 'Theme rebuilt compatible targets', 'Theme/focus are numerical uniform changes, not compatibility changes. No sharpening, extra blur, quality override, LOD approximation or new timer.'),
    ('PERF-06', 'src/render/rhi/SceneItem.cpp', '&QQuickWindow::devicePixelRatioChanged', 'Window DPR signal schedules the existing coalesced polish; old window/screen connections are disconnected on reassignment. Input, single clock and playback are unchanged.'),
    ('PERF-07', 'tools/review/OffscreenSurface.h', 'QCoreApplication::sendEvent(&m_window, &change)', 'External RenderControl DPR changes explicitly deliver the standard Qt window event before polish; actual target DPR and same source state are independently checked. Serial real resources with current/old-plus-new descriptor estimates and null driver VRAM. Shared full regression retains ten SG rebuilds/continuous motion. Native screen changes/VSYNC and missing target devices remain R34.'),
]
for platform, marker in [('windows', 'WIN32'), ('macos', 'YalahooVerification'), ('linux', 'UNIX AND NOT APPLE')]:
    rows = []
    for identity, path, symbol, evidence in CHECKS + [(platform.upper() + '-01', 'cmake/Platform.cmake', marker, 'Native backend/compiler branch preserved. Same-Kit target build and actual target GPU/window/DPR execution required; host qsb is translation only.')]:
        data = (ROOT / path).read_bytes()
        if ' '.join(symbol.split()) not in ' '.join(data.decode().split()):
            raise SystemExit(f'Scale audit source drift: {path}::{symbol}')
        rows.append(dict(checkId=identity, requirement=evidence, platform=platform, sourcePath=path,
                         lineOrSymbol=symbol, evidence=evidence, sourceSha256=hashlib.sha256(data).hexdigest(),
                         result='static-audit-passed', limitations='Runtime/actual visual inspection separate; physical display, native DPR transitions, VSYNC and full product performance are not proved by offscreen output.'))
    output = ROOT / 'verification' / f'{platform}-static' / 'rhi-scale'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'source-matrix.json').write_text(json.dumps(dict(checks=rows, frozen=sources, history=history), indent=2) + '\n')
print('Viewport units, cover/DOF sampling, D1-D4 history and COMMON/PERF/platform source audit passed')
