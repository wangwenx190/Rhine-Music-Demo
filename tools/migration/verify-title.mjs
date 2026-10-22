import {validateTitleState} from './title-report.mjs';
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';

const root = path.resolve(import.meta.dirname, '../..');
const platform = process.platform === 'win32'    ? 'windows'
                 : process.platform === 'darwin' ? 'macos'
                                                 : 'linux';
const options = {
    backend: platform === 'windows' ? 'd3d11'
             : platform === 'macos' ? 'metal'
                                    : 'opengl',
    dpr: 2,
    source: path.join(root, 'verification', platform, 'title-reels/web')
};
let visible = false;
const seen = new Set();
const args = process.argv.slice(2);
for (let i = 0; i < args.length; ++i) {
    const option = args[i];
    if (seen.has(option))
        throw Error('Duplicate option: ' + option);
    seen.add(option);
    if (option === '--visible') {
        visible = true;
    } else {
        if (!['--backend', '--dpr', '--source', '--output'].includes(option) || !args[i + 1])
            throw Error(
                'Usage: verify-title.mjs [--backend metal|d3d11|d3d12|vulkan|opengl] [--dpr 1|1.5|2] '
                +
                '[--source directory] [--output directory] [--visible]');
        options[option.slice(2)] = args[++i];
    }
}
const dpr = Number(options.dpr);
if (![1, 1.5, 2].includes(dpr)
    || !['metal', 'd3d11', 'd3d12', 'vulkan', 'opengl'].includes(options.backend))
    throw Error('Unsupported backend or DPR');
const source = path.resolve(options.source);
const output = options.output ? path.resolve(options.output)
                              : path.join(root, 'verification', platform, 'title-reels',
                                          options.backend + '-dpr-' + dpr + '-' + Date.now());
if (fs.existsSync(path.join(output, 'report.json')))
    throw Error('Use a fresh output directory to retain earlier evidence');
const reference = JSON.parse(fs.readFileSync(path.join(source, 'report.json'), 'utf8'));
if (reference.captures.filter((capture) => Math.abs(capture.dpr - dpr) < 0.001).length !== 20)
    throw Error('Generate twenty frozen source states at the requested DPR first');
if (reference.protocol !== 'source-structure-font-metrics-motion-v1')
    throw Error('Regenerate source metrics with the current non-image exporter');
if ((reference.platform && reference.platform !== process.platform)
    || (process.platform !== 'win32' && !reference.platform))
    throw Error('Source captures must be generated on this platform');
const cache = fs.readFileSync(path.join(root, 'build/release/CMakeCache.txt'), 'utf8');
const kit = cache.match(/^YALAHOO_SELECTED_QT:PATH=(.+)$/m)?.[1].trim();
if (!kit || !/^CMAKE_BUILD_TYPE:STRING=Release\r?$/m.test(cache))
    throw Error('Configure and build the native qt-release preset first');
const environment = {
    ...process.env,
    PATH: path.join(kit, 'bin') + path.delimiter + process.env.PATH,
    QT_PLUGIN_PATH: path.join(kit, 'plugins'),
    QSG_RHI_BACKEND: options.backend,
    QSG_RENDER_LOOP: process.platform === 'darwin' && options.backend === 'opengl' ? 'basic'
                                                                                   : 'threaded',
    QCPAINTER_DEBUG_COLLECT: '0',
    QT_SCALE_FACTOR: '1'
};
delete environment.QSG_NO_VSYNC;
environment.YALAHOO_VERIFY_VISIBLE = visible ? '1' : '0';
environment.YALAHOO_VERIFY_FOREGROUND =
    visible ? (process.env.YALAHOO_VERIFY_FOREGROUND || '0') : '0';
const executable = path.join(root, 'build/release/tools/source-ui-probe'
                                       + (process.platform === 'win32' ? '.exe' : ''));
fs.mkdirSync(output, {recursive: true});
function run(arguments_, log) {
    const result = spawnSync(executable, arguments_, {
        cwd: root,
        env: environment,
        windowsHide: true,
        encoding: 'utf8',
        maxBuffer: 64 * 1024 * 1024,
        timeout: 25 * 60 * 1000
    });
    fs.writeFileSync(path.join(output, log), (result.stdout || '') + (result.stderr || ''));
    if (result.error || result.signal || result.status === null)
        throw Error('Native audit failed: ' + (result.error || result.signal));
    return result;
}
const native = run(['--native-dpr'], 'native-dpr.txt');
const nativeDpr = Number(native.stdout.trim());
if (native.status !== 0 || !Number.isFinite(nativeDpr) || nativeDpr <= 0)
    throw Error('Native screen DPR query failed');
environment.QT_SCALE_FACTOR = String(dpr / nativeDpr);
const result = run(['--titles', output, source], 'title-audit.txt');
if (/Binding loop detected|Failed to build graphics pipeline|No GLSL shader code found|Render-thread font identity differs|ReferenceError|TypeError|Unable to assign/
        .test((result.stdout || '') + (result.stderr || '')))
    throw Error('QML binding or shader pipeline failure; output retained in ' + output);
if (result.status !== 0)
    throw Error('Title audit exited ' + result.status + '; output retained in ' + output);
const report = JSON.parse(fs.readFileSync(path.join(output, 'report.json'), 'utf8'));
const production = validateTitleState(
    report, {backend: options.backend, dpr, visible, platform: process.platform});
// Source DOM font boxes are audited independently of the native geometry assertions.
let baselines = 0, colorFaces = 0;
const references = reference.captures.filter((capture) => Math.abs(capture.dpr - dpr) < .001);
for (const capture of production) {
    const faces = references[capture.case].platformFonts;
    for (const glyph of capture.geometry.actualGlyphs) {
        const face = faces.find((entry) => entry.text === glyph.text);
        if (!glyph.fonts.length
            || glyph.fonts.some((font) =>
                                    !font.headSha256 || !font.glyphIndexes.length
                                    || font.glyphIndexes.length !== font.glyphPositions.length))
            throw Error('Whole-line physical font/glyph identity missing');
        if (process.platform === 'darwin' && !glyph.colorGlyph) {
            if (!face || !Number.isFinite(face.fontAscent) || !Number.isFinite(face.fontDescent)
                || !Number.isFinite(face.rangeTop) || !Number.isFinite(parseFloat(face.lineHeight)))
                throw Error('Same-host CSS primary font-box metrics required');
            const leading = (parseFloat(face.lineHeight) - face.fontAscent - face.fontDescent) / 2;
            const baseline = face.fontAscent + Math.floor(leading);
            if (Math.abs(face.rangeTop - Math.floor(leading)) > .001
                || Math.abs(glyph.fonts[0].firstY * glyph.scale - baseline) > 1 / 64)
                throw Error('Source CSS font box and shaped primary baseline disagree');
            ++baselines;
        }
        if (process.platform === 'darwin' && glyph.colorGlyph) {
            if (!face?.fonts.some((font) => font.postScriptName === 'AppleColorEmoji')
                || glyph.fonts.length !== 1 || glyph.fonts[0].family !== 'Apple Color Emoji'
                || glyph.fonts[0].glyphIndexes.length !== 1)
                throw Error('Source/native color grapheme font identity differs');
            ++colorFaces;
        }
    }
}
if (process.platform === 'darwin' && (!baselines || colorFaces !== 8))
    throw Error('Primary and color-font coverage missing');
const summary = {
    passed: true,
    primaryFontBoxesChecked: baselines,
    colorFacesChecked: colorFaces,
    protocol: 'source-structure-font-metrics-motion-v1',
    visualQuality: 'requires-own-output-inspection',
    platform: process.platform,
    architecture: process.arch,
    configuration: 'Release',
    offscreen: report.offscreen === true,
    renderProtocol: report.renderProtocol || 'native-window',
    qtVersion: report.qtVersion,
    backend: report.backend,
    dpr: report.dpr,
    nativeScreenDpr: nativeDpr,
    scaleFactor: dpr / nativeDpr,
    sourceDirectory: source
};
fs.writeFileSync(path.join(output, 'summary.json'), JSON.stringify(summary, null, 2) + '\n');
console.log(JSON.stringify(summary, null, 2));
console.log('Evidence: ' + output);
