import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import {loadSceneIndex} from './scene-index.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const read=async file=>JSON.parse(await fs.readFile(path.join(root,file),'utf8'));
const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const expected='57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const fixtureBytes=await fs.readFile(path.join(root,'tests/fixtures/web-baseline/anonymous.json'));
const fixture=JSON.parse(fixtureBytes), fixtureHash=hash(fixtureBytes);
assert.equal(fixture.baseline,expected);
assert.deepEqual(Object.keys(fixture.libraries),['empty','single','even','odd','mixed','large']);
assert.ok(fixture.libraries.large.albums.length>432);
for(const library of Object.values(fixture.libraries)) {
  assert.equal(library.roots.length,0);assert.equal(library.onlineEnabled,false);
  for(const album of library.albums) {
    assert.equal(album.folder,'Generated fixture'); assert.match(album.coverUrl,/^\/demo-covers\/(square|portrait|landscape)\.png$/);
    for(const track of album.tracks) {assert.equal(track.audioUrl,'');assert.equal(track.browserPlayable,false);}
  }
}
const source=await read('docs/contracts/source-map.json');
const names=execFileSync('git',['ls-tree','-r','--name-only',expected,'src'],{cwd:root,encoding:'utf8'}).trim().split('\n');
assert.deepEqual(source.modules.map(m=>m.baselinePath),names);
for(const entry of source.modules) {
  const bytes=execFileSync('git',['show',expected+':'+entry.baselinePath],{cwd:root});
  assert.equal(hash(bytes),entry.sha256,entry.baselinePath);
  assert.ok(entry.responsibility);if(entry.role==='migrate')assert.ok(entry.target);
}
const manifest=await read('docs/media/web-baseline/screenshots.json');
const scene=await loadSceneIndex(root,await read('docs/contracts/scene-parameters.json'));
assert.equal(manifest.fixtureHash,fixtureHash);assert.equal(scene.fixtureHash,fixtureHash);
assert.equal(scene.states.length,manifest.captures.length);
const dimensions=[[1920,1080],[1280,720],[995,858],[960,900],[930,900],[390,844],[320,720],[2560,1080],[3440,1440]];
for(const theme of ['day','night'])for(const mode of ['filled','realistic'])for(const [width,height]of dimensions)
  assert.ok(manifest.captures.some(c=>c.name===`browse-${theme}-${mode}-${width}x${height}`),'Missing size/theme/mode');
for(const entry of manifest.captures) {
  assert.equal(entry.baseline,expected);assert.equal(entry.fixtureHash,fixtureHash);
  const bytes=await fs.readFile(path.join(root,entry.file));assert.equal(hash(bytes),entry.sha256,entry.file);
  assert.equal(bytes.readUInt32BE(16),entry.viewport.width*entry.DPR);assert.equal(bytes.readUInt32BE(20),entry.viewport.height*entry.DPR);
  assert.equal(entry.arrayMode,entry.state.arrayMode);assert.equal(entry.theme,entry.state.theme);
  const parameters=scene.states.find(s=>s.name===entry.name).parameters;
  assert.equal(parameters.camera.projectionMatrix.length,16);assert.equal(parameters.camera.matrixWorld.length,16);
  for(const field of ['fov','near','far','aspect'])assert.ok(Number.isFinite(parameters.camera[field]));
  assert.ok(parameters.lights.length>=3);assert.ok(parameters.materials.length>=4);
  assert.ok(parameters.paper.uv.length>=8);assert.ok(parameters.UI.length>20);
  assert.ok(parameters.paper.material.some(m=>m.kind==='MeshLambertMaterial'),'Independent paper material');
  assert.ok(parameters.paper.tileUV.length>0,'Actual instance atlas UV');
  assert.ok(parameters.postprocessing.AO.kernelSize>0,'Actual AO kernel size');
  for(const surface of ['Frosted_Polymer','Ivory_Edges','Optical_Diffuser'])assert.ok(parameters.materials.some(m=>m.surface===surface),'Missing final surface '+surface);
  for(const instance of parameters.instances)assert.equal(instance.matrix.length,instance.count*16);
}
const order=await read('docs/contracts/library-order.json');assert.equal(order.fixtureHash,fixtureHash);
for(const [name,library]of Object.entries(fixture.libraries)) {
 assert.equal(order.libraries[name].albumIds.length,library.albums.length);
 assert.equal(new Set(order.libraries[name].columns.flatMap(c=>c.albumIds)).size,library.albums.length);
}
const motion=await read('docs/contracts/motion-golden.json');assert.equal(motion.baseline,expected);
assert.deepEqual([...new Set(motion.samples.map(s=>s.fps))],[20,30,60,120]);
assert.ok(motion.inputs.frameOrder);assert.ok(motion.glyphSamples.length>=56);assert.ok(motion.longFrames.some((f,i,a)=>i&&f.input-a[i-1].input>.05));
for(const sample of motion.samples)for(const state of Object.values(sample.quintic))for(const key of ['value','velocity','acceleration'])assert.ok(Number.isFinite(state[key]));
const springReference=motion.samples.filter(s=>s.fps===120);
let maximumDampError=0;
for(const sample of motion.samples) {
  const ref=springReference.find(s=>Math.abs(s.time-sample.time)<1e-10);
  if(ref)for(const key of ['value','velocity'])maximumDampError=Math.max(maximumDampError,Math.abs(ref.spring[key]-sample.spring[key]));
}
assert.ok(maximumDampError<=1e-6,'Damp FPS equivalence');
const presentation=await read('docs/contracts/presentation-golden.json');assert.equal(presentation.step,1/120);assert.equal(presentation.samples.length,1081);
assert.equal(presentation.samples.at(-1).state.phase,'archive');
for(const phase of ['switch-hiding','switching','detail','hiding','returning','archive']) assert.ok(presentation.samples.some(s=>s.state.phase===phase),'Missing actual switch phase '+phase);
assert.equal(presentation.samples.find(s=>s.time>2&&s.time<3).state.selected,'fixture-album-0002');
assert.ok(scene.sourceEasings.length>=8);
const environment=await read('docs/contracts/room-environment.json');assert.equal(hash(await fs.readFile(path.join(root,environment.sampleFile))),environment.sha256);
const report=await read('verification/windows/web-baseline/capture-report.json');assert.equal(report.result,'passed');assert.equal(report.errors.length,0);assert.equal(report.blockedExternalRequests.length,0);
const checks=await read('verification/windows/web-baseline/production-checks.json');assert.equal(checks.checks.length,13);assert.ok(checks.checks.every(c=>c.result==='passed'));
const result={baseline:expected,fixtureHash,sourceModules:source.modules.length,captures:manifest.captures.length,productionChecks:checks.checks.length,presentationSamples:presentation.samples.length,motionSamples:motion.samples.length,maximumDampError,result:'passed'};
await fs.writeFile(path.join(root,'verification/windows/web-baseline/contract-check.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result,null,2));
