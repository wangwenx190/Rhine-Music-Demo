import fs from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {RectAreaLightTexturesLib} from '../../legacy-web/node_modules/three/examples/jsm/lights/RectAreaLightTexturesLib.js';
const root=path.resolve(import.meta.dirname,'../..');
const contract=JSON.parse(await fs.readFile(path.join(root,'docs/contracts/lighting.json'),'utf8'));
for(const [source,hash] of Object.entries(contract.upstreamAlgorithms)) {
    if(createHash('sha256').update(await fs.readFile(path.join(root,source))).digest('hex')!==hash)throw Error('Locked lighting algorithm changed: '+source);
}
RectAreaLightTexturesLib.init();
const assets=[];
for(const [name,texture] of [['ltc1',RectAreaLightTexturesLib.LTC_FLOAT_1],['ltc2',RectAreaLightTexturesLib.LTC_FLOAT_2]]) {
    const values=texture.image.data,bytes=Buffer.alloc(values.length*4);
    values.forEach((v,i)=>bytes.writeFloatLE(v,i*4));
    const file='environment/'+name+'.rgba32f';await fs.writeFile(path.join(root,'resources',file),bytes);
    assets.push({path:file,qrc:':/assets/'+file,width:64,height:64,bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex'),source:'legacy-web/node_modules/three/examples/jsm/lights/RectAreaLightTexturesLib.js',license:'resources/licenses/three-LICENSE'});
}
await fs.writeFile(path.join(root,'resources/manifests/lighting.json'),JSON.stringify({format:'RGBA32F little-endian; original Three LTC UV order',assets},null,2)+'\n');
