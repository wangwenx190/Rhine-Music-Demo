import {withSourcePage, rollingNumberSource, wait} from './browser-source.mjs';
import {execFrozenGit, baselineRevision} from './frozen-git.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
const root = path.resolve(import.meta.dirname, '../..');
const baseline = baselineRevision;
const frozen = path.join(root, 'build/ruler-web');
const args = process.argv.slice(2);
if (args.length !== 2 || args[0] !== '--output')
    throw Error('Usage: export-ruler.mjs --output fresh-directory');
const output = path.resolve(args[1]);
await fs.mkdir(output, {recursive: false});
await fs.mkdir(path.join(frozen, 'src'), {recursive: true});
const sources = {};
for (const name of ['music-ruler.ts', 'archive-loop.ts', 'music-motion-settings.ts',
                    'music-navigation-ruler.css']) {
    let text = execFrozenGit(['show', baseline + ':src/' + name], {cwd: root, encoding: 'utf8'});
    sources[name] = createHash('sha256').update(text).digest('hex');
    if (name === 'archive-loop.ts') {
        // Keep the exact two pure helpers used by ruler; other archive functions need the unrelated library.
        text = text.slice(text.indexOf('export type ArchiveCell'),
                          text.indexOf('export function fileAtCell'));
    }
    if (name === 'music-ruler.ts') {
        // Inspection only: expose closure values after the unchanged production update/render path.
        text = text.replace('    destroy() {', `    probe() {
      return { scroll: scroll.value, scrollVelocity: scroll.velocity, width: width.value,
        selected: selectedOrdinal, time: motionTime / 1000,
        slots: slots.map((s) => ({x: s.x, ordinal: s.ordinal, height: s.height.value,
          alpha: Math.max(0, Math.min(1,s.presence.value))*Math.max(0,Math.min(1,s.opacity.value)),
          enabled: !s.button.disabled, active: s.button.classList.contains('active')})) };
    },
    destroy() {`);
    }
    await fs.writeFile(path.join(frozen, 'src', name), text);
}
const harness = await fs.readFile(path.join(root, 'tools/migration/ruler-harness.js'), 'utf8');
const html =
    '<!doctype html><html><head><meta charset="utf-8"><link rel="stylesheet" href="/src/music-navigation-ruler.css">'
    +
    '<style>body{margin:0}#album-ticks{position:absolute!important;left:2px!important;top:2px!important}button{outline:none}</style>'
    +
    '</head><body><div id="album-ticks"></div><script type="module">' + harness
    + '</script></body></html>';
await withSourcePage(
    frozen, html,
    path.join(root, 'build/ruler-profile-' + process.pid), async ({page, version, origin}) => {
        const captures = [];
        for (const dpr of [1, 1.5, 2]) {
            for (const narrow of [false, true]) {
                for (const night of [false, true]) {
                    await page.send('Emulation.setDeviceMetricsOverride', {
                        width: narrow ? 390 : 1280,
                        height: 80,
                        deviceScaleFactor: dpr,
                        mobile: false
                    });
                    const density = '?density=' + dpr + '&narrow=' + narrow + '&night=' + night;
                    await page.send('Page.navigate', {url: origin + '/browse-ui' + density});
                    await wait(() => page.evaluate('window.ready && location.search === '
                                                   + JSON.stringify(density)
                                                   + ' && window.devicePixelRatio === ' + dpr));
                    for (const test of fixture.cases) {
                        for (const fps of [20, 30, 60, 120]) {
                            await page.evaluate('window.resetRuler(' + narrow + ',' + night + ')');
                            const states = [];
                            for (let i = 0; i < test.operations.length; ++i) {
                                const op = test.operations[i];
                                const state = await page.evaluate(
                                    'window.rulerOperation(' + JSON.stringify(op) + ',' + fps + ','
                                    + (dpr === 1 && !narrow && !night && fps === 120) + ')');
                                const capture = fps === 120 && (i === 0 || Boolean(op.frames));
                                states.push({op, state, capture});
                            }
                            captures.push({name: test.name, dpr, narrow, night, fps, states});
                        }
                    }
                }
            }
        }
        const dependency = await rollingNumberSource(sources);
        const trackSource =
            dependency.slice(dependency.indexOf('function L('), dependency.indexOf('function fe('));
        const sampleSource =
            dependency.slice(dependency.indexOf('function J('), dependency.indexOf('function Q('));
        if (!trackSource.startsWith('function L(') || !sampleSource.startsWith('function J('))
            throw Error('Frozen rolling-number pure track symbols were not found');
        const oracle = Function(trackSource + sampleSource + '; return {track:L, sample:J};')();
        const ordinalSamples = [];
        let track = {target: 0, duration: 0, points: [0, 0]}, elapsed = 0,
            state = {position: 0, velocity: 0};
        const operations = [
            {target: 1, instant: true}, {target: 7}, {dt: .123}, {target: -2}, {dt: .1},
            {target: 9}, {dt: .3}, {target: 9}, {dt: .16}, {target: 0}, {dt: .460}
        ];
        for (const op of operations) {
            if (op.dt !== undefined) {
                elapsed += op.dt * 1000;
                state = oracle.sample(track, elapsed);
            } else if (op.instant || op.target !== track.target) {
                track =
                    oracle.track(state.position, op.target, state.velocity, op.instant ? 0 : 460);
                elapsed = 0;
                state = oracle.sample(track, 0);
            }
            ordinalSamples.push({op, value: state.position, velocity: state.velocity});
        }
        await fs.writeFile(
            path.join(output, 'report.json'),
            JSON.stringify({baseline, sources, version, captures, ordinalSamples}, null, 2) + '\n');
        console.log('Frozen ruler scenarios: ' + captures.length);
    }, {diagnostics: true});
