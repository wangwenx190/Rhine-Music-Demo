import {typescript as ts} from './frozen-source.mjs';
import {FrozenSource} from './frozen-source.mjs';
import {baselineRevision} from './frozen-git.mjs';
import {createRequire} from 'node:module';
import {mkdirSync, writeFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const baseline = baselineRevision;
const sources = [];
const counts = [1, 2, 3, 7, 64, 97, 101, 113, 127];
const data = {
    archiveColumns: counts.map((_, i) => String(i)),
    columnFiles: (lane) => Array.from({length: counts[lane]}, (_, i) => `${lane}:${i}`),
    fileLocation: (id) => {
        const [lane, row] = id.split(':').map(Number);
        return {lane, row: row + 12};
    },
};
const frozenSource = new FrozenSource(sources);
function load(name) {
    return frozenSource.load(name, {'./data.ts': data});
}

const THREE = createRequire(path.join(root, 'legacy-web/package.json'))('three');
const sceneSource = frozenSource.read('scene');
const loop = load('archive-loop');
const motion = load('motion');
function extract(start, end) {
    const first = sceneSource.indexOf(start), last = sceneSource.indexOf(end, first);
    if (first < 0 || last < first)
        throw new Error('Frozen production pool block changed');
    return ts
        .transpileModule(sceneSource.slice(first, last), {
            compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.None},
        })
        .outputText;
}
const cells =
    Array.from({length: 432}, (_, index) => loop.visibleCell(index, {lane: 0, row: 12}, 48));
const matrices = [];
const state = {
    selectedCell: {lane: 0, row: 12},
    outgoing: [],
    positions: cells.map((c) => new THREE.Vector3((c.lane - 2) * 5.2, -4.6, (c.row - 15.5) * .62)),
    cells,
    coordinateOrigin: {lane: 0, row: 0},
    scanTime: 26.56,
    scanBlend: 0,
    idleGain: 0,
    pulses: [],
    pulseGain: 1,
    shoulder: {value: 12},
    laneFocus: {value: 0},
    columnCamera: {value: -10.4},
    rail: {value: 0},
    lift: {value: .9},
    rotation: 0,
    dummy: new THREE.Object3D(),
    model: new THREE.Object3D(),
    reduced: false,
    covers: null,
    instances: [{
        setMatrixAt: (index, matrix) => {
            matrices[index] = matrix.clone();
        }
    }],
    actualCell: () => true,
    musicColumnExtent: () => 1,
    musicColumnRowOffset: () => 0,
    applyMusicColumnExtent: (object, extent) => {
        object.scale.setScalar(extent);
    },
};
const production = new Function(
    'motion', 'loop', 'THREE',
    `
    const {archiveWave,idleWave,musicSelectionWave,settlingWave,columnStrength,rippleEnvelope,smooth: ease}=motion;
    const {cellKey}=loop;
    const musicIntro=false,cinematic=null,musicLibrary=true,time=0,detail=0,trackX=-10.4,entryZ=0,ROW_SPACING=.62;
    const selectedRow=12,selectedLane=0,chosen={x:-10.4,y:-4.6,z:-2.17};
    ${extract('    const field = (row: number, lane: number) => {', '    const selectedBase =')}
    ${
        extract('    const hidden = new Set(this.outgoing.map',
                '    for (const inst of this.instances) inst.instanceMatrix.needsUpdate')}
    ${extract('    this.model.position.set(', '    // Measured from frame 787')}
    this.model.updateMatrix();
`);
production.call(state, motion, loop, THREE);
const poseSamples = cells.map(
    (cell, index) => ({
        index,
        ...cell,
        albumId: loop.fileAtCell(cell),
        matrix: (loop.sameCell(cell, state.selectedCell) ? state.model.matrix : matrices[index])
                    .toArray()
    }));
const ownedPoses = [];
for (const [lift, rotation, shoulder] of [[.2, .7, 11], [.35, -.6, 13]]) {
    state.lift.value = lift;
    state.rotation = rotation;
    state.shoulder.value = shoulder;
    production.call(state, motion, loop, THREE);
    ownedPoses.push({lift, rotation, shoulder, matrix: state.model.matrix.toArray()});
}
const indices = [0, 47, 48, 95, 96, 143, 192, 239, 240, 287, 288, 335, 384, 431];
const centers =
    [{lane: 2, row: 12}, {lane: -9.5, row: -24}, {lane: 14.5, row: 72.25}, {lane: 2049, row: 4099}];
const pool = centers.map((center) => ({
                             center,
                             cells: indices.map((index) => {
                                 const cell = loop.visibleCell(index, center, 48);
                                 return {index, ...cell, albumId: loop.fileAtCell(cell)};
                             })
                         }));
const nearest = [[0, 4.5, 9], [0, -4.5, 9], [12, 13.5, 3], [12, -100, 3], [9, 7, 0]].map(
    (input) => ({input, output: loop.nearestOccurrence(...input)}));
const selection = [];
let current = {lane: 2, row: 14};
for (const [id, navigation] of [
         ['2:0', {axis: 'row', direction: 1}], ['2:2', {axis: 'row', direction: -1}],
         ['3:4', {axis: 'lane', direction: 1, row: 16}], ['1:0', undefined]]) {
    const cell = loop.selectionCell(id, current, navigation);
    selection.push({id, current, navigation, cell});
    current = cell;
}
const fields = [];
for (const time of [0, .2, 1, 3, 26.56]) {
    for (const row of [-8, 12, 23]) {
        const lane = 2;
        fields.push({
            time,
            row,
            lane,
            archive: motion.archiveWave(row, lane, time),
            idle: motion.idleWave(row, lane, time),
            pulse: motion.musicSelectionWave(Math.abs(row - 12), time),
            resting: motion.settlingWave(row - 12, 26.56) * motion.columnStrength(lane, 2),
            extraction: motion.extraction(time),
            returnAngle: motion.returnStep(.8, time, false)
        });
    }
}
mkdirSync(path.join(root, 'tests/fixtures/archive-pool'), {recursive: true});
writeFileSync(path.join(root, 'tests/fixtures/archive-pool/golden.json'), JSON.stringify({
    baseline,
    sources,
    counts,
    pool,
    nearest,
    selection,
    fields,
    ownedPoses,
    poses:
        poseSamples.filter((p) => indices.includes(p.index) || loop.sameCell(p, state.selectedCell))
},
                                                                                         null, 2)
                                                                              + '\n');
