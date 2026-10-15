import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';

const root = path.resolve(import.meta.dirname, '../..');
if (process.argv.length !== 3)
    throw Error('Usage: node tools/audit/audit-title.mjs complete-directory');
const directory = path.resolve(process.argv[2]);
const read = (file) => JSON.parse(fs.readFileSync(file, 'utf8'));
const summary = read(path.join(directory, 'summary.json'));
const report = read(path.join(directory, 'report.json'));
const source = read(path.join(summary.sourceDirectory, 'report.json'));
if (!summary.passed || !report.passed || !report.released || !report.windowPolicy.passed
    || summary.protocol !== 'source-structure-font-metrics-motion-v1'
    || source.protocol !== summary.protocol || summary.platform !== source.platform
    || report.backend !== summary.backend || report.dpr !== summary.dpr)
    throw Error('Complete current-platform non-image title checks required');
const references = source.captures.filter((capture) => Math.abs(capture.dpr - summary.dpr) < .001);
const production = report.captures.filter((capture) => capture.mode === 0);
if (references.length !== 20 || production.length !== 20)
    throw Error('Twenty source/production states required');
let baselines = 0, colorFaces = 0;
for (const capture of production) {
    if (!capture.valid || !capture.structurePassed || !capture.geometry.passed)
        throw Error('Invalid native structure');
    const faces = references[capture.case].platformFonts;
    for (const glyph of capture.geometry.actualGlyphs) {
        const face = faces.find((entry) => entry.text === glyph.text);
        if (!glyph.fonts.length
            || glyph.fonts.some((font) =>
                                    !font.headSha256 || !font.glyphIndexes.length
                                    || font.glyphIndexes.length !== font.glyphPositions.length))
            throw Error('Whole-line physical font/glyph identity missing');
        // CSS font-box coordinates are independent of raster appearance. macOS uses
        // the source CoreText integral half-leading, not a per-case image adjustment.
        if (summary.platform === 'darwin' && !glyph.colorGlyph) {
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
        if (summary.platform === 'darwin' && glyph.colorGlyph) {
            if (!face?.fonts.some((font) => font.postScriptName === 'AppleColorEmoji')
                || glyph.fonts.length !== 1 || glyph.fonts[0].family !== 'Apple Color Emoji'
                || glyph.fonts[0].glyphIndexes.length !== 1)
                throw Error('Source/native color grapheme font identity differs');
            ++colorFaces;
        }
    }
}
if (summary.platform === 'darwin' && (!baselines || colorFaces !== 8))
    throw Error('Primary and color-font coverage missing');
const chain = read(path.join(root, 'build/release/toolchain.json'));
const suffix = process.platform === 'win32' ? '.exe' : '';
const qsb = ['bin', 'libexec']
                .map((part) => path.join(chain.qtKit, part, 'qsb' + suffix))
                .find((file) => fs.existsSync(file));
const shader = path.join(root, 'build/release/src/app/.qsb/reel-slot.frag.qsb');
const dump = spawnSync(qsb, ['--dump', shader], {encoding: 'utf8', windowsHide: true});
if (summary.qtVersion !== chain.qtVersion || dump.status !== 0
    || !['MSL', 'GLSL', 'HLSL'].every((kind) => dump.stdout.includes(kind)))
    throw Error('Same-Kit shared shader variants missing');
fs.writeFileSync(path.join(directory, 'reel-slot-qsb.txt'), dump.stdout);
const hash = (file) => createHash('sha256').update(fs.readFileSync(file)).digest('hex');
fs.writeFileSync(path.join(directory, 'font-audit.json'), JSON.stringify({
    passed: true,
    primaryFontBoxesChecked: baselines,
    colorFacesChecked: colorFaces,
    binarySha256: hash(path.join(root, 'build/release/tools/ui-audit' + suffix)),
    sourceReportSha256: hash(path.join(summary.sourceDirectory, 'report.json')),
    shaderSha256: hash(shader),
    visualQuality: 'requires-own-output-inspection',
    limitations:
        'Source structure/font metrics and actual native checks only; physical displays, native input, normal VSYNC and target systems remain separate.'
},
                                                                         null, 2)
                                                              + '\n');
console.log('Title font identities, CSS baselines and shader variants passed');
