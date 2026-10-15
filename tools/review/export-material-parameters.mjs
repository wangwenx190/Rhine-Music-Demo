// Execute frozen material/theme functions without a browser or GPU image oracle.
import {execFrozenGit} from './frozen-git.mjs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import fs from 'node:fs/promises';
import path from 'node:path';
const root = path.resolve(import.meta.dirname, '../..');
const require = createRequire(path.join(root, 'legacy-web/package.json'));
const ts = require('typescript'), THREE = require('three');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const sources = [], modules = {
    three: THREE
};
for (const name
         of ['glass-reveal', 'internal-optics', 'music-model', 'theme-transition', 'appearance']) {
    const source = execFrozenGit(['show', `${baseline}:src/${name}.ts`], {
        cwd: root,
        encoding: 'utf8',
    });
    sources.push(
        {path: `src/${name}.ts`, sha256: createHash('sha256').update(source).digest('hex')});
    const js =
        ts.transpileModule(source, {
              compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS},
          }).outputText;
    const exports = {};
    new Function('exports', 'require', js)(exports, (id) => {
        if (!(id in modules))
            throw Error(`Unknown frozen dependency: ${id}`);
        return modules[id];
    });
    modules[`./${name}.ts`] = exports;
}
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
