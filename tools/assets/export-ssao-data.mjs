import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {execFileSync} from 'node:child_process';
const root = path.resolve(import.meta.dirname, '../..'),
      baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const {SSAOPass} = await import(pathToFileURL(
    path.join(root, 'legacy-web/node_modules/three/examples/jsm/postprocessing/SSAOPass.js'),
    ));
const {PerspectiveCamera} = await import(
    pathToFileURL(path.join(root, 'legacy-web/node_modules/three/build/three.module.js')));
const sha = (b) => crypto.createHash('sha256').update(b).digest('hex');
const output = path.join(root, 'resources/environment'), assets = [], kernels = {};
await fs.mkdir(output, {recursive: true});
for (const count of [16, 32, 64]) {
    const camera = new PerspectiveCamera(32, 4 / 3, 5, 300);
    let state = 417;
    const original = Math.random;
    Math.random = () => {
        state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
        return state / 4294967296;
    };
    let pass;
    try {
        pass = new SSAOPass(undefined, camera, 1, 1, count);
    } finally {
        Math.random = original;
    }
    const samples = pass.kernel.map((v) => [v.x, v.y, v.z, 1]),
          noise = Array.from(pass.noiseTexture.image.data, (v) => [v, 0, 0, 1]);
    kernels[count] = {samples, noise: noise.map((v) => v[0])};
    for (const [kind, data, width, height] of [['kernel', samples, count, 1],
                                               ['noise', noise, 4, 4],
    ]) {
        const bytes = Buffer.alloc(data.length * 16);
        data.flat().forEach((v, i) => bytes.writeFloatLE(v, i * 4));
        const name = `ssao-${kind}-${count}.rgba32f`;
        await fs.writeFile(path.join(output, name), bytes);
        assets.push({
            path: 'environment/' + name,
            width,
            height,
            format: 'RGBA32F little-endian',
            bytes: bytes.length,
            sha256: sha(bytes),
        });
    }
    pass.dispose();
    pass.noiseTexture.dispose();
}
const sources = {};
for (const name of ['src/scene.ts',
                    'src/render-quality.ts',
                    'src/music-lighting.ts',
                    'src/cover-atlas.ts',
])
    sources[name] = sha(execFileSync('git', ['show', baseline + ':' + name], {cwd: root}));
const algorithms = {};
for (const name of ['postprocessing/SSAOPass.js',
                    'shaders/SSAOShader.js',
                    'math/SimplexNoise.js',
]) {
    const p = 'legacy-web/node_modules/three/examples/jsm/' + name;
    algorithms[p] = sha(await fs.readFile(path.join(root, p)));
}
await fs.mkdir(path.join(root, 'tests/fixtures/occlusion'), {recursive: true});
await fs.writeFile(
    path.join(root, 'tests/fixtures/occlusion/kernel.json'),
    JSON.stringify({baseline, seed: 417, kernels}, null, 2) + '\n',
);
await fs.writeFile(
    path.join(root, 'resources/manifests/occlusion.json'),
    JSON.stringify(
        {
            baseline,
            seed: 417,
            sources,
            algorithms,
            assets,
            license: 'Three MIT; resources/licenses/three-LICENSE',
            effective: {
                kernelRadius: 0.18,
                minDistance: 0.001,
                maxDistance: 0.035,
                aoStrength: 0.22,
                shadowIntensity: 0.32,
                normalBias: 0.035,
                bias: -0.0003,
                radius: 4,
                projection: {left: -16, right: 16, bottom: -15, top: 15, near: 0.1, far: 45},
            },
        },
        null,
        2,
        )
        + '\n',
);
console.log('Frozen seeded SSAO kernels/noise exported: 16/32/64');
