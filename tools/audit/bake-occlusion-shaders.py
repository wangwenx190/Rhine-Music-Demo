"""Compile unchanged public Effect snippets for all Qt shader targets.

These wrappers verify syntax/target variants; actual generated binding and
prepass/sourceItem direction is tested separately in Windows GPU fixtures.
"""
from shader_bake import ROOT as root, ShaderBaker
baker = ShaderBaker('occlusion')
header = """#version 440
layout(location = 0) in vec2 INPUT_UV;
layout(location = 0) out vec4 outputColor;
layout(std140, binding = 0) uniform Params
{
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    mat4 VIEW_MATRIX;
    float FRAMEBUFFER_Y_UP;
    float NDC_Y_UP;
    float NEAR_CLIP_VALUE;
    vec2 INPUT_SIZE;
    float cameraNear;
    float cameraFar;
    float kernelRadius;
    float minDistance;
    float maxDistance;
    float aoStrength;
    int kernelSize;
    int channel;
    vec3 backgroundLinear;
    vec3 projectionPlane;
    bool packedDepth;
};
#define FRAGCOLOR outputColor
"""
for index, name in enumerate(
    ['INPUT', 'DEPTH_TEXTURE', 'NORMAL_ROUGHNESS_TEXTURE', 'geometryMap', 'depthMap', 'samplesMap', 'noiseMap', 'rawMap', 'blurMap'],
    1
):
    header += f'layout(binding = {index}) uniform sampler2D {name};\n'
records = []
for name in ['ao-prepass.frag', 'ssao.frag', 'ssao-blur.frag', 'ssao-composite.frag']:
    p = root / 'resources/shaders' / name
    source = p.read_text(encoding='utf-8')
    records.append(baker.bake(name, header + source.replace('void MAIN()', 'void main()')))
baker.report('occlusion', {'qtVersion': baker.chain['qtVersion'], 'shaders': records,
                          'scope': 'unchanged effect snippets; static Effect wrappers; actual RHI AO is validated separately by rhi-ao-gpu'})
print('Four SSAO shaders compiled for five targets with the selected Qt Kit')
