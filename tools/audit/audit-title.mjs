import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';

const root = path.resolve(import.meta.dirname, '../..');
if (process.argv.length !== 3)
    throw Error('Usage: node tools/audit/audit-title.mjs complete-diagnostic-directory');
const evidence = path.resolve(process.argv[2]);
const readJson = (file) => JSON.parse(fs.readFileSync(file, 'utf8'));
const summary = readJson(path.join(evidence, 'summary.json'));
const report = readJson(path.join(evidence, 'report.json'));
const source = readJson(path.join(summary.sourceDirectory, 'report.json'));
if (!summary.diagnosticsPassed || summary.platform !== 'darwin' || summary.backend !== 'metal'
    || summary.configuration !== 'Release' || summary.dpr !== 2 || !report.offscreen
    || report.offscreenResourcePlan?.nativeWindowVisible !== false || !report.windowPolicy.passed
    || !report.released)
    throw Error('Complete hidden Metal/DPR2 diagnostics required');
const sourceCaptures = source.captures.filter((capture) => Math.abs(capture.dpr - 2) < .001);
if (source.platform !== 'darwin' || sourceCaptures.length !== 20)
    throw Error('Twenty same-platform DPR2 source captures required');
for (const index of [10, 11]) {
    const expected = sourceCaptures[index].platformFonts.filter(
        (face) => face.fonts.some((font) => font.postScriptName === 'AppleColorEmoji'));
    const actual = report.captures.find((capture) => capture.case === index && capture.mode === 0);
    const colors = actual?.geometry.actualGlyphs.filter((glyph) => glyph.colorGlyph);
    if (expected.length !== 4 || !actual || !actual.geometry.passed || !actual.candidatePassed
        || colors.length !== 4 || colors.some((glyph, i) => glyph.text !== expected[i].text)
        || colors.some(
            (glyph) => glyph.fonts.length !== 1 || glyph.fonts[0].family !== 'Apple Color Emoji'
                       || !glyph.fonts[0].headSha256 || glyph.fonts[0].glyphIndexes.length !== 1))
        throw Error('Source/native emoji face and whole-line glyph evidence required');
}
const chain = readJson(path.join(root, 'build/release/toolchain.json'));
const qsb = [path.join(chain.qtKit, 'bin/qsb'), path.join(chain.qtKit, 'libexec/qsb')].find(
    (file) => fs.existsSync(file));
const shader = path.join(root, 'build/release/src/app/.qsb/reel-slot.frag.qsb');
const dump = spawnSync(qsb, ['--dump', shader], {encoding: 'utf8', windowsHide: true});
if (dump.status !== 0 || !['MSL', 'GLSL', 'HLSL'].every((kind) => dump.stdout.includes(kind)))
    throw Error('Shared text shader must contain MSL/GLSL/HLSL variants');
fs.writeFileSync(path.join(evidence, 'reel-slot-qsb.txt'), dump.stdout);
// Symbol guards detect source drift. The evidence text records the manually reviewed
// call-chain deductions; it is not inferred from the presence of these strings.
const checks = [
    [
        'COMMON-01', 'src/render/text/CMakeLists.txt', 'YALAHOO_REEL_GLYPH_API',
        'The root requires C++23 and the latest discovered native Kit. YalahooText links Quick; ' +
            'QuickPrivate is limited to the glyph adapter and probed by compiling addGlyphs. ' +
            'AssetCatalog is compiled in the product, ui-audit and TitleTest. The new font policy '
            +
            'uses public QFontDatabase behind Q_OS_MACOS; Windows/Linux keep their previous branch.'
    ],
    [
        'COMMON-02', 'src/app/AssetCatalog.cpp', 'setApplicationEmojiFontFamilies',
        'initializeAssetFonts validates four embedded SFNT outlines before committing fonts. ' +
            'macOS then requires the installed Apple Color Emoji family and selects it once. ' +
            'Missing family rolls back the registered faces and returns failure; no downloaded ' +
            'font, absolute system-font path or font-file I/O enters production rendering. ' +
            'QML, C++ shaping and Canvas all inherit this application policy.'
    ],
    [
        'COMMON-03', 'resources/shaders/reel-slot.frag', 'textureHeight',
        'Each slot uses the existing RGBA8 layer and Qt Quick matrix/opacity/binding ABI. ' +
            'The shader applies the unchanged edge fade and optional bounded nine-tap blur. ' +
            'No new color conversion, sampler, attachment, resolve or projection was added. ' +
            'The native qsb dump has MSL/GLSL/HLSL; generation does not prove target GPU behavior.'
    ],
    [
        'COMMON-04', 'qml/Yalahoo/Music/components/TransportTitle.qml', 'locateRequested',
        'Transport remains visual state plus a locate signal, forwarded through Header. ' +
            'Font selection has no media/provider/file-scanner dependency. Actual audio and ' +
            'locate execution remain the dedicated migration rounds.'
    ],
    [
        'COMMON-05', 'tools/review/export-title.mjs', 'CSS.getPlatformFontsForNode',
        'Frozen Git mapping supplies source CSS/markup/WOFF2. CDP reads actual rendered ' +
            'face text and platform family/PostScript identity without changing source layout. ' +
            'Resolved remote objects are released per face. Native geometry records measured ' +
            'font-head hashes and whole-line IDs/positions. verify-title still rejects structure, '
            +
            'header, motion, cache, hidden-work or lifecycle failure in diagnostic mode; remaining '
            +
            'pixel cases stay explicit and do not count as accepted pixels.'
    ],
    [
        'PERF-01', 'src/render/text/TitleReel.cpp', 'm_animatedSlots',
        'advance visits only moving slots with O(1) stationary return. Reconcile measures ' +
            'input text once using cached fit results, an indexed slot pool and at most 64 idle ' +
            'slots. Text shaping is bounded by input length, not library size. Emoji policy is ' +
            'set only at startup; new diagnostics run only during capture, never every tick.'
    ],
    [
        'PERF-02', 'src/render/rhi/ReelTextNode.cpp', 'QRawFont::fromFont',
        'GUI layout retains font descriptions/head identity and implicitly shared glyph ' +
            'values. The render thread resolves its own QRawFont and checks head identity ' +
            'before using IDs. One queued reconcile coalesces GUI input; no new worker, wait ' +
            'or production readback exists. Review permits only one readback in flight.'
    ],
    [
        'PERF-03', 'src/render/text/ReelInk.cpp', 'sameGlyphInk',
        'Content revision reuses complete identical glyph descriptors in a bounded face ' +
            'list (at most eight). Animation changes transforms/clip/opacity independently. ' +
            'Color faces use Qt Quick Text; hidden ordinary nodes are deleted. The font policy ' +
            'is installed before controllers, so it cannot invalidate a live layout each frame.'
    ],
    [
        'PERF-04', 'src/render/rhi/ResourcePlan.h', 'title-reel-depth',
        'Slot capacity/DPR determines the existing layer at twice DPR; RGBA8 and conservative ' +
            'depth estimates remain in ResourcePlan, with resize overlap recorded separately. ' +
            'Font choice creates no new product render target and leaves 3D compatibility keys ' +
            'unchanged. OffscreenSurface owns one RGBA8 plus depth/stencil target, single sample, '
            +
            'no mips; its target size/DPR changes only on corresponding compatibility changes. ' +
            'Qt font atlas and driver memory remain unknown.'
    ],
    [
        'PERF-05', 'src/render/text/TextLayoutAdapter.cpp', 'line.ascent() * scale',
        'Primary CSS half-leading and mixed-line ascent correction are unchanged. Emoji ' +
            'now resolves to the source face while whole-line QTextLayout still supplies ' +
            'grapheme/ligature IDs and positions. The full cache key and 96-entry/8MiB limits ' +
            'are unchanged; fontDatabaseChanged clears caches. No case-dependent offsets, ' +
            'font-size adjustment or per-character shaping is introduced.'
    ],
    [
        'PERF-06', 'qml/Yalahoo/Music/components/TitleReel.qml', 'root.clock',
        'The same FrameClock controls all motion. Hidden normal/Canvas candidates skip ' +
            'text work; offscreenRendering explicitly permits the hidden review root. QML ' +
            'faces retain Tooltip/Accessible full names and only visible color/candidate Text ' +
            'instances. Header checks cover both themes, nine sizes and unchanged playback IDs.'
    ],
    [
        'PERF-07', 'tools/review/OffscreenSurface.h', 'm_pending',
        'Render requests coalesce into one queued callback. hide invalidates scene graph ' +
            'before owned attachments are destroyed; restore initializes it again. Target DPR ' +
            'is explicit. Ten independent cycles and final zero tracked nodes/renderers are ' +
            'checked by the diagnostic wrapper. Review texture readback is bounded and separate ' +
            'from production; its GUI sync/render/endFrame timings are not VSYNC performance.'
    ],
    [
        'MAC-01', 'src/app/GraphicsBackend.h', 'Metal',
        'The Apple branch selects Metal. Apple Color Emoji is verified against the same-host ' +
            'browser identity and four color glyphs; the private UI family is no longer used. ' +
            'prepareApplication/prepareWindow and the native Cocoa guard remain in place. ' +
            'Actual diagnostics used RenderControl with no native show or swapchain present. ' +
            'Visible interaction and deployment remain R34/R31.'
    ],
    [
        'LNX-01', 'src/app/AssetCatalog.cpp', '#ifdef Q_OS_MACOS',
        'The new family selection is removed by preprocessing on Linux and Windows. ' +
            'Shared layout/render descriptors, qrc aliases and shader path are unchanged. ' +
            'GraphicsBackend selects OpenGL for Linux and D3D11 for Windows, with the existing ' +
            'explicit OpenGL option. No Apple/Windows handle or library enters another platform. ' +
            'Native target fonts, compositor, deployment and GPU results remain unverified.'
    ]
];
const sourcePaths = new Set(checks.map((check) => check[1]));
for (const file
         of ['CMakeLists.txt', 'cmake/BrowseQml.cmake', 'src/render/text/TextLayoutAdapter.h',
             'src/app/VerificationWindow.h', 'src/app/VerificationWindowMac.mm', 'src/app/main.cpp',
             'src/app/CMakeLists.txt', 'tools/review/UiAudit.cpp', 'tools/review/CMakeLists.txt',
             'tests/unit/CMakeLists.txt', 'tools/review/TitleAudit.cpp',
             'tools/review/CanvasTitleProbe.cpp', 'tools/build/verify-title.mjs',
             'tests/unit/TitleTest.cpp'])
    sourcePaths.add(file);
const records = [...sourcePaths].map(
    (file) => ({
        path: file,
        sha256: createHash('sha256').update(fs.readFileSync(path.join(root, file))).digest('hex')
    }));
for (const [checkId, file, symbol] of checks)
    if (!fs.readFileSync(path.join(root, file), 'utf8').includes(symbol))
        throw Error('Reviewed source drift: ' + checkId + ':' + file + ':' + symbol);
for (const platform of ['macos', 'linux']) {
    const directory = path.join(root, 'verification', platform + '-static/title-reels/r16m2');
    fs.mkdirSync(directory, {recursive: true});
    const conclusion = '静态审计通过，实机待用户验证';
    const matrix = checks.map(
        ([checkId, sourcePath, lineOrSymbol, evidence]) => ({
            checkId,
            requirement: checkId.startsWith('PERF') ? 'docs/contracts/performance.md'
                                                    : 'QT_QUICK_MIGRATION_PLAN.md',
            platform,
            sourcePath,
            lineOrSymbol,
            evidence,
            result: 'static-passed',
            limitations:
                platform === 'macos'
                    ? 'Real arm64 Release/Metal/DPR2 offscreen diagnostics ran; visible native ' +
                          'windows/present/VSYNC and remaining pixels are not accepted.'
                    : 'No Linux compiler, color-font backend, compositor or GPU ran. Windows ' +
                          'shared code is unchanged but Windows runtime was not available this round.'
        }));
    fs.writeFileSync(path.join(directory, 'audit-matrix.json'), JSON.stringify({
        conclusion,
        round: 'R16M2',
        toolchain: chain,
        sourceRecords: records,
        diagnosticDirectory: evidence,
        pendingPixelCases: summary.pendingPixelCases,
        checks: matrix
    },
                                                                               null, 2)
                                                                    + '\n');
    fs.writeFileSync(path.join(directory, 'audit-report.md'),
                     '# R16M2 / ' + platform + '\n\n' + conclusion + '\n\n'
                         + matrix
                               .map((check) => check.checkId + ' — ' + check.sourcePath + ':'
                                               + check.lineOrSymbol + '\n' + check.evidence
                                               + '\nLimit: ' + check.limitations)
                               .join('\n\n')
                         + '\n');
}
console.log('R16M2 source/performance audit and text qsb variants passed; pixels pending: '
            + summary.pendingPixelCases.join(','));
