import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync, spawn} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import crypto from 'node:crypto';
import {CDP} from './cdp.mjs';
const root = path.resolve(import.meta.dirname, '../..'),
      baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const work = path.join(root, 'build/lighting-baseline'), frozen = path.join(work, 'web');
const media = path.join(root, 'docs/media/lighting/web'),
      fixtures = path.join(root, 'tests/fixtures/lighting');
for (const d of [frozen, media, fixtures, path.join(root, 'resources/environment')])
    await fs.mkdir(d, {recursive: true});
execFileSync(
    'git',
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
const key=createArchiveLighting(renderer,scene);key.castShadow=false;
const lighting=new MusicSelectionLighting(scene),selected=new T.Object3D();scene.add(selected);
for(const m of model.children){const previous=m.material.onBeforeCompile;m.material.onBeforeCompile=(shader,renderer)=>{previous.call(m.material,shader,renderer);lighting.shade(shader,m.userData.surface);};}
let print;
window.renderCase=async c=>{
 model.position.fromArray(c.origin);model.updateMatrixWorld();camera.position.fromArray(c.position); camera.lookAt(c.origin[0],1.85+c.origin[1],c.origin[2]); camera.updateMatrixWorld();
 scene.background=new T.Color(c.day?'#eae5e1':'#07111f');renderer.toneMappingExposure=c.day?1.05:1.08;
key.color.set(c.day?'#fff7ed':'#e5f0ff');for(const child of scene.children)if(child.isHemisphereLight){child.color.set(c.day?'#fffaf5':'#e2eeff');child.groundColor.set(c.day?'#b4a18c':'#56708c');}
lighting.setTheme(c.day?'day':'night',key);lighting.setExperiment({mode:c.mode,strength:1.3,width:1.35,offset:.3,fill:1,transitionDuration:1.1});lighting.update(selected,camera,0,true,true,false,c.detail,{row:0,lane:0});
 if(print){scene.remove(print);print.geometry.dispose();print.material.map.dispose();print.material.dispose();}
 const image=await new Promise((res,rej)=>{const im=new Image();im.onload=()=>res(im);im.onerror=rej;im.src='/demo-covers/'+c.cover+'.png';});
 const canvas=document.createElement('canvas');canvas.width=canvas.height=1024;paintCover(canvas,undefined,{source:image,width:image.width,height:image.height});
 const map=new T.CanvasTexture(canvas);map.colorSpace=T.SRGBColorSpace;
 print=new T.Mesh(new T.PlaneGeometry(3.06,3.06).translate(.17,1.85,.027),createAlbumPrintMaterial(map));const oldPrint=print.material.onBeforeCompile;print.material.onBeforeCompile=shader=>{oldPrint(shader);lighting.shadePrint(shader);};print.position.fromArray(c.origin);print.visible=c.layer==='all'||c.layer==='paper';scene.add(print);
 // Isolate materials, preserving the same background capture and environment.

 for(const m of model.children){configureMusicGlass(m.userData.surface,m.material);const transition=new ThemeTransition();
 if(!c.day){m.material.color.set(m.userData.surface==='Optical_Diffuser'?'#c6d6e5':'#f6fbff');if(m.userData.surface!=='Optical_Diffuser')m.material.attenuationColor.set('#dceafd');}
 setMusicGlassTheme(m.userData.surface,m.material,c.day,transition);transition.finish();if(m.userData.surface==='Frosted_Polymer')setMusicGlassClarity(m.material,c.clarity,c.day?1:0);
 m.visible=c.layer==='all'||c.layer===m.userData.surface;}
 renderer.render(scene,camera);
 return {png:renderer.domElement.toDataURL('image/png').split(',')[1],projections:[[-2.225,.175,.07],[2.225,.175,.07],[2.225,3.525,.07],[-2.225,3.525,.07],[.17,1.85,.027]].map(p=>{const world=p.map((v,i)=>v+c.origin[i]);const ndc=new T.Vector3(...world).project(camera);return {world,pixel:[(ndc.x+1)*480,(1-ndc.y)*360]};}),parameters:model.children.map(m=>({surface:m.userData.surface,color:m.material.color.toArray(),attenuation:m.material.attenuationColor.toArray(),transmission:m.material.transmission,roughness:m.material.roughness,thickness:m.material.thickness,attenuationDistance:m.material.attenuationDistance}))};
};window.ready=true;
</script></body></html>`;
// Resolve asset URL from frozen source rather than guessing the old model filename.
const assetSource = await fs.readFile(path.join(frozen, 'src/music-cd-asset.ts'), 'utf8');
const modelUrl = assetSource.match(/MUSIC_CD_ASSET\s*=\s*["']([^"']+)/)?.[1];
if (!modelUrl)
    throw Error('Frozen model URL missing');
const server = await createServer({
    root: frozen,
    configFile: false,
    logLevel: 'error',
    server: {host: '127.0.0.1', port: 0},
    plugins: [
        {
            name: 'single-case',
            configureServer(s) {
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
const profile = path.join(work, 'browser-profile');
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
        void page.send(
            e.request.url.startsWith(origin + '/') || /^(data|blob):/.test(e.request.url)
                ? 'Fetch.continueRequest'
                : 'Fetch.failRequest',
            {
                requestId: e.requestId,
                ...(e.request.url.startsWith(origin + '/') || /^(data|blob):/.test(e.request.url)
                        ? {}
                        : {errorReason: 'BlockedByClient'}),
            },
        );
    });
    await page.send('Page.navigate', {url: origin + '/material-review'});
    await wait(() => page.evaluate('window.ready'));
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
        sources[f] = sha(execFileSync('git', ['show', baseline + ':' + f], {cwd: root}));
    const cases = [];
    for (const day of [true, false])
        for (const mode of ['baseline', 'area', 'guided', 'hybrid'])
            for (const [angle, base] of [['front', [0, 1.85, 9]], ['low', [6, 0.3, 6]], ])
                for (const row of [0, 1, 4]) {
                    const origin = [0, 0, row * 0.62], position = base.map((v, i) => v + origin[i]);
                    const c = {
                        index: cases.length,
                        width: 960,
                        height: 720,
                        day,
                        mode,
                        angle,
                        position,
                        origin,
                        cover: 'square',
                        layer: 'all',
                        clarity: 0,
                        detail: 0,
                    };
                    const result = await page.evaluate(
                        'window.renderCase(' + JSON.stringify(c) + ')',
                    );
                    c.parameters = result.parameters;
                    c.projections = result.projections;
                    c.screenshot = String(c.index).padStart(3, '0') + '.png';
                    await fs.writeFile(
                        path.join(media, c.screenshot),
                        Buffer.from(result.png, 'base64'),
                    );
                    cases.push(c);
                }
    if (errors.length)
        throw Error(JSON.stringify(errors));
    await fs.writeFile(
        path.join(fixtures, 'visual.json'),
        JSON.stringify(
            {
                lighting: true,
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
    await fs.mkdir(path.join(root, 'verification/windows/lighting'), {recursive: true});
    await fs.writeFile(
        path.join(root, 'verification/windows/lighting/web-capture.json'),
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
        'Lighting source capture passed: ' + cases.length + ' states; environment ' + env.width
            + 'x' + env.height,
    );
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
