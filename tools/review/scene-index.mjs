import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
export async function loadSceneIndex(root, index) {
  for(const state of index.states) if(!state.parameters) {
    const resolved=path.resolve(root,state.parameterFile);
    assert.ok(resolved.startsWith(path.resolve(root,'docs/contracts/web-scenes')+path.sep),'Scene file must remain in contract directory');
    const bytes=await fs.readFile(resolved);assert.equal(sha(bytes),state.sha256,state.name);
    state.parameters=JSON.parse(bytes);
  }
  return index;
}
export async function writeSceneIndex(root, data) {
  const sceneRoot=path.join(root,'docs/contracts/web-scenes');
  await fs.mkdir(sceneRoot,{recursive:true});
  const states=[];
  for(const state of data.states) {
    assert.match(state.name,/^[a-z0-9.-]+$/);
    const parameterFile=`docs/contracts/web-scenes/${state.name}.json`;
    const bytes=Buffer.from(JSON.stringify(state.parameters,null,2)+'\n');
    await fs.writeFile(path.join(root,parameterFile),bytes);
    states.push({name:state.name,input:state.input,parameterFile,sha256:sha(bytes)});
  }
  const index={...data,states};
  await fs.writeFile(path.join(root,'docs/contracts/scene-parameters.json'),JSON.stringify(index,null,2)+'\n');
  return index;
}
if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
  const data=await loadSceneIndex(root,JSON.parse(await fs.readFile(path.join(root,'docs/contracts/scene-parameters.json'),'utf8')));
  await writeSceneIndex(root,data);console.log(`Indexed ${data.states.length} verified scene parameter files.`);
}
