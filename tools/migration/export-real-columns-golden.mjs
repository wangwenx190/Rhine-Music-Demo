import {FrozenSource, loadSource, repositoryRoot as root, transpile} from './frozen-source.mjs';
import {baselineRevision} from './frozen-git.mjs';
import {mkdirSync, writeFileSync} from 'node:fs';
import path from 'node:path';

const baseline = baselineRevision;
const sources = [];
const frozenSource = new FrozenSource(sources);
const columnsSource = frozenSource.read('music-real-columns');
const archiveSource = frozenSource.read('music-archive-motion');
const loop = frozenSource.load('archive-loop', {'./data.ts': {}});
const motion = frozenSource.load('motion');
const archive = loadSource(archiveSource, {'./archive-loop.ts': loop});
const {MusicRealColumns} = loadSource(columnsSource, {
    './music-archive-motion.ts': archive,
    './motion.ts': motion,
});
const counts = [0, 1, 2, 3, 49, 106];
const cases = [];
for (const fps of [2, 20, 30, 60, 120]) {
    for (const speed of [.25, 1, 3]) {
        const columns = new MusicRealColumns();
        columns.reset(counts, {lane: 1, row: 12});
        const segments = [];
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
                if (step === 0 || step === Math.floor(steps / 2) || step === steps)
                    samples.push(sample);
            }
            segments.push({action, steps, samples});
        }
        cases.push({fps, speed, segments});
    }
}
mkdirSync(path.join(root, 'tests/fixtures/real-columns'), {recursive: true});
writeFileSync(path.join(root, 'tests/fixtures/real-columns/trajectories.json'),
              JSON.stringify({baseline, sources, counts, cases}) + '\n');
// Frozen production field, independent from the native transform implementation.
const scene = frozenSource.read('scene');
const start = scene.indexOf('  private musicRealColumnField(');
const end = scene.indexOf('  private visibleMusicCell(', start);
if (start < 0 || end < 0)
    throw new Error('Missing frozen finite-column production field');
const fieldSource = scene.slice(start, end).replace('  private ', '');
const fieldClass = transpile(`class Field { ${fieldSource} }`);
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
writeFileSync(path.join(root, 'tests/fixtures/real-columns/fields.json'),
              JSON.stringify({baseline, sources, counts, fields}) + '\n');
console.log('Finite-column numerical fixtures generated.');
