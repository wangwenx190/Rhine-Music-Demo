import {FrozenSource, repositoryRoot as root} from './frozen-source.mjs';
import {baselineRevision} from './frozen-git.mjs';
// Frozen source oracle; development only, no product Node dependency.
import fs from 'node:fs/promises';
import path from 'node:path';
import * as THREE from '../../legacy-web/node_modules/three/build/three.module.js';
import {RectAreaLightUniformsLib} from '../../legacy-web/node_modules/three/examples/jsm/lights/RectAreaLightUniformsLib.js';
const baseline = baselineRevision;
const sources = {}, modules = {
    three: THREE,
    'three/addons/lights/RectAreaLightUniformsLib.js': {RectAreaLightUniformsLib},
};
const frozenSource = new FrozenSource(sources);
for (const name of ['music-model', 'theme-transition', 'music-row-lighting', 'music-lighting'])
    modules['./' + name + '.ts'] = frozenSource.load(name, modules);
const {MusicSelectionLighting} = modules['./music-lighting.ts'],
      {ThemeTransition} = modules['./theme-transition.ts'];
const row = modules['./music-row-lighting.ts'];
const cases = [];
for (const fps of [20, 30, 60, 120]) {
    const scene = new THREE.Scene(), key = new THREE.DirectionalLight();
    scene.add(key, new THREE.HemisphereLight(), new THREE.DirectionalLight());
    const lighting = new MusicSelectionLighting(scene);
    lighting.setExperiment(row.DEFAULT_LIGHTING_PRESET);
    lighting.setTheme('day', key);
    const model = new THREE.Object3D(), camera = new THREE.Camera();
    scene.add(model);
    const samples = [];
    let transition, theme = 'day', last = 0;
    for (let n = 0; n <= fps * 4; n++) {
        const time = n / fps, speed = time < 0.75 ? 1 : time < 1.25 ? 0.25 : time < 2 ? 3 : 1;
        const rawDt = n === 0                        ? 0
                      : n === Math.round(fps * 0.65) ? 0.2
                                                     : 1 / fps,
              dt = Math.min(0.05, rawDt) * speed;
        const reduced = time >= 3.5, visible = !(time >= 2.7 && time < 2.9),
              cinematic = time >= 3 && time < 3.3;
        const lane = time < 0.5 ? 0 : time < 1 ? 1 : time < 1.5 ? -1 : time < 2 ? 2 : 0;
        const targetRow = time < 0.2    ? 0
                          : time < 0.35 ? 12
                          : time < 0.5  ? 4
                          : time < 1.5  ? -3
                          : time < 2.5  ? 1048
                                        : 2;
        const independent = time >= 0.5 && time < 2.5;
        const origin = [
            lane * 5.2,
            time >= 2.2 && time < 2.7 ? 2.75 : 0,
            independent ? -0.31 * lane : -targetRow * 0.62,
        ];
        if (time >= 2 && time < 2.5)
            origin[2] -= 1000 * 0.62; // Rebase changes rendered basis, absolute row survives.
        model.position.fromArray(origin);
        model.updateMatrixWorld();
        const desired = (time >= 0.6 && time < 1.1) || time >= 1.6 ? 'night' : 'day';
        if (desired !== theme) {
            theme = desired;
            transition = new ThemeTransition();
            transition.lastTime = 0;
            last = 0;
            lighting.setTheme(theme, key, transition);
        }
        if (transition) {
            last += dt;
            transition.update(last);
            if (reduced)
                transition.finish();
        }
        const settings = {
            ...row.DEFAULT_LIGHTING_PRESET,
            mode: time < 1     ? 'guided'
                  : time < 1.5 ? 'hybrid'
                  : time < 2   ? 'area'
                               : 'baseline',
        };
        lighting.setExperiment(settings);
        const frame = {
            row: targetRow,
            lane,
            independentColumns: independent,
            ...(cinematic ? {introTime: 22.2 + (time - 3) * 10} : {}),
        };
        const detail = time < 2.2 ? 0 : time < 2.7 ? (time - 2.2) / 0.5 : 0;
        lighting.update(model, camera, dt, visible, reduced, cinematic, detail, frame);
        samples.push({
            input: {
                dt,
                origin,
                row: targetRow,
                lane,
                independent,
                visible,
                reduced,
                cinematic,
                detail,
                introTime: frame.introTime,
                night: theme === 'night',
                mode: row.ROW_LIGHTING.modes[settings.mode],
            },
            row: [lighting.rowMotion.value, lighting.rowMotion.velocity],
            lane: [lighting.laneMotion.value, lighting.laneMotion.velocity],
            column: lighting.rowColumn.value.toArray(),
            lightColumn: lighting.column.value.toArray(),
            anchor: lighting.anchor.toArray(),
            spotPosition: lighting.spot.position.toArray(),
            areaPosition: lighting.area.position.toArray(),
            detail: lighting.rowDetail.value,
            palette: {
                environment: scene.environmentIntensity,
                key: key.intensity,
                hemisphere: scene.children.find((x) => x.isHemisphereLight).intensity,
                spot: lighting.spot.intensity,
                area: lighting.areaThemeIntensity.value,
                scatter: lighting.scatterStrength.value,
                printAmbient: lighting.printAmbient.value,
                scatterColor: lighting.scatterColor.value.toArray(),
                spotColor: lighting.spot.color.toArray(),
            },
        });
    }
    cases.push({fps, samples});
}
const fields = [];
for (const mode of ['baseline', 'area', 'guided', 'hybrid'])
    for (const fill of [0, 1])
        for (const dz of [-3, -0.62, 0, 0.186, 0.6045, 1, 5])
            for (const dx of [-54, -27, -5.2, 0, 5.2, 27, 54]) {
                const settings = {...row.DEFAULT_LIGHTING_PRESET, mode, fill};
                fields.push({
                    dx,
                    dz,
                    mode: row.ROW_LIGHTING.modes[mode],
                    fill,
                    value: row.sampleRowLighting(dx, dz, settings),
                });
            }
const output = path.join(root, 'tests/fixtures/lighting');
await fs.mkdir(output, {recursive: true});
await fs.writeFile(
    path.join(output, 'golden.json'),
    JSON.stringify({baseline, sources, cases, fields}, null, 2) + '\n',
);
console.log(
    'Frozen lighting oracle: ' + cases.reduce((n, c) => n + c.samples.length, 0)
        + ' motion/theme samples, ' + fields.length + ' field samples',
);
