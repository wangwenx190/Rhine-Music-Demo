// Verification-only: execute immutable Git blobs using the already frozen TS dependency.
import {execFileSync} from 'node:child_process';
import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const ts = createRequire(import.meta.url)(path.join(root, 'legacy-web/node_modules/typescript'));
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const sources = [];
const frozen = (name) => {
    const source = execFileSync('git', ['show', `${baseline}:src/${name}.ts`], {
        cwd: root,
        encoding: 'utf8',
    });
    sources.push({
        path: `src/${name}.ts`,
        sha256: createHash('sha256').update(source).digest('hex'),
    });
    return source;
};
const spacing = frozen('archive-loop');
const constants = Object.fromEntries(
    ['COLUMN_SPACING', 'ROW_SPACING'].map(
        (key) => [key,
                  Number(spacing.match(new RegExp(`export const ${key} = ([\\d.]+);`))[1]),
]),
);
const modules = {
    './archive-loop.ts': constants
};
for (const name of ['motion', 'music-archive-motion', 'music-real-columns', 'music-boot-timing']) {
    const js =
        ts.transpileModule(frozen(name), {
              compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS},
          }).outputText;
    const exports = {};
    new Function('exports', 'require', js)(exports, (id) => {
        if (!modules[id])
            throw new Error(`Unexpected frozen dependency ${id}`);
        return modules[id];
    });
    modules[`./${name}.ts`] = exports;
}
const motion = modules['./motion.ts'], archive = modules['./music-archive-motion.ts'];
const real = modules['./music-real-columns.ts'];
const keys = archive.ARCHIVE_TRACK_KEYS;
const cases = [];
for (const fps of [20, 30, 60, 120]) {
    const tracks = Object.fromEntries(
        keys.map((k, i) => [k, {value: [2, -1, 4, 0.5][i], velocity: [0.2, -0.3, 0.4, -0.5][i]}]),
    );
    const pathMotion = new archive.MusicArchiveMotion();
    const observeTargets = {rail: 3, column: 2, shoulder: 6, lane: 1};
    const rates = {rail: 3.7, column: 3.7, shoulder: 9, lane: 6.5};
    pathMotion.observe(tracks, observeTargets, rates);
    const initial = structuredClone(pathMotion.state);
    pathMotion.retarget(tracks, {rail: 7.44, column: 15.6, shoulder: 12, lane: 3});
    const scalar = new real.MusicRealColumnScalar(2);
    scalar.retarget(5);
    const spring = {value: -2, velocity: 0.7};
    let speed = 1, eventIndex = 0;
    const events = [
        {at: 0.25, type: 'guide'},
        {at: 0.5, type: 'reverse'},
        {at: 0.75, type: 'slow'},
        {at: 1, type: 'rebase'},
        {at: 1.5, type: 'fast'},
        {at: 1.75, type: 'guide-again'},
        {at: 2.25, type: 'reduced'},
    ];
    const samples = [];
    for (let frame = 0; frame <= fps * 3; frame++) {
        const time = frame / fps, applied = [];
        while (eventIndex < events.length && events[eventIndex].at <= time + 1e-10) {
            const e = events[eventIndex++];
            applied.push(e.type);
            if (e.type === 'guide')
                scalar.retarget(18, {guided: true, duration: 1.8});
            if (e.type === 'reverse') {
                scalar.retarget(-3, {guided: true, duration: 1.4});
                pathMotion.retarget(tracks, {rail: -0.62, column: -5.2, shoulder: -1, lane: -1});
            }
            if (e.type === 'slow')
                speed = 0.25;
            if (e.type === 'fast') {
                speed = 3;
                scalar.retarget(8, {rate: 9});
            }
            if (e.type === 'guide-again')
                scalar.retarget(-6, {guided: true, duration: 0.9});
            if (e.type === 'rebase') {
                const offset = {rail: -0.62, column: -5.2, shoulder: -1, lane: -1};
                pathMotion.translate(offset);
                for (const k of keys)
                    tracks[k].value += offset[k];
            }
        }
        const rawDt = frame === 0                        ? 0
                      : frame === Math.floor(fps * 0.65) ? 0.2
                      : frame === Math.floor(fps * 1.2)  ? -0.1
                                                         : 1 / fps;
        const dt = Math.min(Math.max(rawDt, 0), 0.05) * speed;
        const reduced = time >= 2.25 - 1e-10;
        motion.damp(spring, 1, 3.7, dt);
        pathMotion.update(tracks, dt, reduced);
        scalar.update(dt, reduced);
        samples.push({
            time,
            rawDt,
            speed,
            dt,
            reduced,
            events: applied,
            spring: {...spring},
            quintic: pathMotion.state,
            scalar: scalar.state,
        });
    }
    cases.push({fps, initial, samples});
}
const functions = [];
for (const time of [-0.1, 0, 0.2, 0.46, 1, 3.2, 3.21, 21.92, 22.1, 24.95, 25.4, 26.4, 28.85]) {
    const row = 14, lane = 3, distance = -2;
    functions.push({
        time,
        smooth: motion.smooth(time),
        archiveWave: motion.archiveWave(row, lane, time),
        extraction: motion.extraction(time),
        settlingWave: motion.settlingWave(distance, time),
        baselineSelectionWave: motion.baselineSelectionWave(distance, time),
        musicSelectionWave: motion.musicSelectionWave(distance, time),
        selectionWave: motion.selectionWave(distance, time),
        rippleEnvelope: motion.rippleEnvelope(distance, time),
        columnStrength: motion.columnStrength(lane, 2, time),
        idleWave: motion.idleWave(row, lane, time),
        cinematicField: motion.cinematicField(row, lane, time),
        returnStep: motion.returnStep(0.3, Math.max(0, time)),
        returnStepReduced: motion.returnStep(0.3, Math.max(0, time), true),
    });
}
const output = path.join(root, 'tests/fixtures/motion');
mkdirSync(output, {recursive: true});
writeFileSync(
    path.join(output, 'kernel-golden.json'),
    JSON.stringify(
        {
            baseline,
            sources,
            authority: 'Frozen source; numerical oracle, no rendering claims',
            cases,
            functions,
            r01GoldenSha256:
                createHash('sha256')
                    .update(readFileSync(path.join(root, 'docs/contracts/motion-golden.json')))
                    .digest('hex'),
        },
        null,
        2,
        )
        + '\n',
);
console.log(
    `Exported ${cases.reduce((n, c) => n + c.samples.length, 0)} frozen kernel samples and ${
        functions.length} function vectors`,
);
