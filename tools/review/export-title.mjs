import {frozenReader} from './frozen-source.mjs';
import {withSourcePage, writeSourceUi, rollingNumberSource, sourceMarkup, sourceHtml} from './browser-source.mjs';
import {baselineRevision} from './frozen-git.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';

const root = path.resolve(import.meta.dirname, '../..');
const baseline = baselineRevision;
const frozen = path.join(root, 'build/title-web/legacy-web');
const args = process.argv.slice(2);
const platform = process.platform === 'win32'    ? 'windows'
                 : process.platform === 'darwin' ? 'macos'
                                                 : 'linux';
let output = path.join(root, 'verification', platform, 'title-reels/web');
let dprs = [1, 1.5, 2];
const seen = new Set();
for (let i = 0; i < args.length; i += 2) {
    const option = args[i];
    const value = args[i + 1];
    if (!value || seen.has(option) || !['--dpr', '--output'].includes(option))
        throw Error('Usage: export-title.mjs [--dpr 1|1.5|2] [--output directory]');
    seen.add(option);
    if (option === '--dpr') {
        if (![1, 1.5, 2].includes(Number(value)))
            throw Error('DPR must be 1, 1.5 or 2');
        dprs = [Number(value)];
    } else {
        output = path.resolve(value);
    }
}
try {
    await fs.access(path.join(output, 'report.json'));
    throw Error('Use a fresh source directory to retain earlier evidence');
} catch (error) {
    if (error.code !== 'ENOENT')
        throw error;
}
await fs.mkdir(output, {recursive: true});
const sources = {};
const source = frozenReader(sources);
const appSource = source('src/music-app.ts');
const markup = sourceMarkup(appSource);
await writeSourceUi(
    frozen, source, sources,
    (name, text) =>
        name === 'music-title'
            ? text.replace(
                      'candidate.root.style.display = "block";\n    return candidate;',
                      'candidate.root.style.display = "block";\n    const fresh = candidate.root.cloneNode(true); candidate.root.replaceWith(fresh); candidate.root = fresh; candidate.main = fresh.children[0]; candidate.translation = fresh.children[1];\n    return candidate;')
                  .replace(
                      'return { size: candidate.size, height: Math.min(y, budget), glyphs };',
                      'window.lastTitleLayout = { size: candidate.size, height: Math.min(y, budget), glyphs, rootInlineFont: candidate.root.style.fontSize, rootFont: getComputedStyle(candidate.root).fontSize, mainFont: getComputedStyle(candidate.main).fontSize }; return window.lastTitleLayout;')
        : name === 'music-title-reels' ? text + '\nwindow.SourceTitleTrack = TitleTrack;\n'
                                       : text);
const harness = await fs.readFile(path.join(root, 'tools/review/title-harness.js'), 'utf8');
const html = sourceHtml(markup, harness);
await withSourcePage(
    frozen, html,
    path.join(root, 'build/title-profile-' + process.pid), async ({page, version}) => {
        const captures = [];
        const fixture = JSON.parse(
            await fs.readFile(path.join(root, 'tests/fixtures/text/titles.json'), 'utf8'));
        for (const dpr of dprs) {
            await page.send('Emulation.setDeviceMetricsOverride',
                            {width: 1280, height: 720, deviceScaleFactor: dpr, mobile: false});
            for (const input of fixture.cases) {
                for (const night of [false, true]) {
                    const state = await page.evaluate('window.renderCase('
                                                      + JSON.stringify({...input, night}) + ')');
                    const document = await page.send('DOM.getDocument');
                    const nodes = await page.send(
                        'DOM.querySelectorAll',
                        {nodeId: document.root.nodeId, selector: '#stage .music-title-face'});
                    const platformFonts = [];
                    for (const nodeId of nodes.nodeIds) {
                        const {object} = await page.send('DOM.resolveNode', {nodeId});
                        const description = await page.send('Runtime.callFunctionOn', {
                            objectId: object.objectId,
                            functionDeclaration: `function() {
                            const style = getComputedStyle(this);
                            const canvas = document.createElement('canvas').getContext('2d');
                            canvas.font = style.font;
                            const metrics = canvas.measureText(this.textContent);
                            const range = document.createRange();
                            range.selectNodeContents(this);
                            const box = range.getBoundingClientRect();
                            const face = this.getBoundingClientRect();
                            return {
                                text: this.textContent,
                                font: style.font,
                                lineHeight: style.lineHeight,
                                rangeTop: box.top - face.top,
                                rangeHeight: box.height,
                                fontAscent: metrics.fontBoundingBoxAscent,
                                fontDescent: metrics.fontBoundingBoxDescent
                            };
                        }`,
                            returnByValue: true
                        });
                        const fonts = await page.send('CSS.getPlatformFontsForNode', {nodeId});
                        platformFonts.push({...description.result.value, fonts: fonts.fonts});
                        await page.send('Runtime.releaseObject', {objectId: object.objectId});
                    }
                    captures.push({...input, dpr, night, state, platformFonts});
                }
            }
        }
        const tracks = await page.evaluate('window.sampleTracks()');
        const dependency = await rollingNumberSource(sources);
        const part = (from, to) =>
            dependency.slice(dependency.indexOf(from), dependency.indexOf(to));
        const oracle =
            Function(part('function me(', 'function W(') + part('function L(', 'function fe(')
                     + part('function fe(', 'function J(') + part('function J(', 'function Q(')
                     + ';return {entry:me,track:L,blur:fe,sample:J};')();
        const entry = oracle.entry(1, 460);
        const entryBlur = oracle.blur(entry, 0, 6);
        const roll = oracle.track(.12, 2, 12, 460);
        const rolling = [];
        for (let i = 0; i <= 60; ++i) {
            const ms = i * 1000 / 120;
            rolling.push({
                dt: i / 120,
                entry: oracle.sample(entry, ms),
                entryBlur: oracle.sample(entryBlur, ms),
                roll: oracle.sample(roll, ms)
            });
        }
        await fs.writeFile(path.join(output, 'report.json'), JSON.stringify({
            baseline,
            platform: process.platform,
            architecture: process.arch,
            sources,
            version,
            captures,
            tracks,
            rolling,
            probeRefresh:
                'Fresh source probe root with identical inline styles prevents Chromium contain/style returning a prior coarse font after a fine-size write. No fit algorithm, parameter or product style is changed.',
            protocol: 'source-structure-font-metrics-motion-v1'
        },
                                                                            null, 2)
                                                                 + '\n');
        console.log('Frozen title states: ' + captures.length);
    }, {domains: ['DOM', 'CSS']});
