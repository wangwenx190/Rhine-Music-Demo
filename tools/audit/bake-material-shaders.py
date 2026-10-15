"""Bake unchanged CustomMaterial snippets in public-uniform wrappers for static targets."""
import json, re, subprocess, hashlib, sys
from pathlib import Path
root = Path(__file__).resolve().parents[2]
kit = json.loads((root / 'build/release/toolchain.json').read_text())['qtKit']
qsb = Path(kit) / 'bin' / ('qsb.exe' if sys.platform == 'win32' else 'qsb')
work = root / 'build/material-shaders'
work.mkdir(parents=True, exist_ok=True)
block = """layout(std140,binding=0) uniform Params {
 mat4 MODEL_MATRIX; mat4 MODELVIEWPROJECTION_MATRIX; mat4 VIEWPROJECTION_MATRIX; mat3 NORMAL_MATRIX;
 vec3 CAMERA_POSITION; int surface; vec4 nightTint; vec4 dayTint; vec4 nightAttenuation; vec4 dayAttenuation;
 float warmth;float roughness;float coatRoughness;float environmentIntensity;float thickness;float attenuationDistance;
 float transmission;float clearcoat;float exposure;vec3 paperIrradiance;vec3 backgroundSrgb;
 bool lightingEnabled;bool rawChannel;bool outputAces;bool compositorOutput;vec3 backgroundLinear;vec3 rowColumn;vec4 rowSettings;float rowDetail;float rowFill;
 vec3 lightColumn;float printAmbient;vec3 scatterColor;float scatterStrength;
 vec3 spotPosition;vec3 spotTarget;vec3 spotColor;float spotIntensity;float areaIntensity;
 vec3 areaPosition;vec3 areaTarget;vec3 keyColor;vec3 skyColor;vec3 groundColor;vec3 lightIntensities;
};
"""
reports = []
for name in [
    'case-glass.vert',
    'case-glass.frag',
    'album-paper.vert',
    'album-paper.frag',
    'material-background.frag',
    'material-output.frag'
]:
    source = (root / 'resources/shaders' / name).read_text(encoding='utf-8')
    stage = name.rsplit('.', 1)[1]
    location = [0]
    def varying(m):
        n = location[0]
        location[0] += 1
        return 'layout(location=' + str(n) + ') ' + ('out' if stage == 'vert' else 'in') + ' ' + m[1] + ' ' + m[2] + ';'
    source = re.sub(
        r'VARYING\s+(\w+)\s+(\w+);',
        varying,
        source
    ).replace(
        'void MAIN()',
        'void materialMain()'
    )
    if stage == 'vert':
        header = '#version 440\n' + block + 'layout(location=0) in vec3 VERTEX;layout(location=1) in vec3 NORMAL;layout(location=2) in vec2 UV0;\n#define POSITION gl_Position\n'
        tail = '\nvoid main(){materialMain();}\n'
    else:
        header = '#version 440\n' + block + 'layout(location=0) out vec4 fragmentColor;\n#define FRAGCOLOR fragmentColor\n#define FRAGCOORD gl_FragCoord\n'
        header += 'layout(binding=1)uniform sampler2D environmentMap;layout(binding=2)uniform sampler2D dfgMap;layout(binding=3)uniform sampler2D SCREEN_MIP_TEXTURE;layout(binding=4)uniform sampler2D coverMap;layout(binding=5)uniform sampler2D INPUT;layout(binding=6)uniform sampler2D ltc1Map;layout(binding=7)uniform sampler2D ltc2Map;\n'
        if name == 'material-output.frag':
            header += 'layout(location=0)in vec2 INPUT_UV;\n'
        if name == 'album-paper.frag':
            header += 'vec4 BASE_COLOR;float METALNESS;float ROUGHNESS;vec4 COLOR_SUM;float SHADOW_CONTRIB=1.0;\n'
            tail = '\nvoid main(){materialMain();POST_PROCESS();fragmentColor=COLOR_SUM;}\n'
        else:
            tail = '\nvoid main(){materialMain();}\n'
    input = work / name
    input.write_text(header + source + tail, encoding='utf-8', newline='\n')
    output = work / (name + '.qsb')
    command = [
        str(qsb),
        '--glsl',
        '330,300 es',
        '--hlsl',
        '50',
        '--msl',
        '12',
        '-o',
        str(output),
        str(input)
    ]
    r = subprocess.run(command, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stdout + r.stderr)
    dump = subprocess.run(
        [str(qsb), '--dump', str(output)],
        capture_output=True,
        text=True,
        check=True
    ).stdout
    if not all(x in dump for x in ['MSL', 'HLSL', 'GLSL', 'SPIRV']):
        raise RuntimeError('Missing shader target')
    (work / (name + '.txt')).write_text(dump, encoding='utf-8', newline='\n')
    reports.append({
        'source': str(Path('resources/shaders') / name),
        'sha256': hashlib.sha256((root / 'resources/shaders' / name).read_bytes()).hexdigest(),
        'targets': ['SPIR-V', 'GLSL 330', 'GLSL ES 300', 'HLSL 50', 'MSL 12'],
        'passed': True,
        'diagnostics': r.stderr,
        'scope': 'Static translation of public-uniform wrappers; no target GPU execution inferred'
    })
platform = 'windows' if sys.platform == 'win32' else 'macos' if sys.platform == 'darwin' else 'linux'
component = 'occlusion' if '--occlusion' in sys.argv else 'lighting' if '--lighting' in sys.argv else 'materials'
report = root / 'verification' / platform / component / 'shader-variants.json'
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(
    json.dumps({'qtKitVersion': json.loads((root / 'build/release/toolchain.json').read_text())['qtVersion'], 'shaders': reports}, indent=2) + '\n',
    encoding='utf-8',
    newline='\n'
)
print('Six material shaders baked for five targets')
