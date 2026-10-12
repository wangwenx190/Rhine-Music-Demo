"""Compile public Effect snippets with the selected Kit for all native targets."""
from pathlib import Path
import json,hashlib,subprocess
root=Path(__file__).resolve().parents[2]
chain=json.loads((root/'build/release/toolchain.json').read_text(encoding='utf-8'))
qsb=Path(chain['qtKit'])/'bin/qsb.exe'
work=root/'build/postprocess-shaders';work.mkdir(parents=True,exist_ok=True)
header='''#version 440
layout(location=0)in vec2 INPUT_UV;
layout(location=0)out vec4 outputColor;
layout(std140,binding=0)uniform Params {
 float FRAMEBUFFER_Y_UP;vec2 INPUT_SIZE;float cameraNear;float cameraFar;
 float focus;float aperture;float maxblur;float aspect;float exposure;
 bool rawChannel;bool outputAces;
};
#define FRAGCOLOR outputColor
'''
for i,name in enumerate(['INPUT','depthMap','areaMap','searchMap','weightMap'],1):header+=f'layout(binding={i})uniform sampler2D {name};\n'
records=[]
for name in ['bokeh.frag','smaa-edges.frag','smaa-weights.frag','smaa-blend.frag','material-output.frag']:
 p=root/'resources/shaders'/name;source=p.read_text(encoding='utf-8');f=work/name;out=work/(name+'.qsb')
 f.write_text(header+source.replace('void MAIN()','void main()'),encoding='utf-8',newline='\n')
 r=subprocess.run([str(qsb),'--glsl','330,300 es','--hlsl','50','--msl','12','-o',str(out),str(f)],capture_output=True,text=True)
 if r.returncode:raise RuntimeError(name+': '+r.stdout+r.stderr)
 dump=subprocess.run([str(qsb),'--dump',str(out)],capture_output=True,text=True,check=True).stdout
 if not all(t in dump for t in ['SPIRV','HLSL','MSL','GLSL']):raise RuntimeError('Missing variant')
 records.append({'source':p.relative_to(root).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'targets':['SPIR-V','GLSL 330','GLSL ES 300','HLSL 50','MSL 12'],'passed':True,'diagnostics':r.stderr})
out=root/'verification/windows/postprocess';out.mkdir(parents=True,exist_ok=True)
(out/'shader-variants.json').write_text(json.dumps({'qtVersion':chain['qtVersion'],'shaders':records},indent=2)+'\n',encoding='utf-8',newline='\n')
print('Five postprocess shaders baked for five targets')
