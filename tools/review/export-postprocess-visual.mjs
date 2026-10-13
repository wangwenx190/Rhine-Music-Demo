import {execFrozenGit} from './frozen-git.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync, spawn} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import crypto from 'node:crypto';
import {CDP} from './cdp.mjs';
const root = path.resolve(import.meta.dirname, '../..'),
      baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const work = path.join(root, 'build/postprocess-baseline'), frozen = path.join(work, 'web');
const media = path.join(root, 'docs/media/postprocess/web'),
      fixtures = path.join(root, 'tests/fixtures/postprocess');
for (const d of [frozen, media, fixtures, path.join(root, 'resources/environment')])
    await fs.mkdir(d, {recursive: true});
execFrozenGit(
    [
        'archive',
        '--format=tar',
        '--output=' + path.join(work, 'source.tar'),
        baseline,
        'src',
        'public',
        'package.json',
        'package-lock.json',
    ],
    {cwd: root},
);
execFileSync('tar', ['-xf', path.join(work, 'source.tar'), '-C', frozen]);
try {
    await fs.symlink(
        path.join(root, 'legacy-web/node_modules'),
        path.join(frozen, 'node_modules'),
        'junction',
    );
} catch (e) {
    if (e.code !== 'EEXIST')
        throw e;
}
if (!(await fs.readFile(path.join(root, 'legacy-web/package-lock.json')))
         .equals(
             await fs.readFile(path.join(frozen, 'package-lock.json')),
             ))
    throw Error('Frozen dependency lock differs');
const {createServer} =
    await import(pathToFileURL(path.join(root, 'legacy-web/node_modules/vite/dist/node/index.js')));
const html =
    `<html><body style="margin:0"><script type="importmap">{"imports":{"three":"/node_modules/three/build/three.module.js"}}</script><script type="module">
import * as T from '/node_modules/three/build/three.module.js';
import {GLTFLoader} from '/node_modules/three/examples/jsm/loaders/GLTFLoader.js';
import {RoomEnvironment} from '/node_modules/three/examples/jsm/environments/RoomEnvironment.js';
import {configureMusicGlass,setMusicGlassTheme,setMusicGlassClarity,createAlbumPrintMaterial} from '/src/music-model.ts';
import {ThemeTransition} from '/src/theme-transition.ts';
import {MusicSelectionLighting} from '/src/music-lighting.ts';
import {createArchiveLighting} from '/src/archive-lighting.ts';
import {EffectComposer} from '/node_modules/three/examples/jsm/postprocessing/EffectComposer.js';
import {RenderPass} from '/node_modules/three/examples/jsm/postprocessing/RenderPass.js';
import {SSAOPass} from '/node_modules/three/examples/jsm/postprocessing/SSAOPass.js';
import {BokehPass} from '/node_modules/three/examples/jsm/postprocessing/BokehPass.js';
import {SMAAPass} from '/node_modules/three/examples/jsm/postprocessing/SMAAPass.js';
import {OutputPass} from '/node_modules/three/examples/jsm/postprocessing/OutputPass.js';
import {ShaderPass} from '/node_modules/three/examples/jsm/postprocessing/ShaderPass.js';
import {qualityPresets,normalizeQuality,renderDimensions} from '/src/render-quality.ts';
import {paintCover} from '/src/cover-atlas.ts';
const renderer=new T.WebGLRenderer({antialias:true,preserveDrawingBuffer:true}); renderer.setSize(960,720); renderer.setPixelRatio(1);
renderer.toneMapping=T.ACESFilmicToneMapping; document.body.append(renderer.domElement);
const scene=new T.Scene(),camera=new T.PerspectiveCamera(32,960/720,5,300);
const env=new T.PMREMGenerator(renderer).fromScene(new RoomEnvironment(),.04); scene.environment=env.texture;
const pixels=new Uint16Array(env.width*env.height*4); renderer.readRenderTargetPixels(env,0,0,env.width,env.height,pixels);
window.environment={width:env.width,height:env.height,pixels:Array.from(pixels)};
const gltf=await new GLTFLoader().loadAsync('/public/assets/music-case-v040.glb').catch(()=>new GLTFLoader().loadAsync('/assets/music-case-v040.glb'));
const model=new T.Group(); gltf.scene.updateMatrixWorld(true);
gltf.scene.traverse(m=>{if(!m.isMesh)return; const name=m.material.name.replace(/\\.\\d+$/,''); const mat=new T.MeshPhysicalMaterial(); configureMusicGlass(name,mat); const mesh=new T.Mesh(m.geometry.clone().applyMatrix4(m.matrixWorld),mat); mesh.userData.surface=name; model.add(mesh);}); scene.add(model);
const key=createArchiveLighting(renderer,scene);key.castShadow=true;renderer.shadowMap.enabled=true;renderer.shadowMap.type=T.PCFSoftShadowMap;
Object.assign(key.shadow.camera,{left:-16,right:16,bottom:-15,top:15,near:.1,far:45});key.shadow.mapSize.set(2048,2048);key.shadow.normalBias=.035;key.shadow.bias=-.0003;key.shadow.radius=4;key.shadow.intensity=.32;
const lighting=new MusicSelectionLighting(scene),selected=new T.Object3D();scene.add(selected);
for(const m of model.children){const previous=m.material.onBeforeCompile;m.material.onBeforeCompile=(shader,renderer)=>{previous.call(m.material,shader,renderer);lighting.shade(shader,m.userData.surface);};}
let print;let neighbors=[];
 const shadowFloor=new T.Mesh(new T.PlaneGeometry(12,12),new T.ShaderMaterial({uniforms:T.UniformsUtils.merge([T.UniformsLib.lights]),lights:true,
 vertexShader:'varying vec3 vWorldPosition;\\n#include <common>\\n#include <shadowmap_pars_vertex>\\nvoid main(){vec3 transformed=position;vec3 transformedNormal=normalMatrix*normal;vec4 worldPosition=modelMatrix*vec4(position,1.0);vWorldPosition=worldPosition.xyz;\\n#include <shadowmap_vertex>\\ngl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',
 fragmentShader:'#include <common>\\n#include <packing>\\n#include <lights_pars_begin>\\n#include <shadowmap_pars_fragment>\\n#include <shadowmask_pars_fragment>\\nvoid main(){gl_FragColor=vec4(vec3(getShadowMask()),1.0);}' }));
 shadowFloor.rotation.x=-Math.PI/2;shadowFloor.position.y=-.05;shadowFloor.receiveShadow=true;scene.add(shadowFloor);

window.renderCase=async c=>{
 camera.aspect=c.width/c.height;
 camera.near=c.clipNear??5;camera.far=c.clipFar??300;camera.updateProjectionMatrix();
 model.position.fromArray(c.origin);model.updateMatrixWorld();camera.position.fromArray(c.position); camera.lookAt(c.origin[0],1.85+c.origin[1],c.origin[2]); camera.updateMatrixWorld();
 scene.background=new T.Color(c.day?'#eae5e1':'#07111f');renderer.toneMappingExposure=c.day?1.05:1.08;
key.color.set(c.day?'#fff7ed':'#e5f0ff');for(const child of scene.children)if(child.isHemisphereLight){child.color.set(c.day?'#fffaf5':'#e2eeff');child.groundColor.set(c.day?'#b4a18c':'#56708c');}
lighting.setTheme(c.day?'day':'night',key);lighting.setExperiment({mode:c.mode,strength:1.3,width:1.35,offset:.3,fill:1,transitionDuration:1.1});lighting.update(selected,camera,0,true,true,false,c.detail,{row:0,lane:0});
 if(print){scene.remove(print);print.geometry.dispose();print.material.map.dispose();print.material.dispose();}
 const image=await new Promise((res,rej)=>{const im=new Image();im.onload=()=>res(im);im.onerror=rej;im.src='/demo-covers/'+c.cover+'.png';});
 const canvas=document.createElement('canvas');canvas.width=canvas.height=1024;paintCover(canvas,undefined,{source:image,width:image.width,height:image.height});
 const map=new T.CanvasTexture(canvas);map.colorSpace=T.SRGBColorSpace;
 print=new T.Mesh(new T.PlaneGeometry(3.06,3.06).translate(.17,1.85,.027),createAlbumPrintMaterial(map));const oldPrint=print.material.onBeforeCompile;print.material.onBeforeCompile=shader=>{oldPrint(shader);lighting.shadePrint(shader);};print.position.fromArray(c.origin);print.visible=c.layer==='all'||c.layer==='paper';print.receiveShadow=true;scene.add(print);
 // Isolate materials, preserving the same background capture and environment.

 for(const m of model.children){configureMusicGlass(m.userData.surface,m.material);const transition=new ThemeTransition();
 if(!c.day){m.material.color.set(m.userData.surface==='Optical_Diffuser'?'#c6d6e5':'#f6fbff');if(m.userData.surface!=='Optical_Diffuser')m.material.attenuationColor.set('#dceafd');}
 setMusicGlassTheme(m.userData.surface,m.material,c.day,transition);transition.finish();if(m.userData.surface==='Frosted_Polymer')setMusicGlassClarity(m.material,c.clarity,c.day?1:0);
 m.visible=c.layer==='all'||c.layer===m.userData.surface;}

 for(const o of neighbors){scene.remove(o);}neighbors=[];
 for(const origin of c.neighbors){const clone=model.clone();clone.position.fromArray(origin);for(const m of clone.children){m.castShadow=m.userData.surface==='Optical_Diffuser';m.receiveShadow=true;}scene.add(clone);neighbors.push(clone);const p=print.clone();p.position.fromArray(origin);scene.add(p);neighbors.push(p);}
 for(const m of model.children){m.castShadow=m.userData.surface==='Optical_Diffuser';m.receiveShadow=true;}
 shadowFloor.visible=Boolean(c.shadowProbe);
 const q=qualityPresets[c.quality],dims=renderDimensions(q,c.width,c.height,1,c.dpr,4096);
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
 let bokeh,smaa;
 const focal=new T.Vector3(...c.origin).add(new T.Vector3(0,2,0)).applyMatrix4(camera.matrixWorldInverse);
 const focus=-focal.z,aperture=(.0003+.0005*c.clarity)*q.depthOfField/100;
 if(c.channel===0&&q.depthOfField&&!c.disableBokeh){bokeh=new BokehPass(scene,camera,{focus,aperture,maxblur:.011});composer.addPass(bokeh);bokeh.setSize(dims.width,dims.height);}
 if(c.channel===0&&q.antialias==='smaa'&&!c.disableSmaa){smaa=new SMAAPass();composer.addPass(smaa);smaa.setSize(dims.width,dims.height);await Promise.all([smaa._areaTexture.image.decode(),smaa._searchTexture.image.decode()]);smaa._areaTexture.needsUpdate=smaa._searchTexture.needsUpdate=true;}
 if(c.channel===0)composer.addPass(new OutputPass());
 // A lone RenderPass becomes renderToScreen and applies material ACES/sRGB.
 // Append an explicit identity pass to expose the composer's true linear input.
 if(c.channel===5)composer.addPass(new ShaderPass({uniforms:{tDiffuse:{value:null}},vertexShader:'varying vec2 vUv;void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',fragmentShader:'uniform sampler2D tDiffuse;varying vec2 vUv;void main(){gl_FragColor=texture2D(tDiffuse,vUv);}'}));
 composer.render();
 const captureCanvas=document.createElement('canvas');captureCanvas.width=Math.round(c.width*c.dpr);captureCanvas.height=Math.round(c.height*c.dpr);captureCanvas.getContext('2d').drawImage(renderer.domElement,0,0,captureCanvas.width,captureCanvas.height);
 const screenshotData=captureCanvas.toDataURL('image/png').split(',')[1];
 ao.dispose();ao.noiseTexture.dispose();for(const p of composer.passes)if(p!==ao)p.dispose();composer.dispose();

 return {focus,aperture,targetWidth:dims.width,targetHeight:dims.height,png:screenshotData,projections:[[-2.225,.175,.07],[2.225,.175,.07],[2.225,3.525,.07],[-2.225,3.525,.07],[.17,1.85,.027]].map(p=>{const world=p.map((v,i)=>v+c.origin[i]);const ndc=new T.Vector3(...world).project(camera);return {world,pixel:[(ndc.x+1)*c.width/2,(1-ndc.y)*c.height/2]};}),parameters:model.children.map(m=>({surface:m.userData.surface,color:m.material.color.toArray(),attenuation:m.material.attenuationColor.toArray(),transmission:m.material.transmission,roughness:m.material.roughness,thickness:m.material.thickness,attenuationDistance:m.material.attenuationDistance}))};
};window.ready=true;
</script></body></html>`;
// Resolve asset URL from frozen source rather than guessing the old model filename.
const assetSource = await fs.readFile(path.join(frozen, 'src/music-cd-asset.ts'), 'utf8');
const modelUrl = assetSource.match(/MUSIC_CD_ASSET\s*=\s*["']([^"']+)/)?.[1];
if (!modelUrl)
    throw Error('Frozen model URL missing');
const legacyHtml =
    `<html><body style="margin:0"><script type="importmap">{"imports":{"three":"/node_modules/three/build/three.module.js"}}</script><script type="module">
import * as T from '/node_modules/three/build/three.module.js';
import {GLTFLoader} from '/node_modules/three/examples/jsm/loaders/GLTFLoader.js';
import {RoomEnvironment} from '/node_modules/three/examples/jsm/environments/RoomEnvironment.js';
import {configureMusicGlass,setMusicGlassTheme,setMusicGlassClarity,createAlbumPrintMaterial} from '/src/music-model.ts';
import {ThemeTransition} from '/src/theme-transition.ts';
import {paintCover} from '/src/cover-atlas.ts';
const renderer=new T.WebGLRenderer({antialias:true,preserveDrawingBuffer:true}); renderer.setSize(960,720); renderer.setPixelRatio(1);
renderer.toneMapping=T.ACESFilmicToneMapping; document.body.append(renderer.domElement);
const scene=new T.Scene(),camera=new T.PerspectiveCamera(32,960/720,.1,100);
const env=new T.PMREMGenerator(renderer).fromScene(new RoomEnvironment(),.04); scene.environment=env.texture;
const pixels=new Uint16Array(env.width*env.height*4); renderer.readRenderTargetPixels(env,0,0,env.width,env.height,pixels);
window.environment={width:env.width,height:env.height,pixels:Array.from(pixels)};
const gltf=await new GLTFLoader().loadAsync('/public/assets/music-case-v040.glb').catch(()=>new GLTFLoader().loadAsync('/assets/music-case-v040.glb'));
const model=new T.Group(); gltf.scene.updateMatrixWorld(true);
gltf.scene.traverse(m=>{if(!m.isMesh)return; const name=m.material.name.replace(/\\.\\d+$/,''); const mat=new T.MeshPhysicalMaterial(); configureMusicGlass(name,mat); const mesh=new T.Mesh(m.geometry.clone().applyMatrix4(m.matrixWorld),mat); mesh.userData.surface=name; model.add(mesh);}); scene.add(model);
let print;
window.renderCase=async c=>{
 camera.position.fromArray(c.position); camera.lookAt(0,1.85,0); camera.updateMatrixWorld();
 scene.background=new T.Color(c.day?'#eae5e1':'#07111f'); scene.environmentIntensity=c.day?.48:.68; renderer.toneMappingExposure=c.day?1.05:1.08;
 if(print){scene.remove(print);print.geometry.dispose();print.material.map.dispose();print.material.dispose();}
 const image=await new Promise((res,rej)=>{const im=new Image();im.onload=()=>res(im);im.onerror=rej;im.src='/demo-covers/'+c.cover+'.png';});
 const canvas=document.createElement('canvas');canvas.width=canvas.height=1024;paintCover(canvas,undefined,{source:image,width:image.width,height:image.height});
 const map=new T.CanvasTexture(canvas);map.colorSpace=T.SRGBColorSpace;
 print=new T.Mesh(new T.PlaneGeometry(3.06,3.06),createAlbumPrintMaterial(map));print.position.set(.17,1.85,.027);print.visible=c.layer==='all'||c.layer==='paper';scene.add(print);
 // Isolate materials, preserving the same background capture and environment.
 const hemi=new T.HemisphereLight('#ffffff','#ffffff',Math.PI);scene.add(hemi);
 for(const m of model.children){configureMusicGlass(m.userData.surface,m.material);const transition=new ThemeTransition();
 if(!c.day){m.material.color.set(m.userData.surface==='Optical_Diffuser'?'#c6d6e5':'#f6fbff');if(m.userData.surface!=='Optical_Diffuser')m.material.attenuationColor.set('#dceafd');}
 setMusicGlassTheme(m.userData.surface,m.material,c.day,transition);transition.finish();if(m.userData.surface==='Frosted_Polymer')setMusicGlassClarity(m.material,c.clarity,c.day?1:0);
 m.visible=c.layer==='all'||c.layer===m.userData.surface;}
 renderer.render(scene,camera);scene.remove(hemi);
 return {png:renderer.domElement.toDataURL('image/png').split(',')[1],projections:[[-2.225,.175,.07],[2.225,.175,.07],[2.225,3.525,.07],[-2.225,3.525,.07],[.17,1.85,.027]].map(p=>{const ndc=new T.Vector3(...p).project(camera);return {world:p,pixel:[(ndc.x+1)*480,(1-ndc.y)*360]};}),parameters:model.children.map(m=>({surface:m.userData.surface,color:m.material.color.toArray(),attenuation:m.material.attenuationColor.toArray(),transmission:m.material.transmission,roughness:m.material.roughness,thickness:m.material.thickness,attenuationDistance:m.material.attenuationDistance}))};
};window.ready=true;
</script></body></html>`;
const server = await createServer({
    root: frozen,
    configFile: false,
    logLevel: 'error',
    server: {host: '127.0.0.1', port: 0},
    plugins: [
        {
            name: 'single-case',
            configureServer(s) {
                s.middlewares.use('/material-legacy', (_, res) => {
                    res.setHeader('Content-Type', 'text/html');
                    res.end(
                        legacyHtml.replace(
                            '\'/public/assets/music-case-v040.glb\'',
                            '\'/' + modelUrl + '\'',
                            ),
                    );
                });
                s.middlewares.use('/material-review', (_, res) => {
                    res.setHeader('Content-Type', 'text/html');
                    res.end(
                        html.replace('\'/public/assets/music-case-v040.glb\'',
                                     '\'/' + modelUrl + '\''),
                    );
                });
            },
        },
    ],
});
await server.listen();
const origin = 'http://127.0.0.1:' + server.httpServer.address().port;
const candidates = [
    process.env.REVIEW_BROWSER,
    path.join(process.env.PROGRAMFILES, 'Google/Chrome/Application/chrome.exe'),
    path.join(process.env['PROGRAMFILES(X86)'], 'Microsoft/Edge/Application/msedge.exe'),
].filter(Boolean);
let executable;
for (const p of candidates) {
    try {
        await fs.access(p);
        executable = p;
        break;
    } catch {
    }
}
if (!executable)
    throw Error('Installed browser required');
const profile = path.join(work, 'browser-profile-' + process.pid);
await fs.mkdir(profile, {recursive: true});
await fs.rm(path.join(profile, 'DevToolsActivePort'), {force: true});
const proc = spawn(
    executable,
    [
        '--headless=new',
        '--remote-debugging-port=0',
        '--user-data-dir=' + profile,
        '--no-first-run',
        '--disable-background-networking',
        '--disable-component-update',
        '--disable-extensions',
        '--enable-gpu',
        '--use-angle=d3d11',
        'about:blank',
    ],
    {windowsHide: true, stdio: 'ignore'},
);
const wait = async (probe) => {
    const end = Date.now() + 60000;
    while (Date.now() < end) {
        try {
            const x = await probe();
            if (x)
                return x;
        } catch {
        }
        await new Promise((r) => setTimeout(r, 100));
    }
    throw Error('Capture timeout');
};
let browser, page;
const errors = [];
try {
    const port = (await wait(() => fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')))
                     .split('\n');
    browser = await CDP.connect('ws://127.0.0.1:' + port[0] + port[1]);
    const version = await browser.send('Browser.getVersion'),
          gpu = await browser.send('SystemInfo.getInfo');
    const {targetId} = await browser.send('Target.createTarget', {url: 'about:blank'});
    const targets = await (await fetch('http://127.0.0.1:' + port[0] + '/json/list')).json();
    page = await CDP.connect(targets.find((t) => t.id === targetId).webSocketDebuggerUrl);
    page.on('Runtime.exceptionThrown', (e) => {
        errors.push(e.exceptionDetails);
        console.error(JSON.stringify(e.exceptionDetails));
    });
    page.on('Runtime.consoleAPICalled', (e) => {
        if (e.type === 'error')
            console.error(JSON.stringify(e.args));
    });
    await page.send('Runtime.enable');
    await page.send('Page.enable');
    await page.send('Fetch.enable', {patterns: [{urlPattern: '*'}]});
    page.on('Fetch.requestPaused', (e) => {
        void page
            .send(
                e.request.url.startsWith(origin + '/') || /^(data|blob):/.test(e.request.url)
                    ? 'Fetch.continueRequest'
                    : 'Fetch.failRequest',
                {
                    requestId: e.requestId,
                    ...(e.request.url.startsWith(origin + '/')
                                || /^(data|blob):/.test(e.request.url)
                            ? {}
                            : {errorReason: 'BlockedByClient'}),
                },
                )
            .catch(() => {
                   });
    });
    console.log('Browser attached; navigating frozen harness');
    await page.send('Page.navigate', {url: origin + '/material-review'});
    await wait(() => page.evaluate('window.ready'));
    console.log('Harness ready; exporting environment');
    const env = await page.evaluate('window.environment');
    const data = Buffer.alloc(env.pixels.length * 4);
    function half(h) {
        const sign = h & 32768 ? -1 : 1, e = (h >> 10) & 31, m = h & 1023;
        return (sign
                * (e === 0    ? (Math.pow(2, -14) * m) / 1024
                   : e === 31 ? NaN
                              : Math.pow(2, e - 15) * (1 + m / 1024)));
    }
    env.pixels.forEach((h, i) => {
        const v = half(h);
        if (!Number.isFinite(v))
            throw Error('Nonfinite environment');
        data.writeFloatLE(v, i * 4);
    });
    if (!data.equals(
            await fs.readFile(path.join(root, 'resources/environment/room-pmrem.rgba32f'))))
        throw Error('Room capture changed');
    const sha = (b) => crypto.createHash('sha256').update(b).digest('hex');
    const sources = {};
    for (const f of ['src/music-model.ts',
                     'src/music-lighting.ts',
                     'src/music-row-lighting.ts',
                     'src/theme-transition.ts',
                     'src/archive-lighting.ts',
                     'src/cover-atlas.ts',
                     'src/scene.ts',
                     'src/music-cd-asset.ts',
    ])
        sources[f] = sha(execFrozenGit(['show', baseline + ':' + f], {cwd: root}));
    const cases = [];
    const dims = [
        [1920, 1080],
        [1280, 720],
        [995, 858],
        [960, 900],
        [930, 900],
        [390, 844],
        [320, 720],
        [2560, 1080],
        [3440, 1440],
    ];
    async function capture(input) {
        const c = {
            index: cases.length,
            width: 960,
            height: 720,
            dpr: 1,
            day: true,
            mode: 'guided',
            angle: 'low',
            position: [6, 0.3, 6],
            origin: [0, 0, 0],
            neighbors: [],
            quality: 'original',
            channel: 0,
            shadows: false,
            cover: 'square',
            layer: 'all',
            clarity: 0,
            detail: 0,
            ...input,
        };
        if (c.group === 'size-aa') {
            const scale = Math.max(1, 4 / 3 / (c.width / c.height));
            c.position = [6 * scale, 1.85 + (0.3 - 1.85) * scale, 6 * scale];
        }
        if (c.materialBaseline) {
            await page.send('Page.navigate', {url: origin + '/material-legacy'});
            await wait(() => page.evaluate('window.ready'));
        }
        console.log(
            'Capturing ' + c.index + ' ' + c.group + ' ' + c.width + 'x' + c.height + ' DPR '
                + c.dpr,
        );
        const result = await page.evaluate('window.renderCase(' + JSON.stringify(c) + ')');
        if (c.materialBaseline) {
            const v = c.position.map((x, i) => [0, 1.85, 0][i] - x), length = Math.hypot(...v);
            result.focus = v.reduce(
                (sum, x, i) => sum + (x * ([0, 2, 0][i] - c.position[i])) / length,
                0,
            );
            result.aperture = 0.0008;
            result.targetWidth = 960;
            result.targetHeight = 720;
            await page.send('Page.navigate', {url: origin + '/material-review'});
            await wait(() => page.evaluate('window.ready'));
        }
        Object.assign(c, {
            parameters: result.parameters,
            projections: result.projections,
            focus: result.focus,
            aperture: result.aperture,
            targetWidth: result.targetWidth,
            targetHeight: result.targetHeight,
        });
        c.screenshot = String(c.index).padStart(3, '0') + '.png';
        await fs.writeFile(path.join(media, c.screenshot), Buffer.from(result.png, 'base64'));
        cases.push(c);
    }
    for (const day of [true, false])
        for (const [width, height] of dims)
            for (const aaMode of [0, 1, 2])
                await capture({day, width, height, aaMode, disableBokeh: true, group: 'size-aa'});
    for (const day of [true, false])
        for (const quality of ['performance', 'original', 'high', 'ultra'])
            for (const cover of ['square', 'portrait', 'landscape'])
                await capture({
                    day,
                    quality,
                    cover,
                    clarity: 0.65,
                    detail: 0.65,
                    group: 'quality',
                });
    for (const day of [true, false])
        for (const disableBokeh of [true, false])
            await capture({
                day,
                neighbors: [
                    [0, 0, 4],
                    [0, 0, -4],
                ],
                position: [0, 1.85, 12],
                angle: 'front',
                disableBokeh,
                group: 'depth-response',
            });
    for (const day of [true, false])
        for (const disableSmaa of [true, false])
            await capture({
                day,
                quality: 'high',
                disableSmaa,
                disableBokeh: true,
                group: 'smaa-response',
            });
    for (const day of [true, false])
        for (const channel of [0, 5])
            await capture({day, channel, disableBokeh: true, group: 'output'});
    // Exact R07 075 input: night, oblique, square, isolated print/capture boundary.
    for (const aaMode of [0, 1, 2])
        await capture({
            day: false,
            position: [6, 3.8, 6],
            clarity: 1,
            layer: 'all',
            disableBokeh: true,
            aaMode,
            materialBaseline: true,
            clipNear: 0.1,
            clipFar: 100,
            group: 'paper-boundary-075',
        });
    for (const dpr of [1.5, 2])
        for (const day of [true, false])
            for (const aaMode of [0, 1, 2])
                await capture({
                    dpr,
                    day,
                    width: 1280,
                    height: 720,
                    aaMode,
                    disableBokeh: true,
                    group: 'dpr-aa',
                });
    if (errors.length)
        throw Error(JSON.stringify(errors));
    await fs.writeFile(
        path.join(fixtures, 'visual.json'),
        JSON.stringify(
            {
                postprocess: true,
                baseline,
                sources,
                environment: {
                    width: env.width,
                    height: env.height,
                    sha256: sha(data),
                    format: 'RGBA32F little-endian; original WebGL bottom row first',
                },
                width: 960,
                height: 720,
                dpr: 1,
                fov: 32,
                cases,
            },
            null,
            2,
            )
            + '\n',
    );
    for (const dpr of [1, 1.5, 2])
        await fs.writeFile(
            path.join(fixtures, 'visual-dpr-' + dpr + '.json'),
            JSON.stringify(
                {postprocess: true, baseline, dpr, cases: cases.filter((c) => c.dpr === dpr)},
                null,
                2,
                )
                + '\n',
        );
    await fs.mkdir(path.join(root, 'verification/windows/postprocess'), {recursive: true});
    await fs.writeFile(
        path.join(root, 'verification/windows/postprocess/web-capture.json'),
        JSON.stringify(
            {
                baseline,
                browser: version,
                gpu: gpu.gpu,
                cases: cases.length,
                errors,
                environmentSha256: sha(data),
            },
            null,
            2,
            )
            + '\n',
    );
    console.log(
        'Postprocess source capture passed: ' + cases.length + ' states; environment ' + env.width
            + 'x' + env.height,
    );
} catch (e) {
    console.error(e);
    throw e;
} finally {
    page?.close();
    if (browser) {
        try {
            await browser.send('Browser.close');
        } catch {
        }
        browser.close();
    }
    proc.kill();
    await server.close();
}
