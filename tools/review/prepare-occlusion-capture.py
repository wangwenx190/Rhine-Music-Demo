"""Build the R09 isolated source harness from the established R08 transport.

Only local frozen source/dependencies are loaded. The generated JS is retained
in tools/review so it can also be run directly; this script makes reuse explicit.
"""
raise SystemExit("Historical GPU image capture generator retired; see docs/contracts/source-fidelity-review.md and the source-math export tools.")
from pathlib import Path
from source_text import SourceText
root = Path(__file__).resolve().parents[2]
s = SourceText((root / 'tools/review/export-lighting-visual.mjs').read_text(encoding='utf-8'))
s = s.replace(
    'lighting-baseline',
    'occlusion-baseline'
).replace(
    'docs/media/lighting',
    'docs/media/occlusion'
).replace(
    'tests/fixtures/lighting',
    'tests/fixtures/occlusion'
).replace(
    'verification/windows/lighting',
    'verification/windows/occlusion'
)
s = s.replace(
    "import {paintCover}",
    "import {EffectComposer} from '/node_modules/three/examples/jsm/postprocessing/EffectComposer.js';\nimport {RenderPass} from '/node_modules/three/examples/jsm/postprocessing/RenderPass.js';\nimport {SSAOPass} from '/node_modules/three/examples/jsm/postprocessing/SSAOPass.js';\nimport {OutputPass} from '/node_modules/three/examples/jsm/postprocessing/OutputPass.js';\nimport {ShaderPass} from '/node_modules/three/examples/jsm/postprocessing/ShaderPass.js';\nimport {qualityPresets,normalizeQuality,renderDimensions} from '/src/render-quality.ts';\nimport {paintCover}"
)
s = s.replace('32,960/720,.1,100', '32,960/720,5,300')
s = s.replace(
    ' model.position.fromArray(c.origin);',
    " camera.near=c.clipNear??5;camera.far=c.clipFar??300;camera.updateProjectionMatrix();\n model.position.fromArray(c.origin);"
)
s = s.replace(
    'key.castShadow=false',
    'key.castShadow=true;renderer.shadowMap.enabled=true;renderer.shadowMap.type=T.PCFSoftShadowMap;\nObject.assign(key.shadow.camera,{left:-16,right:16,bottom:-15,top:15,near:.1,far:45});key.shadow.mapSize.set(2048,2048);key.shadow.normalBias=.035;key.shadow.bias=-.0003;key.shadow.radius=4;key.shadow.intensity=.32'
)
s = s.replace('scene.add(print);', 'print.receiveShadow=true;scene.add(print);')
s = s.replace(' renderer.render(scene,camera);', '''
 for(const o of neighbors){scene.remove(o);}neighbors=[];
 for(const origin of c.neighbors){const clone=model.clone();clone.position.fromArray(origin);for(const m of clone.children){m.castShadow=m.userData.surface==='Optical_Diffuser';m.receiveShadow=true;}scene.add(clone);neighbors.push(clone);const p=print.clone();p.position.fromArray(origin);scene.add(p);neighbors.push(p);}
 for(const m of model.children){m.castShadow=m.userData.surface==='Optical_Diffuser';m.receiveShadow=true;}
 const q=qualityPresets[c.quality],dims=renderDimensions(q,960,720,1,1,4096);
 renderer.setSize(dims.width,dims.height,false);renderer.shadowMap.enabled=c.shadows;key.shadow.mapSize.set(q.shadows,q.shadows);key.shadow.map?.dispose();key.shadow.map=null;
 let state=417;const original=Math.random;Math.random=()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/4294967296;};
 let ao;try{ao=new SSAOPass(scene,camera,Math.floor(dims.width*q.aoResolution),Math.floor(dims.height*q.aoResolution),q.aoSamples||16);}finally{Math.random=original;}
 ao.kernelRadius=c.kernelRadius??.18;ao.minDistance=.001;ao.maxDistance=.035;
 if(c.channel===0)ao.copyMaterial.fragmentShader=ao.copyMaterial.fragmentShader.replace('gl_FragColor = opacity * texel;','gl_FragColor = vec4(mix(vec3(1.0), texel.rgb, 0.22), texel.a);');
 const composer=new EffectComposer(renderer);composer.setSize(dims.width,dims.height);composer.addPass(new RenderPass(scene,camera));
 if(c.channel>=1&&c.channel<=4){ao.output=c.channel;composer.addPass(ao);}
 else if(c.channel!==5&&q.aoSamples){composer.addPass(ao);}
 // Match setQuality/resize: AO target may be fractional, compositor is full-sized.
 ao.setSize(Math.floor(dims.width*q.aoResolution),Math.floor(dims.height*q.aoResolution));
 if(c.channel===3){ao.depthRenderMaterial.fragmentShader=ao.depthRenderMaterial.fragmentShader.replace('vec3( 1.0 - depth )','vec3( texture2D(tDepth,vUv).x )');}
 if(c.channel===0)composer.addPass(new OutputPass());
 composer.render();
 const captureCanvas=document.createElement('canvas');captureCanvas.width=960;captureCanvas.height=720;captureCanvas.getContext('2d').drawImage(renderer.domElement,0,0,960,720);
 const screenshotData=captureCanvas.toDataURL('image/png').split(',')[1];
 ao.dispose();ao.noiseTexture.dispose();for(const p of composer.passes)if(p!==ao)p.dispose();composer.dispose();
''')
s = s.replace('let print;', 'let print;let neighbors=[];')
s = s.replace('let print;let neighbors=[];', r'''let print;let neighbors=[];
 const shadowFloor=new T.Mesh(new T.PlaneGeometry(12,12),new T.ShaderMaterial({uniforms:T.UniformsUtils.merge([T.UniformsLib.lights]),lights:true,
 vertexShader:'varying vec3 vWorldPosition;\\n#include <common>\\n#include <shadowmap_pars_vertex>\\nvoid main(){vec3 transformed=position;vec3 transformedNormal=normalMatrix*normal;vec4 worldPosition=modelMatrix*vec4(position,1.0);vWorldPosition=worldPosition.xyz;\\n#include <shadowmap_vertex>\\ngl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',
 fragmentShader:'#include <common>\\n#include <packing>\\n#include <lights_pars_begin>\\n#include <shadowmap_pars_fragment>\\n#include <shadowmask_pars_fragment>\\nvoid main(){gl_FragColor=vec4(vec3(getShadowMask()),1.0);}' }));
 shadowFloor.rotation.x=-Math.PI/2;shadowFloor.position.y=-.05;shadowFloor.receiveShadow=true;scene.add(shadowFloor);
''')
s = s.replace(
    ' const q=qualityPresets[c.quality]',
    ' shadowFloor.visible=Boolean(c.shadowProbe);\n const q=qualityPresets[c.quality]'
)
s = s.replace("path.join(work,'browser-profile')", "path.join(work,'browser-profile-'+process.pid)")
s = s.replace("png:renderer.domElement.toDataURL('image/png').split(',')[1]", "png:screenshotData")
a = s.index(' for(const day of [true,false])for(const mode')
b = s.index(' if(errors.length)', a)
s = s[: a] + ''' for(const day of [true,false])for(const count of [1,3])for(const [angle,base]of [['front',[0,1.85,9]],['low',[6,.3,6]]]) {
 for(const quality of ['performance','original','high','ultra'])for(const channel of [0,1,2,3,4,5]) {
 if(quality!=='original'&&channel!==0)continue;
 for(const shadows of (channel===0&&quality==='original'?[false,true]:[false])) {
 const origin=[0,0,0],position=base,mode='guided';
 const neighbors=count===1?[]:[[0,0,.62],[0,0,-.62]];
 const c={index:cases.length,width:960,height:720,day,mode,angle,position,origin,neighbors,count,quality,channel,shadows,cover:'square',layer:'all',clarity:0,detail:0};
 const result=await page.evaluate('window.renderCase('+JSON.stringify(c)+')');
 c.parameters=result.parameters;c.projections=result.projections;c.screenshot=String(c.index).padStart(3,'0')+'.png';await fs.writeFile(path.join(media,c.screenshot),Buffer.from(result.png,'base64'));cases.push(c);
 }}}
''' + ''' for(const channel of [0,1,2,3,4,5]) {
 const c={index:cases.length,width:960,height:720,day:true,mode:'guided',angle:'low',position:[6,.3,6],origin:[0,0,0],neighbors:[[1.5,0,.12],[-1.5,0,-.12]],count:3,quality:'original',channel,shadows:false,cover:'square',layer:'all',clarity:0,detail:0,clipNear:.1,clipFar:100,kernelRadius:.38,diagnostic:true};
 const result=await page.evaluate('window.renderCase('+JSON.stringify(c)+')');c.parameters=result.parameters;c.projections=result.projections;c.screenshot=String(c.index).padStart(3,'0')+'.png';await fs.writeFile(path.join(media,c.screenshot),Buffer.from(result.png,'base64'));cases.push(c);
 }
''' + ''' for(const shadows of [false,true]) {
 const c={index:cases.length,width:960,height:720,day:true,mode:'guided',angle:'low',position:[6,4,6],origin:[0,0,0],neighbors:[[0,0,.62],[0,0,-.62]],count:3,quality:'original',channel:5,shadows,shadowProbe:true,cover:'square',layer:'all',clarity:0,detail:0,diagnostic:true};
 const result=await page.evaluate('window.renderCase('+JSON.stringify(c)+')');c.parameters=result.parameters;c.projections=result.projections;c.screenshot=String(c.index).padStart(3,'0')+'.png';await fs.writeFile(path.join(media,c.screenshot),Buffer.from(result.png,'base64'));cases.push(c);
 }
''' + s[b:]
s = s.replace('{lighting:true,baseline', '{occlusion:true,baseline')
s = s.replace(
    "console.log('Lighting source capture passed:",
    "console.log('Occlusion source capture passed:"
)
(root / 'tools/review/export-occlusion-visual.mjs').write_text(s, encoding='utf-8', newline='\n')
print('R09 isolated frozen source harness generated')
