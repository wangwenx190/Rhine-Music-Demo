"""Adapt the verified R07/R08 shell mathematics to explicit RHI inputs.

No source constants, optics, BRDF, PMREM, LTC or bicubic coefficients are fitted here.
Shadow reception uses the R10A source filter; transmission always reads opaque capture.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
directory = root / 'resources/shaders/rhi'
source = (root / 'resources/shaders/case-glass.frag').read_text(encoding='utf-8')
source = re.sub(r'^VARYING .*;\n', '', source, flags=re.M)
source = source.replace('void MAIN()', 'void main()')
source = source.replace('vec3 n = normalize(worldNormal);',
                        'vec3 n = normalize(worldNormal);\n'
                        '    keyShadow = lightingEnabled && dot(n, normalize(vec3(-6, 14, -5))) > 0.0\n'
                        '            ? shadowFactor() : 1.0;')
source = source.replace('vh = max(dot(v, h), 0.0);',
                        'vh = max(dot(v, h), 0.0);\n'
                        '    if (nl == 0.0)\n'
                        '        return vec3(0);')
source = source.replace('keyColor * lightIntensities.x',
                        'keyColor * lightIntensities.x * keyShadow')
source = source.replace('vec3 direct = lightingEnabled ? musicIrradiance(worldPosition, n) : paperIrradiance;',
                        'vec3 direct = lightingEnabled ? musicIrradiance(worldPosition, n) : vec3(1);')
source = source.replace('    FRAGCOLOR = vec4(compositorOutput ? outgoing : ACESFilmicToneMapping(outgoing), 1.0);',
                        '    color = vec4(lightingEnabled ? outgoing : ACESFilmicToneMapping(outgoing), 1.0);\n'
                        '    normalOut = vec4(normalize(viewNormal) * .5 + .5, 1);\n'
                        '    depthOut = vec4(packDepth((viewDepth - f.clip.x) / (f.clip.y - f.clip.x)), 1);')
mapping = {
    'rowColumn': 'f.rowColumn.xyz',
    'rowSettings': 'f.rowSettings',
    'lightColumn': 'f.lightColumn.xyz',
    'printAmbient': 'f.settings.x',
    'rowFill': 'f.settings.z',
    'rowDetail': 'f.settings.y',
    'spotPosition': 'f.spotPosition.xyz',
    'spotTarget': 'f.spotTarget.xyz',
    'spotColor': 'f.spotColor.xyz',
    'spotIntensity': 'f.optics.x',
    'groundColor': 'f.groundColor.xyz',
    'skyColor': 'f.skyColor.xyz',
    'lightIntensities': 'f.lightIntensities.xyz',
    'keyColor': 'f.keyColor.xyz',
    'scatterColor': 'f.scatter.xyz',
    'scatterStrength': 'f.scatter.w',
    'areaPosition': 'f.areaPosition.xyz',
    'areaTarget': 'f.areaTarget.xyz',
    'areaIntensity': 'f.areaPosition.w',
    'CAMERA_POSITION': 'f.camera.xyz',
    'VIEWPROJECTION_MATRIX': 'f.nativeVP',
    'FRAGCOORD': 'gl_FragCoord',
    'SCREEN_MIP_TEXTURE': 'captureMap',
    'worldPosition': 'world',
    'lightOrigin': 'f.model[3].xyz',
    'roughness': 'f.material.y',
    'thickness': 'f.material.z',
    'transmission': 'f.material.x',
    'attenuationDistance': 'f.material.w',
    'clearcoat': 'f.coat.x',
    'coatRoughness': 'f.coat.y',
    'surface': 'int(f.coat.z)',
    'environmentIntensity': 'f.coat.w',
    'exposure': 'f.camera.w',
    'lightingEnabled': '(f.areaTarget.w > .5)',
    'mix(nightTint.rgb, dayTint.rgb, warmth)': 'f.tint.rgb',
    'mix(nightAttenuation.rgb, dayAttenuation.rgb, warmth)': 'f.attenuation.rgb',
}
source = source.replace('mix(nightTint.rgb, dayTint.rgb, warmth)', 'f.tint.rgb')
source = source.replace('mix(nightAttenuation.rgb, dayAttenuation.rgb, warmth)', 'f.attenuation.rgb')
a = source.index('vec3 musicShell(')
b = source.index('float LTC_', a)
source = source[:a] + re.sub(r'\bsurface\b', 'shellSurface', source[a:b]) + source[b:]
# Function parameters named roughness must remain valid declarations in PMREM helpers.
source = source.replace('float roughness)', 'float pmremRoughness)')
a = source.index('float roughnessToMip(')
b = source.index('float w0(', a)
source = source[:a] + re.sub(r'\broughness\b', 'pmremRoughness', source[a:b]) + source[b:]
for name, replacement in mapping.items():
    if '(' not in name:
        source = re.sub(r'\b' + re.escape(name) + r'\b', replacement, source)
# These source expressions depend only on the immutable entity origin and frame settings.
# RhiItem evaluates them once per entity; local geometry/normal/view terms stay in the shader.
source = re.sub(r'float musicRowField\(vec3 origin\)\s*\{.*?\n\}',
                'float musicRowField(vec3 origin)\n{\n    return f.originLight0.x;\n}',
                source, flags=re.S)
source = re.sub(r'float musicPrintExposure\(vec3 origin\)\s*\{.*?\n\}',
                'float musicPrintExposure(vec3 origin)\n{\n    return f.originLight0.y;\n}',
                source, flags=re.S)
a = source.index('    float laneDistance =')
b = source.index('    float edgeTransport =', a)
source = source[:a] + ('    float laneLight = f.originLight0.z;\n'
                       '    float guided = f.originLight0.w;\n'
                       '    float hot = f.originLight1.x;\n') + source[b:]
source = source.replace('    if ((f.areaTarget.w > .5) && f.areaPosition.w > 0.0)\n'
                        '        musicArea(n, v, world, r, f0, areaDiffuse, areaSpecular);',
                        '#if AREA_ENABLED\n'
                        '    if ((f.areaTarget.w > .5) && f.areaPosition.w > 0.0)\n'
                        '        musicArea(n, v, world, r, f0, areaDiffuse, areaSpecular);\n'
                        '#endif')
a = source.index('        if (f.areaPosition.w > 0.0) {')
b = source.index('\n        }', a) + len('\n        }')
source = source[:a] + '#if AREA_ENABLED\n' + source[a:b] + '\n#endif' + source[b:]
prefix = '''#version 440
#ifndef AREA_ENABLED
#define AREA_ENABLED 1
#endif
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(binding = 2) uniform sampler2D environmentMap;
layout(binding = 3) uniform sampler2DShadow shadowMap;
layout(binding = 4) uniform sampler2D captureMap;
layout(binding = 5) uniform sampler2D dfgMap;
layout(binding = 6) uniform sampler2D ltc1Map;
layout(binding = 7) uniform sampler2D ltc2Map;
layout(location = 0) centroid in vec2 paperUv;
layout(location = 1) in vec3 world;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in float viewDepth;
layout(location = 5) in vec3 localPosition;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 depthOut;
#include "shadow-filter.glsl"
float keyShadow;
'''
(directory / 'shell.frag').write_text(prefix + source, encoding='utf-8', newline='\n')
(directory / 'shell-basic.frag').write_text(prefix.replace('#define AREA_ENABLED 1',
                                                         '#define AREA_ENABLED 0') + source,
                                          encoding='utf-8', newline='\n')
