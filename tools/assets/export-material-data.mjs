// Deterministic derived material data; uses only existing locked development dependencies.
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import crypto from 'node:crypto';
const root=path.resolve(import.meta.dirname,'../..');
const contractPath=path.join(root,'docs/contracts/materials.json');
const contract=JSON.parse(await fs.readFile(contractPath,'utf8'));
for(const [source,expected] of Object.entries(contract.upstreamAlgorithms)){const bytes=await fs.readFile(path.join(root,source));if(crypto.createHash('sha256').update(bytes).digest('hex')!==expected)throw Error('Locked algorithm changed: '+source);}
const {getDFGLUT}=await import(pathToFileURL(path.join(root,'legacy-web/node_modules/three/src/renderers/shaders/DFGLUTData.js')));
const lut=getDFGLUT().image;
if(lut.width!==16||lut.height!==16||lut.data.length!==512)throw Error('Unexpected source DFG layout');
const half=h=>{const sign=(h&32768)?-1:1,e=(h>>10)&31,m=h&1023;return sign*(e===0?2**-14*m/1024:e===31?NaN:2**(e-15)*(1+m/1024));};
const data=Buffer.alloc(4096);
for(let i=0;i<256;++i)for(let c=0;c<4;++c){const v=c<2?half(lut.data[2*i+c]):c===3?1:0;if(!Number.isFinite(v))throw Error('Nonfinite DFG');data.writeFloatLE(v,(4*i+c)*4);}
await fs.writeFile(path.join(root,'resources/environment/dfg.rgba32f'),data);
const manifestPath=path.join(root,'resources/manifests/materials.json');
const manifest=JSON.parse(await fs.readFile(manifestPath,'utf8'));
for(const asset of manifest.assets){const bytes=await fs.readFile(path.join(root,'resources',asset.path));asset.bytes=bytes.length;asset.sha256=crypto.createHash('sha256').update(bytes).digest('hex');}
await fs.writeFile(manifestPath,JSON.stringify(manifest,null,2)+'\n');
const fixture=JSON.parse(await fs.readFile(path.join(root,'tests/fixtures/materials/single-case.json'),'utf8'));
contract.environment=fixture.environment;
await fs.writeFile(contractPath,JSON.stringify(contract,null,2)+'\n');
console.log('Fixed DFG and material manifest refreshed');
