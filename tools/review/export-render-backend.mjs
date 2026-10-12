// Reuse the R09B source harness, without modifying it or its artifacts.
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const root = path.resolve(import.meta.dirname, '../..');
let source = await fs.readFile(
    path.join(root, 'tools/review/export-postprocess-visual.mjs'),
    'utf8',
);
source = source.replace('from \'./cdp.mjs\'', 'from \'../tools/review/cdp.mjs\'')
             .replace(/path\.resolve\(import\.meta\.dirname,\s*'\.\.\/\.\.'\)/,
                      'path.resolve(import.meta.dirname,\'..\')')
             .replaceAll('postprocess-baseline', 'render-backend-baseline')
             .replaceAll('docs/media/postprocess', 'docs/media/render-backend')
             .replaceAll('tests/fixtures/postprocess', 'tests/fixtures/render-backend')
             .replaceAll('verification/windows/postprocess', 'verification/windows/render-backend');
source = source.replace(
    'const q=qualityPresets[c.quality],dims=',
    'const q={...qualityPresets[c.quality],...(c.shadowSize?{shadows:c.shadowSize}:{})},dims=',
);
// Diagnostic floor is persistent across shadow off/on cases. Three's program
// cache requires invalidation when shadowMap.enabled changes without a light
// list change; otherwise the off-state shader is reused and produces no shadow.
source = source.replace(
    'key.shadow.map=null;',
    'key.shadow.map=null;if(c.shadowProbe)shadowFloor.material.needsUpdate=true;',
);
source = source.replace(
    'for(const origin of c.neighbors)',
    'for(const origin of (c.instancePool?c.instances.slice(1):c.neighbors))',
);
source = source.replace(
    ' composer.render();',
    ` if(c.atlas){
 const bytes=new Uint8Array(2048*2048*4);
 for(let y=0;y<2048;y++)for(let x=0;x<2048;x++){const i=(y*2048+x)*4,t=(y>>8)*8+(x>>8);
 bytes[i]=40+t*3;bytes[i+1]=180-t*2;bytes[i+2]=70+t*2;bytes[i+3]=255;
 if(t===0&&c.atlasPhase){bytes[i]=230;bytes[i+1]=30;bytes[i+2]=90;}}
 const papers=[print,...neighbors.filter(o=>o.isMesh)];
 papers.forEach((p,i)=>{const map=new T.DataTexture(bytes,2048,2048);map.needsUpdate=true;map.minFilter=map.magFilter=T.NearestFilter;
 map.repeat.set(1/8,1/8);map.offset.set((i%8)/8,Math.floor(i/8)/8);p.material=new T.MeshBasicMaterial({map});});
 }
 composer.render();`,
);
const start = source.indexOf('    const cases = [];'),
      end = source.indexOf('    if (errors.length)', start);
if (start < 0 || end < 0)
    throw Error('R09B harness markers changed; adapt the R10 wrapper');
const loop = source.slice(start, end);
const capture = loop.slice(
    loop.indexOf('    async function capture(input)'),
    loop.indexOf('    for (const day of [true, false])'),
);
source = source.slice(0, start) + ' const cases=[];\n' + capture + `
 const inputs=JSON.parse(await fs.readFile(path.join(root,'tests/fixtures/render-backend/inputs.json'),'utf8'));
 for(const c of inputs.cases)await capture(c);
` + source.slice(end);
source = source.replace(
    /postprocess:\s*true,\s*baseline/,
    'renderBackend:true,postprocess:true,baseline',
);
// All R10 inputs have DPR1. R09B already covers the nine sizes and real DPR1/1.5/2.
const dprStart = source.indexOf('    for (const dpr of [1, 1.5, 2])');
const dprEnd = source.indexOf(
    '    await fs.mkdir(path.join(root, \'verification/windows/render-backend\')',
    dprStart,
);
source = source.slice(0, dprStart) + source.slice(dprEnd);
await fs.mkdir(path.join(root, 'build'), {recursive: true});
const generated = path.join(root, 'build/render-backend-capture.mjs');
await fs.writeFile(generated, source);
await import(pathToFileURL(generated));
