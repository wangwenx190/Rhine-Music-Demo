"""R19E source provenance and call-path audit; runtime results are separate."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPPING = json.loads((ROOT / 'docs/contracts/history-rewrite.json').read_text())['revisions']
BASELINE = MAPPING['57f4f845c20cce646a15dbf4008c2a162aaaf4e2']


def historical(revision, path, reason):
    data = subprocess.check_output(['git', 'show', f'{revision}:{path}'], cwd=ROOT)
    return dict(revision=revision, sourcePath=path,
                sourceSha256=hashlib.sha256(data).hexdigest(), evidence=reason)


frozen = [historical(BASELINE, 'src/' + name, reason) for name, reason in [
    ('music-app.ts', 'Production markup/actions and stylesheet import precedence.'),
    ('music.css', 'Final responsive callout/grid/footer overrides and CSS inline strut.'),
    ('music-theme.css', 'Palette, linear reading veil and transition colors.'),
    ('music-theme-switch.css', 'Final portrait header grid and song-cap 280/width-320 override.'),
    ('music-transport-title.css', 'Landscape caps, growing song slot, fixed right playback buttons.'),
    ('music-navigation-ruler.css', 'Full-height slot hit region, rect ticks and continuous alpha mask.'),
    ('music-ruler.ts', 'Sixteen slots, source springs/recycle/ripple/focus and CSS measure.'),
    ('music-wheel-navigation.ts', 'Wheel sign/48px/three-step cap, ownership/reset boundaries.'),
    ('music-text-motion.ts', 'Ordinal occurrence direction and locked rolling-number caller.'),
    ('viewport-layout.ts', 'Logical viewport and 1.05 portrait division.'),
]]
history = [historical(rev, path, reason) for rev, path, reason in [
    ('1a082f2', 'qml/Yalahoo/Music/components/ArchiveText.qml', 'R14: retain local fractional vector scale, CSS line-box metrics and explicit source padding; detailed title font semantics are R19F.'),
    ('1a082f2', 'qml/Yalahoo/Music/components/ArchiveButton.qml', 'R14: remove image-driven fourfold SVG raster size; preserve source geometry and actual output DPR.'),
    ('1a082f2', 'tools/review/UiAudit.cpp', 'R14: own output, CPU diagnostics and lifecycle.'),
    ('2ee2294', 'src/render/navigation/RulerController.cpp', 'R15: retain source exact springs, bounded slot matching and existing single clock.'),
    ('2ee2294', 'src/render/navigation/InputRouter.cpp', 'R15: preserve Qt unit/sign/ownership adapters and one replaceable destination for four-cover backpressure.'),
    ('2ee2294', 'src/render/navigation/OrdinalMotion.cpp', 'R15: retain 49-point 460ms dependency track, occurrence direction and bounded faces.'),
    ('2ee2294', 'tools/review/RulerAudit.cpp', 'R15: frozen states remain <=1e-6; Canvas2D and C++ use Qt default antialiasing at actual target DPR.'),
    ('2abb4f6', 'qml/Yalahoo/Music/components/Header.qml', 'R16: restore final portrait CSS song-cap precedence; preserve transport exit width controller pending R19F font audit.'),
    ('c7e9b61', 'tools/review/UiAudit.cpp', 'Apple Silicon: retain actual RenderControl surface; no frontmost window is required for Canvas checks.'),
    ('e98c798', 'tools/review/UiAudit.cpp', 'macOS followup: retain opt-in native visible path and default hidden window policy.'),
    ('1c9b8c6', 'qml/Yalahoo/Music/components/Header.qml', 'R19: preserve overview navigation enable/opacity delegation; overview timing belongs R19G.'),
]]
CHECKS = [
    ('COMMON-01', 'tools/review/CMakeLists.txt', 'Qt6::CanvasPainter', 'Same discovered native Kit/C++23; C++ CanvasPainter production, Canvas2D only Review, QML/shader assets compiled in both closures.'),
    ('COMMON-02', 'qml/Yalahoo/Music/components/Header.qml', 'qrc:/assets/branding/logo-', 'Embedded fonts/icons/brand retain author/license chain; no current-directory or private-library dependency.'),
    ('COMMON-03', 'qml/Yalahoo/Music/components/ArchiveButton.qml', 'width * outputDpr', 'SVG raster at actual window DPR; settings Curve Shape retains round caps/circle layering; no fourfold sampling or new target.'),
    ('COMMON-04', 'src/render/navigation/InputRouter.cpp', 'ownsInput', 'Qt routes native events before input owner/ancestor checks; navigation only reaches SelectionController. No media calls, queue or playback changes.'),
    ('COMMON-05', 'tools/review/compare-browse-structure.py', "delta <= 1", 'Frozen final CSS anchors, numeric motion and actual visual inspection are separate; reports never treat missing target execution as passed.'),
    ('PERF-01', 'src/render/navigation/RulerController.cpp', 'result.reserve(16)', 'Fixed sixteen slots, O(16 squared) regroup and indexed album lookup; at most ten digits/two faces, no per-frame full-library scan.'),
    ('PERF-02', 'src/render/navigation/InputRouter.cpp', 'm_pending = target', 'One replaceable absolute destination, bounded event step/remainder, cancel on library/mode and rebase through shared selection.'),
    ('PERF-03', 'src/render/navigation/RulerInk.cpp', 'controller()->drawing()', 'GUI/render sync copies only fixed drawing values/QColors; once-published implicit-shared QML slot list, no pixel copies or I/O in production paint.'),
    ('PERF-04', 'src/render/rhi/ResourcePlan.h', 'navigation-ruler-color', 'Existing 3D -> reading veils -> UI/ordinal/ruler composition. Fixed single-sample transparent Canvas attachment and bounded digit layers; actual-DPR descriptor budget, driver VRAM unknown.'),
    ('PERF-05', 'src/render/navigation/RulerInk.cpp', 'removePathGroup', 'Three renderer-owned actual-size paths removed only on geometry change; moving heights use rects, full ticks clipped by Quick, source continuous alpha mask.'),
    ('PERF-06', 'qml/Yalahoo/Music/components/OrdinalReel.qml', 'root.tokens.offscreenRendering', 'Unique FrameClock, effective visibility/Review-only offscreen switch; production hidden window remains inactive. Fixed button pool, disabled invisible overscan and focus handoff.'),
    ('PERF-07', 'tools/review/RulerAudit.cpp', 'surface->hide()', 'One asynchronous readback with backend-bounded drain; actual full-window/graph invalidation, resize/restore and renderer release; an owned epoch rejects old callbacks and a basic-loop cleanup frame retires queued renderer deletions. No production readback or timer added.'),
]
for platform, marker in [('windows', 'WIN32'), ('macos', 'YalahooVerification'), ('linux', 'UNIX AND NOT APPLE')]:
    rows = []
    for identity, path, symbol, reason in CHECKS + [(platform.upper() + '-01', 'cmake/Platform.cmake', marker, 'Platform native compiler/backend branch preserved; host generation does not prove target execution.')]:
        data = (ROOT / path).read_bytes()
        if ' '.join(symbol.split()) not in ' '.join(data.decode().split()):
            raise SystemExit(f'R19E source drift: {path}::{symbol}')
        rows.append(dict(checkId=identity, requirement=reason, platform=platform, sourcePath=path,
                         lineOrSymbol=symbol, evidence=reason,
                         sourceSha256=hashlib.sha256(data).hexdigest(), result='static-audit-passed',
                         limitations='Actual target GPU, native input/window, physical DPR/VSYNC and deployment require runtime evidence; R36 retains missing targets.'))
    output = ROOT / 'verification' / f'{platform}-static' / 'browse-navigation'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'source-matrix.json').write_text(json.dumps(dict(frozen=frozen, history=history, checks=rows), indent=2) + '\n')
print('R19E frozen/history/call-path COMMON/PERF/platform audit passed')
