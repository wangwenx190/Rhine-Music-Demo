"""R19F provenance and call-path review; runtime and visual quality remain separate."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPPING = json.loads((ROOT / 'docs/contracts/history-rewrite.json').read_text())['revisions']
BASELINE = MAPPING['57f4f845c20cce646a15dbf4008c2a162aaaf4e2']


def record(revision, path, reason):
    data = subprocess.check_output(['git', 'show', f'{revision}:{path}'], cwd=ROOT)
    return dict(revision=revision, sourcePath=path,
                sourceSha256=hashlib.sha256(data).hexdigest(), evidence=reason)


frozen = [record(BASELINE, 'src/' + name, reason) for name, reason in [
    ('music-app.ts', 'Production title/transport callers and stylesheet order.'),
    ('style.css', 'MiSans CSS weight mapping and font-synthesis:none.'),
    ('music.css', 'Final title 1.4/translation .45 and 1.45 line boxes, mask/padding/fit sizes.'),
    ('music-title.ts', 'Whole-line DOM ranges, fit coarse/fine search, split/ellipsis and queued layout.'),
    ('music-title-reels.ts', '460ms analytic spring, 49 samples, visible face retention and bounded idle slots.'),
    ('music-transport-title.css', 'Song width/offset/opacity, 29px line and rolling-number blur/hold.'),
    ('music-transport-title.ts', 'Song updates retain locate-only action; playing state independent of browsing.'),
]]
frozen.append(record(BASELINE, 'package-lock.json', 'Locked rolling-number dependency and integrity; export-title validates actual package and hashes its sampled implementation.'))
history = [record(rev, path, reason) for rev, path, reason in [
    ('2abb4f6', 'src/render/text/TextLayoutAdapter.cpp', 'R16: preserve whole-line QTextLayout/string-index association, grapheme grouping, fractional local scale and original font mapping. Contextual forms cannot use per-face fillText as production.'),
    ('2abb4f6', 'qml/Yalahoo/Music/components/TitleReel.qml', 'R16: remove twice-DPR slot allocation: no source caller or API necessity. Keep source local edge fade/transport blur and Qt linear layer filter at actual DPR; CurveRendering retains vector semantics.'),
    ('6636ddb', 'src/render/text/ReelInk.cpp', 'R16B1: retain bounded descriptor matching/reuse and render ownership, independent of image acceptance.'),
    ('6636ddb', 'src/render/text/TextLayoutAdapter.cpp', 'R16B1: trim width endpoint as well as text to inkEnd; required whole-line trailing-space semantics.'),
    ('509afe8', 'tools/review/CanvasTitleProbe.cpp', 'R16B2: retain effective visibility in synchronize/paint, window disconnect and actual cost counters; candidate still reshapes faces and is not semantically equivalent.'),
    ('07514a5', 'src/render/text/TextLayoutAdapter.cpp', 'R16B2V: actual QTextLine ascent includes fallback; subtract it to retain primary CSS baseline. Color inline face retains separate metrics, not an image offset.'),
    ('07514a5', 'tools/review/TitleAudit.cpp', 'R16B2V: restore title dimensions before lifecycle rendering; preserve real invalidation/one pending readback. B2V-P had no separate implementation; no pixel repair task survives.'),
    ('6740abc', 'QT_QUICK_MIGRATION_PLAN.md', 'Handoff only: no separate B2W/B3 production implementation; shared GL/DPR/candidate branches are reviewed here; target runtime remains R36.'),
    ('c7e9b61', 'tools/review/OffscreenSurface.h', 'R16M1: preserve true RenderControl target with explicit DPR, hidden production-state consumers and font-head identity.'),
    ('b9a5ea7', 'src/app/AssetCatalog.cpp', 'R16M2: retain public Apple Color Emoji family policy; source PostScript face differs from UI fallback. Initialize before controller, rollback missing faces, no font file path in render.'),
    ('9470126', 'src/render/text/TextLayoutAdapter.cpp', 'R16M2P: retain macOS integral half-leading only with same-host DOM rangeTop/Canvas font-box metric proof; no screenshot or case-dependent baseline. Other platforms unchanged.'),
    ('2840110', 'src/render/text/TitleReel.cpp', 'R16M3: retain changed-only natural width and one object-owned queued hidden finish, canceled by restore; prevents QML binding reentry.'),
    ('2840110', 'qml/Yalahoo/Music/components/TransportTitle.qml', 'R16M3: content/motion visibility retains exit width and forbids exit click; not render-score compensation.'),
    ('33e8260', 'src/render/text/TitleReel.cpp', 'R17 followup: preserve font database invalidation of full measurement cache.'),
    ('e98c798', 'tools/review/TitleAudit.cpp', 'macOS followup: keep bounded backend readback drain and basic native surface teardown; hidden SG rebuild does not prove native hide/restore.'),
]]
CHECKS = [
    ('COMMON-01', 'src/render/text/CMakeLists.txt', 'YALAHOO_REEL_GLYPH_API', 'Latest Kit C++23 compile probe isolates private glyph API. Product/ui-audit/TitleTest link same text/assets/QML/shader closure.'),
    ('COMMON-02', 'src/app/AssetCatalog.cpp', 'setApplicationEmojiFontFamilies', 'Embedded original SFNT physical table selection and public macOS emoji face policy precede QML. No user data, absolute font path or network in text pipeline.'),
    ('COMMON-03', 'resources/shaders/reel-slot.frag', 'qt_Opacity', 'Qt Quick RGBA8 layer at ceil(capacity*actual DPR), linear filter, local source fade/blur and premultiplied output; no extra projection/resolve/conversion.'),
    ('COMMON-04', 'qml/Yalahoo/Music/components/TransportTitle.qml', 'locateRequested', 'Visual song button only emits locate-song. Playback IDs stay independent; real media and locate execution remain R24/R23.'),
    ('COMMON-05', 'tools/review/export-title.mjs', 'source-structure-font-metrics-motion-v1', 'Frozen source states, DOM font boxes, physical font identity and exact numeric tracks are checked independently of own-output quality.'),
    ('PERF-01', 'src/render/text/TitleReel.cpp', 'm_animatedSlots', 'Stationary advance O(1), active slots only; indexed pool <=64 idle. Fit <=25 line measurements, final glyph grouping O(n log n), independent of library size.'),
    ('PERF-02', 'src/render/rhi/ReelTextNode.cpp', 'QRawFont::fromFont', 'GUI sends implicitly shared value descriptors, not GUI QRawFont. Render resolves and verifies head before glyph IDs. One coalesced reconcile; no hot-path IO/wait/readback.'),
    ('PERF-03', 'src/render/text/ReelInk.cpp', 'sameGlyphInk', 'At most eight faces per slot matched locally, O(f squared); whole-run context/font/position invalidates content. Animation updates transform/clip/color without rebuilding text.'),
    ('PERF-04', 'src/render/text/TitleReel.cpp', 'slot->capacityWidth * m_dpr', 'Actual-DPR layer sizes shared with resource descriptors; RGBA8 plus conservative depth and old/new overlap counted by ResourcePlan. QML layerSizes checks actual vs planned sizes. Font atlas/driver bytes unknown.'),
    ('PERF-05', 'src/render/text/TextLayoutAdapter.cpp', 'CacheBytes', 'Complete input/font/width/budget/DPR key, 96 entries/8MiB; fontDatabaseChanged clears. Fractional scale preserves CSS geometry without inflated font size. Mac half-leading independently checked via source font metrics.'),
    ('PERF-06', 'qml/Yalahoo/Music/components/TransportTitle.qml', 'transition.moving', 'Unique scaled FrameClock, exit retains content/width and forbids click. Hidden layers/Canvas stop text work; only Review explicitly renders hidden roots.'),
    ('PERF-07', 'tools/review/TitleAudit.cpp', 'restorePending.exchange(false)', 'One readback/backend bounded drain, ten actual SG invalidations/restore captures and final zero tracked nodes/renderers; no native window/VSYNC claim from RenderControl.'),
]
current_paths = {row[1] for row in CHECKS} | {
    'CMakeLists.txt', 'cmake/BrowseQml.cmake', 'src/app/GraphicsBackend.h',
    'src/render/text/TextLayoutAdapter.h', 'src/render/text/TitleReel.h',
    'src/render/text/ReelInk.cpp', 'src/render/text/TransportMotion.cpp',
    'src/render/rhi/SceneItem.cpp', 'src/render/rhi/ResourcePlan.h',
    'qml/Yalahoo/Music/components/TitleReel.qml', 'qml/Yalahoo/Music/components/ArchiveText.qml',
    'qml/Yalahoo/Music/components/Header.qml', 'tests/unit/TitleTest.cpp',
    'tools/build/verify-title.mjs', 'tools/build/verify-r16.ps1',
    'tools/audit/audit-title.mjs', 'tools/audit/audit-r16.ps1',
    'tools/review/CanvasTitleProbe.cpp', 'tools/review/OffscreenSurface.h',
    'tools/review/title-harness.js',
}
current = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest())
           for path in sorted(current_paths)]
for platform, marker in [('windows', 'WIN32'), ('macos', 'YalahooVerification'), ('linux', 'UNIX AND NOT APPLE')]:
    rows = []
    for identity, path, symbol, reason in CHECKS + [(platform.upper() + '-01', 'cmake/Platform.cmake', marker, 'Native platform compiler/backend/resources preserved; cross-target shader generation is not target runtime.')]:
        data = (ROOT / path).read_bytes()
        if ' '.join(symbol.split()) not in ' '.join(data.decode().split()):
            raise SystemExit(f'Title source drift: {path}::{symbol}')
        rows.append(dict(checkId=identity, requirement=reason, platform=platform, sourcePath=path,
                         lineOrSymbol=symbol, evidence=reason,
                         sourceSha256=hashlib.sha256(data).hexdigest(), result='static-audit-passed',
                         limitations='Runtime, own-output quality, physical DPR/input/VSYNC and deployment require separate evidence; missing target environment remains R36.'))
    directory = ROOT / 'verification' / f'{platform}-static' / 'title-reels' / 'r19f'
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'source-matrix.json').write_text(json.dumps(dict(frozen=frozen, history=history, current=current, checks=rows), indent=2) + '\n')
print('R19F history, COMMON/PERF and three-platform call-path audit passed')
