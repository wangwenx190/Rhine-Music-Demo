"""Compile unchanged public Effect snippets for all Qt shader targets.

These wrappers verify syntax/target variants; actual generated binding and
prepass/sourceItem direction is tested separately in Windows GPU fixtures.
"""
from pathlib import Path
import json, hashlib, subprocess
root = Path(__file__).resolve().parents[2]
chain = json.loads((root / 'build/release/toolchain.json').read_text())
qsb = Path(chain['qtKit']) / 'bin/qsb.exe'
work = root / 'build/occlusion-shaders'
work.mkdir(parents=True, exist_ok=True)
header = '''#version 440
layout(location=0)in vec2 INPUT_UV;
layout(location=0)out vec4 outputColor;
layout(std140,binding=0)uniform Params {
 mat4 PROJECTION_MATRIX;mat4 INVERSE_PROJECTION_MATRIX;mat4 VIEW_MATRIX;
 float FRAMEBUFFER_Y_UP;float NDC_Y_UP;float NEAR_CLIP_VALUE;
 vec2 INPUT_SIZE;float cameraNear;float cameraFar;float kernelRadius;
 float minDistance;float maxDistance;float aoStrength;int kernelSize;int channel;
 vec3 backgroundLinear;vec3 projectionPlane;bool packedDepth;
};
#define FRAGCOLOR outputColor
'''
for index, name in enumerate(
    ['INPUT', 'DEPTH_TEXTURE', 'NORMAL_ROUGHNESS_TEXTURE', 'geometryMap', 'depthMap', 'samplesMap', 'noiseMap', 'rawMap', 'blurMap'],
    1
):
    header += f'layout(binding={index})uniform sampler2D {name};\n'
records = []
for name in ['ao-prepass.frag', 'ssao.frag', 'ssao-blur.frag', 'ssao-composite.frag']:
    p = root / 'resources/shaders' / name
    source = p.read_text(encoding='utf-8')
    input = work / name
    output = work / (name + '.qsb')
    input.write_text(
        header + source.replace('void MAIN()', 'void main()'),
        encoding='utf-8',
        newline='\n'
    )
    r = subprocess.run(
        [str(qsb), '--glsl', '330,300 es', '--hlsl', '50', '--msl', '12', '-o', str(output), str(input)],
        capture_output=True,
        text=True
    )
    if r.returncode:
        raise RuntimeError(r.stdout + r.stderr)
    dump = subprocess.run(
        [str(qsb), '--dump', str(output)],
        capture_output=True,
        text=True,
        check=True
    ).stdout
    if not all(t in dump for t in ['SPIRV', 'HLSL', 'MSL', 'GLSL']):
        raise RuntimeError('Missing shader variant')
    records.append({
        'source': str(p.relative_to(root)).replace('\\', '/'),
        'sha256': hashlib.sha256(p.read_bytes()).hexdigest(),
        'targets': ['SPIR-V', 'GLSL 330', 'GLSL ES 300', 'HLSL 50', 'MSL 12'],
        'passed': True,
        'diagnostics': r.stderr
    })
out = root / 'verification/windows/occlusion'
out.mkdir(parents=True, exist_ok=True)
(out / 'shader-variants.json').write_text(
    json.dumps({'qtVersion': chain['qtVersion'], 'shaders': records, 'scope': 'unchanged effect snippets; actual generated D3D11 shaders compiled by runtime-occlusion'}, indent=2) + '\n',
    encoding='utf-8',
    newline='\n'
)
print('Four SSAO shaders compiled for five targets with the selected Qt Kit')
