import {FrozenSource, repositoryRoot as root, typescript as ts, transpile} from './frozen-source.mjs';
// Mathematical test input: execute frozen overview classes and actual production scene methods.
import {baselineRevision} from './frozen-git.mjs';
import {createRequire} from 'node:module';
import {mkdirSync, writeFileSync} from 'node:fs';
import path from 'node:path';
const baseline = baselineRevision;
const sources = [];
const frozenSource = new FrozenSource(sources);
const motion = frozenSource.load('motion', {});
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
const THREE = createRequire(import.meta.url)(path.join(root, 'legacy-web/node_modules/three'));
const overview = frozenSource.load('music-overview', {three: THREE, './motion.ts': motion});
const edge = frozenSource.load('music-overview-edge', {});
const archive = frozenSource.load(
    'music-archive-motion',
    {'./motion.ts': motion, './archive-loop.ts': {COLUMN_SPACING: 5.2, ROW_SPACING: .62}});
const real = frozenSource.load('music-real-columns',
                               {'./motion.ts': motion, './music-archive-motion.ts': archive});
const sceneText = frozenSource.read('scene');
const ast =
    ts.createSourceFile('scene.ts', sceneText, ts.ScriptTarget.Latest, true, ts.ScriptKind.TS);
const names = ['getOverviewColumns', 'musicColumnRowOffset', 'musicColumnExtent'];
const methods = [];
function visit(node) {
    if (ts.isMethodDeclaration(node) && names.includes(node.name.getText(ast)))
        methods.push(`${node.name.getText(ast)}(${
            node.parameters.map(p => p.getText(ast)).join(',')}) ${node.body.getText(ast)}`);
    ts.forEachChild(node, visit);
}
visit(ast);
if (methods.length !== names.length)
    throw new Error('Frozen overview projection methods missing');
const compiledMethods = transpile(`return {${methods.join(',')}}`);
const cases = [];
for (const fps of [20, 30, 60, 120]) {
    const transition = new overview.MusicOverviewMotion();
    const samples = [];
    for (let i = 0; i < fps * 3; ++i) {
        const time = i / fps;
        transition.active = time < .4 || (time >= .65 && time < 1.8) || time >= 2.5;
        const reduced = time >= 2.6;
        const speed = time >= 1.3 ? 3 : time >= 1 ? .25 : 1;
        const dt = (i === Math.floor(fps * 1.6) ? .05 : 1 / fps) * speed;
        const value = transition.update(dt, reduced);
        samples.push({
            active: transition.active,
            reduced,
            dt,
            value,
            velocity: transition.progress.velocity,
            acceleration: reduced
                              ? 0
                              : 6.5 * 6.5 * (Number(transition.active) - transition.progress.value)
                                    - 13 * transition.progress.velocity
        });
    }
    cases.push({fps, samples});
}
const extents = [];
for (const center of [-100.5, -.5, 0, .37, 106])
    for (const progress of [0, .12, .5, .65, .75, 1])
        for (const offset of [-5, -4.5, -4.49, -4.25, -4, -3.5, 0, 3.5, 4, 4.25, 4.49, 4.5, 5]) {
            const lane = center + offset;
            extents.push({
                lane,
                center,
                progress,
                value: edge.musicOverviewColumnExtent(lane, center, progress)
            });
        }
const sizes = [
    [1920, 1080], [1280, 720], [995, 858], [960, 900], [930, 900], [390, 844], [320, 720],
    [2560, 1080], [3440, 1440]
];
const labels = [];
for (const [width, height] of sizes)
    for (const expanded of [null, 0, 1])
        for (const retained of [[], [0, 1, 2, 3, 4, 5, 6, 7, 8]]) {
            const columns =
                Array.from({length: 9}, (_, lane) => ({
                                            lane,
                                            name: `列 ${lane}`,
                                            count: lane + 1,
                                            x: width / 2 + (lane - 4) * 95,
                                            y: height / 2 + (lane % 2) * 35,
                                            selected: lane === 4,
                                            extent: [.04, .06, .08, .12, .13, .2, .5, .9, 1][lane]
                                        }));
            labels.push({
                width,
                height,
                expanded,
                retained,
                columns,
                visible:
                    overview
                        .visibleOverviewLabels(columns, width, height, new Set(retained), expanded)
                        .map(c => c.lane)
            });
        }
const projections = [];
const counts = [1, 2, 49, 100, 3, 7, 12, 6, 4, 20000];
for (const [width, height] of sizes)
    for (const mode of ['realistic', 'filled'])
        for (const progress of [0, .5, 1]) {
            const framing = overview.overviewFraming(width, height);
            const yaw = framing.yaw * Math.PI / 180, elevation = framing.elevation * Math.PI / 180;
            const aim = new THREE.Vector3(0, -1.8, -1.5);
            const direction =
                new THREE.Vector3(-Math.sin(yaw) * Math.cos(elevation), Math.sin(elevation),
                                  Math.cos(yaw) * Math.cos(elevation));
            const camera = new THREE.PerspectiveCamera(
                2 * Math.atan(framing.span / (2 * framing.distance)) * 180 / Math.PI,
                width / height, 5, 300);
            camera.position.copy(aim).addScaledVector(direction, framing.distance);
            camera.lookAt(aim);
            camera.updateMatrixWorld(true);
            const columns = counts.map((_, lane) => `列 ${lane}`);
            const methods = new Function('THREE', 'archiveColumns', 'columnFiles', 'COLUMN_SPACING',
                                         'ROW_SPACING', 'MUSIC_MODEL', 'musicLibrary',
                                         'musicOverviewColumnExtent', compiledMethods)(
                THREE, columns, lane => ({length: counts[lane]}), 5.2, .62,
                {center: {y: 1.85}, height: 3.35}, true, edge.musicOverviewColumnExtent);
            const realColumns = new real.MusicRealColumns();
            realColumns.reset(counts, {lane: 2, row: 48});
            const host = {
                ...methods,
                loaded: true,
                looping: true,
                container: {clientWidth: width, clientHeight: height},
                camera,
                columnCamera: {value: 0},
                rail: {value: -2.17 - (48 - 15.5) * .62},
                selectedCell: {lane: 2, row: 48},
                musicArrayMode: mode,
                musicOverview: {value: progress},
                musicRealColumns: realColumns
            };
            Object.defineProperty(host, 'musicBrowseRow', {get: () => 48});
            projections.push(
                {width, height, mode, progress, counts, columns: host.getOverviewColumns()});
        }
const cameraStart = sceneText.indexOf('    if (overview > 0) {');
const cameraBlock = sceneText.slice(
    cameraStart, sceneText.indexOf('    if (!cinematic && !this.reduced', cameraStart));
if (!cameraBlock.includes('overviewFraming') || !cameraBlock.includes('const cameraPosition'))
    throw new Error('Frozen camera blend block missing');
const sourceCamera =
    new Function('THREE', 'overviewFraming', 'width', 'height', 'overview', 'base', `
    const cameraAim = new THREE.Vector3(...base.aim);
    const viewDirection = new THREE.Vector3(...base.position).sub(cameraAim).normalize();
    const distance = new THREE.Vector3(...base.position).distanceTo(cameraAim);
    const framing = {span:base.span};
    ${cameraBlock}
    return {position:cameraPosition.toArray(), aim:cameraAim.toArray(), span:framing.span};
`);
const cameraCases = [];
for (const [width, height] of sizes)
    for (const progress of [0, .01, .12, .35, .65, .9, 1]) {
        const base = {position: [-86, 61, 68], aim: [-1.2, .8, .5], span: 7.33};
        cameraCases.push({
            width,
            height,
            progress,
            base,
            expected: sourceCamera(THREE, overview.overviewFraming, width, height, progress, base)
        });
    }
const output = path.join(root, 'tests/fixtures/overview/source-golden.json');
mkdirSync(path.dirname(output), {recursive: true});
writeFileSync(
    output,
    JSON.stringify({baseline, sources, cases, extents, labels, projections, cameraCases}, null, 2)
        + '\n');
console.log(`Overview frozen oracle: ${cases.reduce((n, c) => n + c.samples.length, 0)} motion, ${
    extents.length} edge, ${labels.length} collision and ${
    projections.length} production projection cases`);
