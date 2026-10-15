"""Extend the frozen single-case source harness with the production compositor."""
raise SystemExit("Historical GPU image capture generator retired; see docs/contracts/source-fidelity-review.md and the source-math export tools.")
from pathlib import Path
from source_text import SourceText
root = Path(__file__).resolve().parents[2]
s = SourceText((root / 'tools/review/export-occlusion-visual.mjs').read_text(encoding='utf-8'))
s = s.replace(
    'occlusion-baseline',
    'postprocess-baseline'
).replace(
    'docs/media/occlusion',
    'docs/media/postprocess'
).replace(
    'tests/fixtures/occlusion',
    'tests/fixtures/postprocess'
).replace(
    'verification/windows/occlusion',
    'verification/windows/postprocess'
)
s = s.replace(
    "import {OutputPass}",
    "import {BokehPass} from '/node_modules/three/examples/jsm/postprocessing/BokehPass.js';\nimport {SMAAPass} from '/node_modules/three/examples/jsm/postprocessing/SMAAPass.js';\nimport {OutputPass}"
)
s = s.replace(
    ' camera.near=c.clipNear',
    ' camera.aspect=c.width/c.height;\n camera.near=c.clipNear'
)
s = s.replace(
    'renderDimensions(q,960,720,1,1,4096)',
    'renderDimensions(q,c.width,c.height,1,c.dpr,4096)'
)
s = s.replace('if(c.channel===0)composer.addPass(new OutputPass());', '''let bokeh,smaa;
 const focal=new T.Vector3(...c.origin).add(new T.Vector3(0,2,0)).applyMatrix4(camera.matrixWorldInverse);
 const focus=-focal.z,aperture=(.0003+.0005*c.clarity)*q.depthOfField/100;
 if(c.channel===0&&q.depthOfField&&!c.disableBokeh){bokeh=new BokehPass(scene,camera,{focus,aperture,maxblur:.011});composer.addPass(bokeh);bokeh.setSize(dims.width,dims.height);}
 if(c.channel===0&&q.antialias==='smaa'&&!c.disableSmaa){smaa=new SMAAPass();composer.addPass(smaa);smaa.setSize(dims.width,dims.height);await Promise.all([smaa._areaTexture.image.decode(),smaa._searchTexture.image.decode()]);smaa._areaTexture.needsUpdate=smaa._searchTexture.needsUpdate=true;}
 if(c.channel===0)composer.addPass(new OutputPass());
 // A lone RenderPass becomes renderToScreen and applies material ACES/sRGB.
 // Append an explicit identity pass to expose the composer's true linear input.
 if(c.channel===5)composer.addPass(new ShaderPass({uniforms:{tDiffuse:{value:null}},vertexShader:'varying vec2 vUv;void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',fragmentShader:'uniform sampler2D tDiffuse;varying vec2 vUv;void main(){gl_FragColor=texture2D(tDiffuse,vUv);}'}));''')
s = s.replace(
    'captureCanvas.width=960;captureCanvas.height=720',
    'captureCanvas.width=Math.round(c.width*c.dpr);captureCanvas.height=Math.round(c.height*c.dpr)'
)
s = s.replace(
    'drawImage(renderer.domElement,0,0,960,720)',
    'drawImage(renderer.domElement,0,0,captureCanvas.width,captureCanvas.height)'
)
s = s.replace(
    'return {png:screenshotData,',
    'return {focus,aperture,targetWidth:dims.width,targetHeight:dims.height,png:screenshotData,'
)
s = s.replace('(ndc.x+1)*480,(1-ndc.y)*360', '(ndc.x+1)*c.width/2,(1-ndc.y)*c.height/2')
# Reuse the exact R07 direct-render source for its explicitly assigned 075 input.
legacy = SourceText((root / 'tools/review/export-material-golden.mjs').read_text(encoding='utf-8'))
expression = legacy[legacy.after('const html='): legacy.index('// Resolve asset URL')].rstrip().removesuffix(';')
at = s.index('const server=')
s = s[: at] + 'const legacyHtml=' + expression + ';\n' + s[at:]
s = s.replace(
    "s.middlewares.use('/material-review'",
    "s.middlewares.use('/material-legacy',(_,res)=>{res.setHeader('Content-Type','text/html');res.end(legacyHtml.replace(\"'/public/assets/music-case-v040.glb'\",\"'/\"+modelUrl+\"'\"));});s.middlewares.use('/material-review'"
)
a = s.index(' const cases=[];')
b = s.index(' if(errors.length)', a)
s = s[: a] + ''' const cases=[];
 const dims=[[1920,1080],[1280,720],[995,858],[960,900],[930,900],[390,844],[320,720],[2560,1080],[3440,1440]];
 async function capture(input){
 const c={index:cases.length,width:960,height:720,dpr:1,day:true,mode:'guided',angle:'low',position:[6,.3,6],origin:[0,0,0],neighbors:[],quality:'original',channel:0,shadows:false,cover:'square',layer:'all',clarity:0,detail:0,...input};
 if(c.group==='size-aa'){
 const scale=Math.max(1,(4/3)/(c.width/c.height));c.position=[6*scale,1.85+(.3-1.85)*scale,6*scale];
 }
 if(c.materialBaseline){await page.send('Page.navigate',{url:origin+'/material-legacy'});await wait(()=>page.evaluate('window.ready'));}
 const result=await page.evaluate('window.renderCase('+JSON.stringify(c)+')');
 if(c.materialBaseline){const v=c.position.map((x,i)=>[0,1.85,0][i]-x),length=Math.hypot(...v);result.focus=v.reduce((sum,x,i)=>sum+x*([0,2,0][i]-c.position[i])/length,0);result.aperture=.0008;result.targetWidth=960;result.targetHeight=720;await page.send('Page.navigate',{url:origin+'/material-review'});await wait(()=>page.evaluate('window.ready'));}
 Object.assign(c,{parameters:result.parameters,projections:result.projections,focus:result.focus,aperture:result.aperture,targetWidth:result.targetWidth,targetHeight:result.targetHeight});
 c.screenshot=String(c.index).padStart(3,'0')+'.png';await fs.writeFile(path.join(media,c.screenshot),Buffer.from(result.png,'base64'));cases.push(c);
 }
 for(const day of [true,false])for(const [width,height] of dims)for(const aaMode of [0,1,2])await capture({day,width,height,aaMode,disableBokeh:true,group:'size-aa'});
 for(const day of [true,false])for(const quality of ['performance','original','high','ultra'])for(const cover of ['square','portrait','landscape'])await capture({day,quality,cover,clarity:.65,detail:.65,group:'quality'});
 for(const day of [true,false])for(const disableBokeh of [true,false])await capture({day,neighbors:[[0,0,4],[0,0,-4]],position:[0,1.85,12],angle:'front',disableBokeh,group:'depth-response'});
 for(const day of [true,false])for(const disableSmaa of [true,false])await capture({day,quality:'high',disableSmaa,disableBokeh:true,group:'smaa-response'});
 for(const day of [true,false])for(const channel of [0,5])await capture({day,channel,disableBokeh:true,group:'output'});
 // Exact R07 075 input: night, oblique, square, isolated print/capture boundary.
 for(const aaMode of [0,1,2])await capture({day:false,position:[6,3.8,6],clarity:1,layer:'all',disableBokeh:true,aaMode,materialBaseline:true,clipNear:.1,clipFar:100,group:'paper-boundary-075'});
 for(const dpr of [1.5,2])for(const day of [true,false])for(const aaMode of [0,1,2])await capture({dpr,day,width:1280,height:720,aaMode,disableBokeh:true,group:'dpr-aa'});
''' + s[b:]
s = s.replace('{occlusion:true,baseline', '{postprocess:true,baseline')
s = s.replace(
    "console.log('Occlusion source capture passed:",
    "console.log('Postprocess source capture passed:"
)
# Separate native processes use real QT_SCALE_FACTOR, never pretend their DPR.
needle = " await fs.mkdir(path.join(root,'verification/windows/postprocess')"
at = s.index(needle)
s = s[: at] + ''' for(const dpr of [1,1.5,2])await fs.writeFile(path.join(fixtures,'visual-dpr-'+dpr+'.json'),JSON.stringify({postprocess:true,baseline,dpr,cases:cases.filter(c=>c.dpr===dpr)},null,2)+'\\n');
''' + s[at:]
s = s.replace(
    " await page.send('Page.navigate'",
    " console.log('Browser attached; navigating frozen harness');\n await page.send('Page.navigate'"
)
s = s.replace(
    " const env=await page.evaluate",
    " console.log('Harness ready; exporting environment');\n const env=await page.evaluate"
)
s = s.replace(
    " const result=await page.evaluate('window.renderCase('",
    " console.log('Capturing '+c.index+' '+c.group+' '+c.width+'x'+c.height+' DPR '+c.dpr);\n const result=await page.evaluate('window.renderCase('"
)
s = s.replace(
    "}finally{page?.close();",
    "}catch(e){console.error(e);throw e;}finally{page?.close();"
)
s = s.replace(
    "{errorReason:'BlockedByClient'})});});",
    "{errorReason:'BlockedByClient'})}).catch(()=>{});});"
)
(root / 'tools/review/export-postprocess-visual.mjs').write_text(s, encoding='utf-8', newline='\n')
print('Frozen production postprocess capture harness prepared')
