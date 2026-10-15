import {execFrozenGit} from './frozen-git.mjs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import {mkdirSync, writeFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const ts = createRequire(import.meta.url)(path.join(root, 'legacy-web/node_modules/typescript'));
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const platform = process.platform === 'win32'    ? 'windows'
                 : process.platform === 'darwin' ? 'macos'
                                                 : 'linux';
const sources = [];
const modules = new Map();
function load(name) {
    if (modules.has(name))
        return modules.get(name);
    const source =
        execFrozenGit(['show', `${baseline}:src/${name}.ts`], {cwd: root, encoding: 'utf8'});
    sources.push(
        {path: `src/${name}.ts`, sha256: createHash('sha256').update(source).digest('hex')});
    const js =
        ts.transpileModule(source, {
              compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS},
          }).outputText;
    const exports = {};
    modules.set(name, exports);
    new Function('exports', 'require', js)(exports, (id) => {
        if (id === './data.ts')
            return {};
        return load(id.replace('./', '').replace('.ts', ''));
    });
    return exports;
}
const {MusicRealColumns} = load('music-real-columns');
const motion = load('motion');
const counts = [0, 1, 2, 3, 49, 106];
const cases = [];
const fullCases = [];
for (const fps of [2, 20, 30, 60, 120]) {
    for (const speed of [.25, 1, 3]) {
        const columns = new MusicRealColumns();
        columns.reset(counts, {lane: 1, row: 12});
        const segments = [];
        const fullSegments = [];
        const actions = [
            {lane: 5, row: 117, guided: true},
            {lane: 4, row: 60},
            {lane: 5, row: 38},
            {lane: 5, row: 39},
            {lane: 5, row: 12, detail: true},
            {lane: 3, row: 13},
            {lane: 2, row: 13, reduced: true},
            {lane: 4, row: 36},
        ];
        for (const action of actions) {
            columns.select(action, action);
            const steps = Math.ceil(.4 * fps / speed);
            const samples = [];
            const fullSamples = [];
            for (let step = 0; step <= steps; ++step) {
                if (step)
                    columns.update(speed / fps, action.reduced === true);
                const state = columns.state.map((column) => [column.row.value,
                                                             column.row.velocity,
                                                             column.row.acceleration,
                                                             column.activity.value,
                                                             column.activity.velocity,
                                                             column.activity.acceleration,
                ]);
                const sample = {step, state};
                fullSamples.push(sample);
                if (step === 0 || step === Math.floor(steps / 2) || step === steps)
                    samples.push(sample);
            }
            segments.push({action, steps, samples});
            fullSegments.push({action, steps, samples: fullSamples});
        }
        cases.push({fps, speed, segments});
        fullCases.push({fps, speed, segments: fullSegments});
    }
}
mkdirSync(path.join(root, 'tests/fixtures/real-columns'), {recursive: true});
writeFileSync(path.join(root, 'tests/fixtures/real-columns/trajectories.json'),
              JSON.stringify({baseline, sources, counts, cases}) + '\n');
const output = path.join(root, 'verification', platform, 'real-columns');
mkdirSync(output, {recursive: true});
writeFileSync(path.join(output, 'full-trajectories.json'),
              JSON.stringify({baseline, sources, counts, cases: fullCases}) + '\n');

// Frozen production field, independent from the native transform implementation.
const scene = execFrozenGit(['show', `${baseline}:src/scene.ts`], {cwd: root, encoding: 'utf8'});
const start = scene.indexOf('  private musicRealColumnField(');
const end = scene.indexOf('  private visibleMusicCell(', start);
if (start < 0 || end < 0)
    throw new Error('Missing frozen finite-column production field');
const fieldSource = scene.slice(start, end).replace('  private ', '');
const fieldClass = ts.transpileModule(`class Field { ${fieldSource} }`, {
                         compilerOptions: {target: ts.ScriptTarget.ES2022},
                     }).outputText;
const Field = new Function('motion', 'THREE', `
    const { idleWave, musicSelectionWave, rippleEnvelope, settlingWave } = motion;
    ${fieldClass}
    return Field;
`)(motion, {
    MathUtils: {lerp: (a, b, t) => a + (b - a) * t, clamp: (v, a, b) => Math.max(a, Math.min(b, v))}
});
const finite = new MusicRealColumns();
finite.reset(counts, {lane: 1, row: 12});
const oracle = Object.assign(new Field(), {
    musicRealColumns: finite,
    musicOverview: {value: 0},
    idleGain: 0,
    reduced: false,
    pulses: [],
    pulseGain: 1,
    deferSelectionPulse: false,
});
const fields = [];
for (let lane = 1; lane < counts.length; ++lane) {
    for (let row = 12; row < 12 + counts[lane]; ++row) {
        fields.push({
            lane,
            row,
            depth: finite.row(lane),
            height: oracle.musicRealColumnField(row, lane, 0),
            slope: oracle.musicRealColumnField(row + .5, lane, 0)
                       - oracle.musicRealColumnField(row - .5, lane, 0)
        });
    }
}
writeFileSync(path.join(output, 'frozen-fields.json'), JSON.stringify({baseline, fields}) + '\n');
console.log('Frozen finite-column inputs generated; full frame series remain local.');
