import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
const root = path.resolve(import.meta.dirname, '../..');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const sha = (b) => crypto.createHash('sha256').update(b).digest('hex');
const source = 'legacy-web/node_modules/three/examples/jsm/postprocessing/SMAAPass.js';
const text = await fs.readFile(path.join(root, source), 'utf8');
await fs.mkdir(path.join(root, 'resources/postprocess'), {recursive: true});
const assets = [];
for (const [kind, width, height] of [['Area', 160, 560], ['Search', 66, 33], ]) {
    const match = text.match(
        new RegExp('_get' + kind + 'Texture\\(\\) \\{[\\s\\S]*?base64,([^\']+)'),
    );
    if (!match)
        throw Error('Missing source lookup');
    const bytes = Buffer.from(match[1], 'base64'),
          name = 'postprocess/smaa-' + kind.toLowerCase() + '.png';
    await fs.writeFile(path.join(root, 'resources', name), bytes);
    assets.push({
        path: name,
        width,
        height,
        sha256: sha(bytes),
        bytes: bytes.length,
        format:
            'PNG; numeric lookup, not sRGB; source flipY=false, decoded row zero stays at lookup UV.y=0',
    });
}
const algorithms = {};
for (const name of ['postprocessing/BokehPass.js',
                    'postprocessing/SMAAPass.js',
                    'postprocessing/OutputPass.js',
                    'shaders/BokehShader.js',
                    'shaders/SMAAShader.js',
                    'shaders/OutputShader.js',
]) {
    const p = 'legacy-web/node_modules/three/examples/jsm/' + name;
    algorithms[p] = sha(await fs.readFile(path.join(root, p)));
}
const sources = {};
for (const p of ['src/scene.ts', 'src/render-quality.ts', 'src/music-model.ts'])
    sources[p] = sha(execFileSync('git', ['show', baseline + ':' + p], {cwd: root}));
await fs.writeFile(
    path.join(root, 'resources/manifests/postprocess.json'),
    JSON.stringify(
        {
            baseline,
            sources,
            algorithms,
            assets,
            license: 'Three MIT; resources/licenses/three-LICENSE',
            order: [
                'opaque linear capture',
                'glass linear composition',
                'SSAO',
                'Bokeh 41 taps',
                'SMAA 1x Medium when requested',
                'ACES exposure',
                'sRGB',
            ],
            focusPoint: [0, 2, 0],
            aperture: [0.0003, 0.0008],
            maxblur: 0.011,
            initial: {focus: 25, aperture: 0.0018},
        },
        null,
        2,
        )
        + '\n',
);
console.log('SMAA source lookups exported with provenance');
