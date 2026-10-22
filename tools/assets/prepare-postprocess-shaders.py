"""Port the locked Three postprocessing kernels to shared Qt shader snippets."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[2]
deps = root / 'legacy-web/node_modules/three/examples/jsm/shaders'
out = root / 'resources/shaders'
notice = '// Derived from locked Three.js (MIT); resources/licenses/three-LICENSE.\n'
s = (deps / 'BokehShader.js').read_text(encoding='utf-8')
s = re.search(r'fragmentShader: /\* glsl \*/`([\s\S]+?)`', s)[1]
s = s[s.index('void main()'):]
s = s.replace(
    'void main()',
    'void MAIN()'
).replace(
    'vUv',
    'INPUT_UV'
).replace(
    'texture2D( tColor,',
    'texture( INPUT,'
).replace(
    'gl_FragColor',
    'FRAGCOLOR'
)
s = s.replace(
    'getViewZ( getDepth( INPUT_UV ) )',
    '-mix(cameraNear,cameraFar,dot(texture(depthMap,INPUT_UV).rgb,vec3(65536.0,256.0,1.0)/65793.0))'
)
(out / 'bokeh.frag').write_text(notice + s, encoding='utf-8', newline='\n')
text = (deps / 'SMAAShader.js').read_text(encoding='utf-8')
for name, kind in [
    ('SMAAEdgesShader', 'edges'),
    ('SMAAWeightsShader', 'weights'),
    ('SMAABlendShader', 'blend')
]:
    section = text[text.index('const ' + name):]
    vs, fs = re.findall(r'(?:vertexShader|fragmentShader): /\* glsl \*/`([\s\S]+?)`', section)[: 2]
    defines = {
        'edges': '#define SMAA_THRESHOLD 0.1\n',
        'weights': '#define SMAA_MAX_SEARCH_STEPS 8\n#define SMAA_AREATEX_MAX_DISTANCE 16\n#define SMAA_AREATEX_PIXEL_SIZE (1.0/vec2(160.0,560.0))\n#define SMAA_AREATEX_SUBTEX_SIZE (1.0/7.0)\n',
        'blend': ''
    } [kind]
    # Work in canonical WebGL UVs, transforming only framebuffer reads. Numeric
    # lookup images keep decoded row order (source flipY=false), without color conversion.
    header = notice + defines + '''#define resolution (1.0/INPUT_SIZE)
vec2 frameUv(vec2 uv) { return FRAMEBUFFER_Y_UP>0.0?uv:vec2(uv.x,1.0-uv.y); }
vec4 readFrame(sampler2D tex,vec2 uv) { return texture(tex,frameUv(uv)); }
vec4 readFrame(sampler2D tex,vec2 uv,float bias) { return textureLod(tex,frameUv(uv),0.0); }
'''
    declarations = re.findall(r'varying ([^;]+);', vs)
    header += '\n'.join(d + ';' for d in declarations) + '\n'
    helper = vs[vs.index('void SMAA'): vs.index('void main()')]
    fs = re.sub(r'\s*(uniform|varying) [^;]+;', '', fs)
    # Legacy ES100 fallback is unnecessary for Qt's modern generated variants.
    fs = re.sub(r'#if __VERSION__ == 100[\s\S]*?#endif', '', fs)
    fs = fs.replace('texture2D(', 'readFrame(')
    fs = fs.replace(
        'readFrame( searchTex,',
        'textureLod( searchTex,'
    ).replace(
        'readFrame( areaTex,',
        'textureLod( areaTex,'
    )
    # Clear every edge pixel explicitly; discarding risks stale reused buffers.
    fs = fs.replace('discard;', 'return vec4(0.0);')
    fs = fs.replace(
        'vec2 offset;',
        'vec2 chosenOffset;'
    ).replace(
        'offset.x',
        'chosenOffset.x'
    ).replace(
        'offset.y',
        'chosenOffset.y'
    ).replace(
        'sign( offset )',
        'sign( chosenOffset )'
    )
    sampler = {'edges': 'INPUT', 'weights': 'INPUT', 'blend': 'weightMap'} [kind]
    fs = fs.replace(
        'tDiffuse',
        sampler
    ).replace(
        'tArea',
        'areaMap'
    ).replace(
        'tSearch',
        'searchMap'
    ).replace(
        'tColor',
        'INPUT'
    )
    call = {
        'edges': 'SMAAEdgeDetectionVS',
        'weights': 'SMAABlendingWeightCalculationVS',
        'blend': 'SMAANeighborhoodBlendingVS'
    } [kind]
    fs = fs.replace('void main() {', 'void MAIN() {\n vUv=frameUv(INPUT_UV);\n ' + call + '(vUv);')
    fs = fs.replace('gl_FragColor', 'FRAGCOLOR')
    clean = '\n'.join(line.rstrip() for line in (header + helper + fs).splitlines()).rstrip() + '\n'
    (out / ('smaa-' + kind + '.frag')).write_text(clean, encoding='utf-8', newline='\n')
print('41-tap Bokeh and three SMAA kernels prepared')
