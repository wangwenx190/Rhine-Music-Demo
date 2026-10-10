import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {createRequire} from 'node:module';
const root=path.resolve(import.meta.dirname,'../..'),baseline='57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const require=createRequire(path.join(root,'legacy-web/package.json')),ts=require('typescript');
const source=execFileSync('git',['show',baseline+':src/render-quality.ts'],{cwd:root}).toString();
const code=ts.transpileModule(source,{compilerOptions:{module:ts.ModuleKind.ESNext}}).outputText;
const {normalizeQuality,renderDimensions,qualityPresets,matchingPreset}=await import('data:text/javascript;base64,'+Buffer.from(code).toString('base64'));
const normalized=[];
for(const input of [null,{},...Object.values(qualityPresets),{scale:53,depthOfField:148},{scale:'150',pixelRatio:3,shadows:0,aoSamples:16,aoResolution:.75},{scale:-1,depthOfField:1000,shadows:3000,aoSamples:99},{scale:true,aoResolution:'0.5'}, {scale:NaN,depthOfField:Infinity}])for(const legacyHigh of [true,false]) {
 const actual=normalizeQuality(input,legacyHigh);normalized.push({input,legacyHigh,expected:actual,preset:matchingPreset(actual)});
}
const dimensions=[];
for(const name of Object.keys(qualityPresets))for(const [w,h]of [[1920,1080],[1280,720],[995,858],[960,900],[930,900],[390,844],[320,720],[2560,1080],[3440,1440],[8000,8000]])for(const dpr of [1,1.5,2,3])for(const maximum of [2048,4096,16384]) {
 const stageScale=Math.min(w/1920,h/1080),expected=renderDimensions(qualityPresets[name],w,h,stageScale,dpr,maximum);
 dimensions.push({preset:name,width:w,height:h,stageScale,dpr,maximum,expected});
}
await fs.writeFile(path.join(root,'tests/fixtures/occlusion/quality.json'),JSON.stringify({baseline,normalized,dimensions},null,2)+'\n');
console.log(`Quality source oracle: ${normalized.length} normalization + ${dimensions.length} target cases`);
