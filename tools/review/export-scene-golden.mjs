import {execFrozenGit} from './frozen-git.mjs';
// Verification-only execution of immutable Git source, using existing Web dev dependencies.
import {execFileSync} from 'node:child_process';
import {mkdirSync, writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const req = createRequire(path.join(root, 'legacy-web/package.json'));
const ts = req('typescript'), THREE = req('three');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2', sources = [];
function frozen(name) {
    const source = execFrozenGit(['show', `${baseline}:src/${name}.ts`], {
        cwd: root,
        encoding: 'utf8',
    });
    sources.push({
        path: `src/${name}.ts`,
        sha256: createHash('sha256').update(source).digest('hex'),
    });
    return source;
}
const modules = {
    three: THREE
};
for (const name of ['motion', 'viewport-layout', 'music-camera']) {
    const exports = {};
    const source =
        ts.transpileModule(frozen(name), {
              compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS},
          }).outputText;
    new Function('exports', 'require', source)(exports, (id) => {
        if (!modules[id])
            throw Error(`Unknown frozen dependency: ${id}`);
        return modules[id];
    });
    modules[`./${name}.ts`] = exports;
}
const motion = modules['./motion.ts'], layout = modules['./viewport-layout.ts'],
      music = modules['./music-camera.ts'];
const scene = frozen('scene'), model = frozen('music-model');
const spacing = frozen('archive-loop');
const columnSpacing = Number(spacing.match(/export const COLUMN_SPACING = ([\d.]+)/)[1]);
const rowSpacing = Number(spacing.match(/export const ROW_SPACING = ([\d.]+)/)[1]);
// Execute the actual production target block, with cinematic/overview disabled for R06.
const block = ts.transpileModule(
                    scene.slice(
                        scene.indexOf('    const orbit = ease((shot - 22.6)'),
                        scene.indexOf('    if (!cinematic && !this.reduced && (!musicLibrary'),
                        ),
                    {compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.None}},
                    )
                  .outputText;
if (!block.includes('const cameraPosition = cameraAim'))
    throw Error('Frozen camera target extraction failed');
const target = new Function(
    'THREE',
    'ease',
    'settlingWave',
    'musicArchiveOffset',
    'archiveFraming',
    'musicCinematicPose',
    'musicExtractionAnchor',
    'overviewFraming',
    'MUSIC_MODEL',
    'MUSIC_DETAIL_ELEVATION',
    'MUSIC_INSPECTION_LIFT',
    'dt',
    'detail',
    `const shot=29.1,musicLibrary=true,musicIntro=false,cinematic=null,overview=0,introSettle=1,COLUMN_SPACING=${
        columnSpacing},ROW_SPACING=${rowSpacing};
   ${block}
   return {position:cameraPosition,aim:cameraAim,span:framing.span};`,
);
const dimensions = [
    [1920, 1080],
    [1280, 720],
    [995, 858],
    [960, 900],
    [930, 900],
    [390, 844],
    [320, 720],
    [2560, 1080],
    [3440, 1440],
    [945, 900],
    [944, 900],
    [1100, 700],
    [1099, 700],
    [700, 600],
    [480, 600],
    [1400, 500],
];
const modelInfo = {
    width: Number(model.match(/width: ([\d.]+)/)[1]),
    height: Number(model.match(/height: ([\d.]+)/)[1]),
    center: {y: 1.85},
};
const points = [];
for (const x of [-2.225, 2.225])
    for (const y of [0.175, 3.525])
        for (const z of [-0.07, 0.07])
            points.push([x, y, z]);
points.push([0, 1.85, 0], [0.17, 1.85, 0.027], [-1.875, 1.85, 0.052]);
const cases = [];
for (const [width, height] of dimensions)
    for (const detail of [0, 0.35, 1]) {
        const metrics = layout.viewportLayout(width, height, false);
        const fixture = {
            container: {
                clientWidth: width,
                clientHeight: height,
                closest: () => ({dataset: {layout: metrics.kind}}),
            },
            columnCamera: {velocity: 0},
            rail: {velocity: 0},
            musicArrayMode: 'filled',
            musicCamera: new music.MusicCameraMotion(),
        };
        const pose = target.call(
            fixture,
            THREE,
            motion.smooth,
            motion.settlingWave,
            music.musicArchiveOffset,
            layout.archiveFraming,
            music.musicCinematicPose,
            music.musicExtractionAnchor,
            () => ({distance: 172}),
            modelInfo,
            (20 * Math.PI) / 180,
            3.47,
            0,
            detail,
        );
        const camera = new THREE.PerspectiveCamera(
            (2 * Math.atan(pose.span / (2 * pose.position.distanceTo(pose.aim))) * 180) / Math.PI,
            width / height,
            5,
            300,
        );
        camera.position.copy(pose.position);
        camera.lookAt(pose.aim);
        camera.updateMatrixWorld();
        const casePosition = [
            0,
            -4.6 + motion.settlingWave(0, 26.56) + 0.9 + (3.47 - 0.9) * detail,
            -2.17,
        ];
        const projections = points.map((local) => {
            const world = new THREE.Vector3(...local).add(new THREE.Vector3(...casePosition));
            const ndc = world.clone().project(camera);
            return {
                local,
                world: world.toArray(),
                pixel: [((ndc.x + 1) * width) / 2, ((1 - ndc.y) * height) / 2],
                depth: ndc.z,
            };
        });
        cases.push({
            width,
            height,
            detail,
            metrics,
            pose: {position: pose.position.toArray(), aim: pose.aim.toArray(), span: pose.span},
            fov: camera.fov,
            casePosition,
            projections,
        });
    }
// Independent original MusicCameraMotion oracle with nonzero state and interruptions.
const sequences = [];
for (const fps of [20, 30, 60, 120]) {
    const camera = new THREE.PerspectiveCamera(6.15, 16 / 9, 5, 300);
    camera.position.set(-62.26, 35.98, 43.28);
    const aim = new THREE.Vector3(-0.5, 1.1, 0.4), controller = new music.MusicCameraMotion();
    controller.observe(camera, aim, 0);
    const samples = [];
    for (let frame = 0; frame <= fps * 3; frame++) {
        const time = frame / fps,
              dt = frame === 0 ? 0
                               : Math.min(frame === Math.floor(fps * 0.65) ? 0.2 : 1 / fps, 0.05)
                                     * (time >= 1.5 ? 3
                                        : time >= 1 ? 0.25
                                                    : 1);
        const c = cases[time < 0.5 ? 0 : time < 1.25 ? 2 : time < 2 ? 31 : time < 2.5 ? 28 : 1];
        const reduced = time >= 2.5;
        controller.update(
            camera,
            aim,
            new THREE.Vector3(...c.pose.position),
            new THREE.Vector3(...c.pose.aim),
            c.pose.span,
            dt,
            reduced,
        );
        camera.aspect = c.width / c.height;
        camera.updateProjectionMatrix();
        camera.updateMatrixWorld();
        const projections = points.map((local) => {
            const world = new THREE.Vector3(...local).add(new THREE.Vector3(...c.casePosition));
            const ndc = world.clone().project(camera);
            return {
                world: world.toArray(),
                pixel: [((ndc.x + 1) * c.width) / 2, ((1 - ndc.y) * c.height) / 2],
            };
        });
        samples.push({
            time,
            dt,
            reduced,
            width: c.width,
            height: c.height,
            detail: c.detail,
            position: camera.position.toArray(),
            aim: aim.toArray(),
            fov: camera.fov,
            projections,
        });
    }
    sequences.push({fps, samples});
}
const output = path.join(root, 'tests/fixtures/scene');
mkdirSync(output, {recursive: true});
writeFileSync(
    path.join(output, 'camera-golden.json'),
    JSON.stringify(
        {
            baseline,
            sources,
            authority: 'Frozen source targets and THREE projection; debug geometry only',
            cases,
            sequences,
        },
        null,
        2,
        )
        + '\n',
);
console.log(
    `Exported ${cases.length} target cases and ${
        sequences.reduce((n, s) => n + s.samples.length, 0)} camera frames`,
);
