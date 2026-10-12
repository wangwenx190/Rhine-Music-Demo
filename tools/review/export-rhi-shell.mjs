// Reuse the frozen-source production harness; isolate R10B's pre-AO/DOF material boundary.
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const root = path.resolve(import.meta.dirname, '../..');
let wrapper = await fs.readFile(path.join(root, 'tools/review/export-render-backend.mjs'), 'utf8');
wrapper = wrapper.replace('from \'./cdp.mjs\'', 'from \'../tools/review/cdp.mjs\'');
wrapper = wrapper.replace('path.resolve(import.meta.dirname, \'../..\')',
                          'path.resolve(import.meta.dirname, \'..\')');
// The nested generated capture script already resolves its build directory to the repository.
wrapper = wrapper.replaceAll('render-backend-baseline', 'rhi-shell-baseline')
              .replaceAll('docs/media/render-backend', 'docs/media/rhi-shell')
              .replaceAll('tests/fixtures/render-backend', 'tests/fixtures/rhi-shell')
              .replaceAll('verification/windows/render-backend', 'verification/windows/rhi-shell')
              .replaceAll('render-backend-capture.mjs', 'rhi-shell-capture.mjs');
// Direct-material captures retain their exact legacy path. All other inputs expose linear color.
const original = JSON.parse(
    await fs.readFile(path.join(root, 'tests/fixtures/render-backend/visual.json'), 'utf8'));
const selected = original.cases.filter(c => c.index <= 65 && ![0, 1, 3, 58, 59].includes(c.index));
const cases = selected.map((c, index) => ({
                               ...c,
                               index,
                               sourceIndex: c.index,
                               channel: c.materialBaseline ? c.channel : 5,
                               disableBokeh: true,
                               disableSmaa: true,
                           }));
const fixtureRoot = path.join(root, 'tests/fixtures/rhi-shell');
await fs.mkdir(fixtureRoot, {recursive: true});
await fs.writeFile(path.join(fixtureRoot, 'inputs.json'), JSON.stringify({cases}, null, 2) + '\n');
const generated = path.join(root, 'build/rhi-shell-export.mjs');
await fs.writeFile(generated, wrapper);
await import(pathToFileURL(generated));
const visualPath = path.join(fixtureRoot, 'visual.json');
const visual = JSON.parse(await fs.readFile(visualPath, 'utf8'));
visual.round = 'R10B';
visual.referenceMedia = 'docs/media/rhi-shell/web/';
visual.stage =
    'source pre-AO/Bokeh/SMAA linear material composition; direct-material exceptions unchanged';
await fs.writeFile(visualPath, JSON.stringify(visual, null, 2) + '\n');
