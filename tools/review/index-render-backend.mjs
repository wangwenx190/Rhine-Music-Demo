import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
const root=path.resolve(import.meta.dirname,'../..'),out='verification/windows/render-backend';
const hash=data=>crypto.createHash('sha256').update(data).digest('hex');
const sources=['CMakeLists.txt','src/render/SceneAdapter.h','src/render/RenderBackend.h','src/app/CMakeLists.txt','src/app/MaterialReview.cpp','qml/Yalahoo/Music/scene/CaseGeometry.qml','qml/Yalahoo/Music/scene/GeometryScene.qml','src/app/RenderProbe.cpp','src/app/RenderProbe.h','tools/review/CMakeLists.txt','tools/review/ImageComparison.cpp','tools/review/ImageComparison.h','tools/review/CompareImages.cpp','tools/review/RenderBackendAudit.cpp','tools/review/InstanceProbe.qml','tools/review/instanced-glass.vert','tools/review/instanced-paper.vert','tools/review/probe-atlas.vert','tools/review/probe-atlas.frag','tools/review/export-render-backend.mjs','tools/review/export-postprocess-visual.mjs','tools/review/index-render-backend.mjs','tools/audit/bake-render-probe.mjs','tools/audit/audit-r10.ps1','tools/audit/r10-rules.json','tools/build/verify-r10.ps1','tests/unit/ImageComparisonTest.cpp','tests/unit/CMakeLists.txt','docs/contracts/render-backend.json','docs/contracts/render-backend.md'];
const records=[];
for(const file of sources){const text=await fs.readFile(path.join(root,file),'utf8');records.push({path:file,sha256:hash(text.replaceAll('\r\n','\n')),encoding:'UTF-8 normalized LF'});}
const artifacts=[];
for(const folder of ['docs/media/render-backend/web','docs/media/render-backend/qt',out,'tests/fixtures/render-backend']){
 for(const name of (await fs.readdir(path.join(root,folder))).sort()){
  if(name==='evidence-index.json'||name==='evidence-index.txt'||name==='report.md')continue;
  const file=folder+'/'+name,bytes=await fs.readFile(path.join(root,file));artifacts.push({path:file,sha256:hash(bytes),bytes:bytes.length});
 }
}
await fs.writeFile(path.join(root,out,'evidence-index.json'),JSON.stringify({baseline:'57f4f845c20cce646a15dbf4008c2a162aaaf4e2',sources,sourceRecords:records,artifacts},null,2)+'\n');
console.log(`${records.length} source hashes and ${artifacts.length} artifact hashes indexed.`);
