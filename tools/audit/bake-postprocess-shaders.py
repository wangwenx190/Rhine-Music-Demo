"""Compile public Effect snippets with the selected Kit for all native targets."""
from shader_bake import ROOT as root, ShaderBaker
baker = ShaderBaker('postprocess')
header = '''#version 440
layout(location=0)in vec2 INPUT_UV;
layout(location=0)out vec4 outputColor;
layout(std140,binding=0)uniform Params {
 float FRAMEBUFFER_Y_UP;vec2 INPUT_SIZE;float cameraNear;float cameraFar;
 float focus;float aperture;float maxblur;float aspect;float exposure;
 bool rawChannel;bool outputAces;
};
#define FRAGCOLOR outputColor
'''
for i, name in enumerate(['INPUT', 'depthMap', 'areaMap', 'searchMap', 'weightMap'], 1):
    header += f'layout(binding={i})uniform sampler2D {name};\n'
records = []
for name in [
    'bokeh.frag',
    'smaa-edges.frag',
    'smaa-weights.frag',
    'smaa-blend.frag',
    'material-output.frag'
]:
    p = root / 'resources/shaders' / name
    source = p.read_text(encoding='utf-8')
    records.append(baker.bake(name, header + source.replace('void MAIN()', 'void main()')))
baker.report('postprocess', {'qtVersion': baker.chain['qtVersion'], 'shaders': records,
                          'scope': 'Static Effect-wrapper translation; target GPU runtime is checked separately'})
print('Five postprocess shaders baked for five targets')
