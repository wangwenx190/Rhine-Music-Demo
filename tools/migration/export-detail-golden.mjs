import {FrozenSource, repositoryRoot as root} from './frozen-source.mjs';
// Verification input: execute the frozen placement/gate and production drag expressions.
import {baselineRevision} from './frozen-git.mjs';
import {mkdirSync, writeFileSync} from 'node:fs';
import path from 'node:path';
const baseline = baselineRevision;
const sources = [];
const frozenSource = new FrozenSource(sources);
const motion = frozenSource.load('motion', {});
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
const camera = frozenSource.load('music-camera', {
    three: {MathUtils: {clamp}},
    './motion.ts': motion,
    './viewport-layout.ts': {},
});
const scene = frozenSource.read('scene');
const rotationExpression = scene.match(
    /this\.rotation = this\.reduced\s*\? inspecting[\s\S]*?returnStep\(this\.rotation, dt, this\.reduced\);/)[0];
const advanceRotation = new Function('rotation', 'targetRotation', 'dt', 'inspecting',
                                     'aligningSelection', 'reduced', 'THREE', 'returnStep', `
    const holder = {rotation, targetRotation, reduced};
    const blend = 1 - Math.exp(-dt * (reduced ? 35 : 2.8));
    (function() { ${rotationExpression} }).call(holder);
    return holder.rotation;
`);
const cases = [];
for (const fps of [20, 30, 60, 120]) {
    const placement = new camera.MusicPlacementMotion();
    const gate = new camera.MusicPresentation();
    gate.request('archive');
    gate.update(0, true, true, true);
    const samples = [];
    let speed = 1, reduced = false;
    const events = [
        {at: 0, type: 'open'},
        {at: .4, type: 'return'},
        {at: .65, type: 'open'},
        {at: 1, type: 'slow'},
        {at: 1.4, type: 'fast'},
        {at: 1.7, type: 'selection'},
        {at: 2, type: 'return'},
        {at: 2.3, type: 'reduced'},
    ];
    let eventIndex = 0;
    for (let frame = 0; frame < fps * 2.6; ++frame) {
        const at = frame / fps, applied = [];
        while (eventIndex < events.length && events[eventIndex].at <= at + 1e-10) {
            const event = events[eventIndex++];
            applied.push(event.type);
            if (event.type === 'open')
                gate.request('detail');
            if (event.type === 'return')
                gate.request('archive');
            if (event.type === 'selection')
                gate.selectionChanged();
            if (event.type === 'slow')
                speed = .25;
            if (event.type === 'fast')
                speed = 3;
            if (event.type === 'reduced')
                reduced = true;
        }
        const dt = Math.min(.05, 1 / fps) * speed;
        gate.returnWhenAligned(true);
        placement.update(Number(gate.holdsDetail), dt, reduced);
        gate.update(dt, placement.settled, placement.settled, reduced);
        samples.push({
            events: applied,
            dt,
            reduced,
            phase: gate.phase,
            state: [placement.value, placement.velocity, placement.acceleration]
        });
    }
    cases.push({fps, samples});
}
const rotations = [];
for (const reduced of [false, true]) {
    let rotation = 0;
    for (let frame = 0; frame < 360; ++frame) {
        const inspecting = frame < 120;
        const target = frame < 60 ? .8 : -.8;
        const dt = 1 / 120;
        rotation =
            advanceRotation(rotation, target, dt, inspecting, false, reduced,
                            {MathUtils: {lerp: (a, b, t) => a + (b - a) * t}}, motion.returnStep);
        if (frame % 15 === 0 || frame === 359)
            rotations.push({frame, reduced, rotation});
    }
}
const decryption = frozenSource.load('decryption', {});
const clarity = new decryption.DecryptionController();
const claritySamples = [];
for (let frame = 0; frame < 1200; ++frame) {
    let event = '';
    if (frame === 0) {
        clarity.enter();
        event = 'enter';
    }
    if (frame === 540) {
        clarity.leave();
        event = 'leave';
    }
    if (frame === 570) {
        clarity.enter();
        event = 'enter';
    }
    if (frame === 630) {
        clarity.select(.65);
        clarity.enter();
        event = 'reclaim';
    }
    if (frame === 900) {
        clarity.select(1);
        clarity.enter(true);
        event = 'already-clear';
    }
    if (frame === 1000) {
        clarity.leave();
        event = 'leave';
    }
    if (frame === 1080) {
        clarity.select();
        clarity.enter();
        event = 'new';
    }
    const ready = frame >= 60;
    const reduced = frame >= 1100;
    const dt = (frame >= 600 && frame < 720 ? .25 : frame >= 720 && frame < 840 ? 3 : 1) / 120;
    clarity.update(dt, ready, reduced);
    claritySamples.push([event, dt, ready, reduced, clarity.clarity]);
}
const output = path.join(root, 'tests/fixtures/motion/detail-golden.json');
mkdirSync(path.dirname(output), {recursive: true});
writeFileSync(output, JSON.stringify({baseline, sources, cases, rotations, claritySamples}, null, 2)
                          + '\n');
console.log(`Frozen detail oracle: ${
    cases.reduce((sum, c) => sum + c.samples.length,
                 0)} placement frames, ${rotations.length} rotation checkpoints`);
