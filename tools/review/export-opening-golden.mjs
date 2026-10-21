import {transpile} from './frozen-source.mjs';
import {execFrozenGit, baselineRevision} from './frozen-git.mjs';
import {createRequire} from 'node:module';
import {createHash} from 'node:crypto';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const require = createRequire(import.meta.url);
const ts = require(path.join(root, 'legacy-web/node_modules/typescript'));
const THREE = require(path.join(root, 'legacy-web/node_modules/three'));
const revision = baselineRevision;
const sources = [];
const frozen = (name) => {
    const text =
        execFrozenGit(['show', `${revision}:src/${name}.ts`], {cwd: root, encoding: 'utf8'});
    sources.push({path: `src/${name}.ts`, sha256: createHash('sha256').update(text).digest('hex')});
    return text;
};
const modules = {
    'three': THREE,
    './brand': {bootMarkContour: ''},
    './archive-loop.ts': {COLUMN_SPACING: 5.2, ROW_SPACING: 0.62}
};
const load = (name) => {
    const text = frozen(name);
    const js = transpile(text);
    const exports = {};
    new Function('exports', 'require', js)(exports, (id) => {
        const result = modules[id] ?? modules[id.replace(/\.ts$/, '')];
        if (!result)
            throw new Error(`Unexpected source dependency ${id}`);
        return result;
    });
    modules[`./${name}`] = exports;
    return exports;
};
const timing = load('music-boot-timing');
const opening = load('music-opening');
const motion = load('motion');
const viewport = load('viewport-layout');
const camera = load('music-camera');
const scene = frozen('scene');
const cameraStart = scene.indexOf('    const orbit = ease((shot - 22.6) / 1.6);');
const cameraEnd =
    scene.indexOf('    if (!cinematic && !this.reduced && (!musicLibrary', cameraStart);
if (cameraStart < 0 || cameraEnd < 0)
    throw new Error('Frozen production camera boundary changed');
const body = ts.transpileModule(scene.slice(cameraStart, cameraEnd), {
                   compilerOptions: {target: ts.ScriptTarget.ES2022}
               }).outputText;
const cameraFn = new Function(
    'THREE', 'ease', 'settlingWave', 'archiveFraming', 'musicArchiveOffset', 'musicCinematicPose',
    'musicExtractionAnchor', 'overviewFraming', 'MUSIC_MODEL', 'MUSIC_DETAIL_ELEVATION',
    'MUSIC_INSPECTION_LIFT', `return function(shot,width,height,selected) {
    const cinematic = true, musicLibrary = true, musicIntro = true, detail = 0, dt = 0, overview = 0;
    const introSettle = ease((shot - 25.3) / 1.42);
    const model = {position: new THREE.Vector3(...selected)};
    const host = {clientWidth: width, clientHeight: height, closest: () => null};
    const evaluate = function() {
        ${body}
        return {position: cameraPosition.toArray(), aim: cameraAim.toArray(), span: framing.span};
    };
    return evaluate.call({model,container:host});
}`)(THREE, motion.smooth, motion.settlingWave, viewport.archiveFraming, camera.musicArchiveOffset,
    camera.musicCinematicPose, camera.musicExtractionAnchor, () => ({distance: 140}),
    {width: 4.45, height: 3.35, center: {y: 1.85}}, 20 * Math.PI / 180, 3.47);
const times = [...new Set([
                  0,
                  .2,
                  .24,
                  .48,
                  .84,
                  1.5,
                  1.64,
                  1.8,
                  2.05,
                  2.26,
                  2.72,
                  2.78,
                  2.82,
                  2.9,
                  3.18,
                  3.28,
                  3.3,
                  3.58,
                  3.8,
                  5.56,
                  5.7,
                  6.08,
                  6.32,
                  6.4,
                  6.82,
                  10.16,
                  11.2,
                  11.6,
                  ...Array.from({length: 48}, (_, i) => i * .25 + 1 / 120)
              ])].sort((a, b) => a - b);
const frames = times.map(
    time =>
        ({time, ...opening.musicOpeningFrame(time), sceneTime: timing.musicBootSceneTime(time)}));
const poses = [];
for (const [width, height] of [[1920, 1080], [1280, 720], [995, 858], [960, 900], [930, 900],
                               [390, 844], [320, 720], [2560, 1080], [3440, 1440]]) {
    for (const shot of [21.92, 22.03, 22.4, 23.25, 24.5, 25.1, 25.6, 26.2, 26.72, 27.12]) {
        const intro = motion.smooth((shot - 25.3) / 1.42);
        const field = motion.cinematicField(12, 2, shot) * (1 - intro)
                      + motion.settlingWave(0, 26.56) * intro;
        const lift = motion.extraction(shot) * (1 - intro) + .9 * intro;
        const entry = -23 * (1 - Math.max(0, Math.min(1, (shot - 21.92) / .75))) ** 2;
        const selected = [0, -4.6 + field + lift, -2.17 + entry];
        poses.push({shot, width, height, selected, ...cameraFn(shot, width, height, selected)});
    }
}
const output = path.join(root, 'tests/fixtures/opening/source-golden.json');
mkdirSync(path.dirname(output), {recursive: true});
const bytes = JSON.stringify({revision, sources, frames, poses}) + '\n';
if (process.argv.includes('--check')) {
    if (readFileSync(output, 'utf8') !== bytes)
        throw new Error('Opening golden differs from frozen sources');
} else {
    writeFileSync(output, bytes);
}
console.log(`${process.argv.includes('--check') ? 'Verified' : 'Exported'} ${
    frames.length} formula vectors and ${poses.length} production camera poses`);
