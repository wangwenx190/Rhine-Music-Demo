import {FrozenSource} from './frozen-source.mjs';
// Execute frozen material/theme functions without a browser or GPU image oracle.
import {baselineRevision} from './frozen-git.mjs';
import {createRequire} from 'node:module';
import fs from 'node:fs/promises';
import path from 'node:path';
const root = path.resolve(import.meta.dirname, '../..');
const require = createRequire(path.join(root, 'legacy-web/package.json'));
const THREE = require('three');
const baseline = baselineRevision;
const sources = [], modules = {
    three: THREE
};
const frozenSource = new FrozenSource(sources);
for (const name
         of ['glass-reveal', 'internal-optics', 'music-model', 'theme-transition', 'appearance'])
    modules[`./${name}.ts`] = frozenSource.load(name, modules);
const {configureMusicGlass, setMusicGlassClarity} = modules['./music-model.ts'];
const {CardAppearance} = modules['./appearance.ts'];
const {ThemeTransition} = modules['./theme-transition.ts'];
const cases = [];
for (const [surface, name] of ['Frosted_Polymer', 'Ivory_Edges', 'Optical_Diffuser'].entries()) {
    for (const time of [0, .065, .25, .475, .65]) {
        for (const clarity of [0, .5, 1]) {
            const material = new THREE.MeshPhysicalMaterial();
            configureMusicGlass(name, material);
            const appearance = new CardAppearance();
            appearance.register(name, material);
            appearance.setTheme('night');
            const transition = new ThemeTransition();
            // Match the existing deterministic clock in export-lighting-golden.
            transition.lastTime = 0;
            appearance.setTheme('day', transition);
            transition.update(time);
            const warmth = appearance.warmth.value;
            if (surface === 0)
                setMusicGlassClarity(material, clarity, warmth);
            cases.push({
                surface,
                time,
                warmth,
                clarity,
                transport: [
                    material.transmission, material.roughness, material.thickness,
                    material.attenuationDistance
                ],
                tint: material.color.toArray(),
                attenuation: material.attenuationColor.toArray(),
                coat: [material.clearcoat, material.clearcoatRoughness],
            });
            material.dispose();
        }
    }
}
await fs.writeFile(path.join(root, 'tests/fixtures/materials/parameters.json'),
                   JSON.stringify({baseline, sources, cases}, null, 2) + '\n');
console.log(`Frozen material parameters: ${cases.length} continuous theme/clarity samples`);
