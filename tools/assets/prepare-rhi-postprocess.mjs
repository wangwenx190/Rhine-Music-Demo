// Adapt R09/R09B algorithms with explicit bindings and loop-invariant AO math.
import fs from 'node:fs/promises';
import path from 'node:path';
const root = path.resolve(import.meta.dirname, '../..');
const header = `#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Post {
    vec4 size;
    vec4 clip;
    vec4 ao;
    vec4 dof;
    vec4 options;
    vec4 kernel[64];
} f;
layout(binding = 2) uniform sampler2D depthMap;
layout(binding = 3) uniform sampler2D geometryMap;
layout(binding = 4) uniform sampler2D samplesMap;
layout(binding = 5) uniform sampler2D noiseMap;
layout(binding = 6) uniform sampler2D auxiliaryMap;
layout(binding = 7) uniform sampler2D areaMap;
layout(binding = 8) uniform sampler2D searchMap;
#define INPUT colorMap
#define INPUT_UV (f.clip.z > 0.0 ? uv : vec2(uv.x, 1.0 - uv.y))
#define INPUT_SIZE f.size.xy
#define FRAMEBUFFER_Y_UP f.clip.z
#define cameraNear f.clip.x
#define cameraFar f.clip.y
#define projectionPlane vec3(f.size.zw, 0)
#define kernelSize int(f.ao.x)
#define kernelRadius f.ao.y
#define minDistance f.ao.z
#define maxDistance f.ao.w
#define focus f.dof.x
#define aperture f.dof.y
#define maxblur f.dof.z
#define aspect f.dof.w
#define weightMap auxiliaryMap
#define FRAGCOLOR color
`;
for (const name of ['ssao', 'ssao-blur', 'bokeh', 'smaa-edges', 'smaa-weights', 'smaa-blend']) {
    const source = await fs.readFile(path.join(root, 'resources/shaders', name + '.frag'), 'utf8');
    if (!source.includes('void MAIN()'))
        throw Error('R09 algorithm entry point changed');
    let result = header
                 + source.replace('void MAIN()', 'void main()')
                       .replace('texelFetch(samplesMap, ivec2(i, 0), 0).xyz', 'f.kernel[i].xyz');
    if (name == 'ssao') {
        result = result
                     .replace('mat3(tangent, cross(normal, tangent), normal);',
                              'mat3(tangent, cross(normal, tangent), normal) * kernelRadius;')
                     .replace('basis * f.kernel[i].xyz * kernelRadius', 'basis * f.kernel[i].xyz');
        const loop =
            /for \(int i = 0; i < 64; \+\+i\) \{\s*if \(i >= kernelSize\)\s*break;([\s\S]*?)\n    \}/;
        const match = result.match(loop);
        if (!match)
            throw Error('R09 AO sample loop changed');
        // Keep every authored sample and its accumulation order. Constant indices avoid
        // dynamic uniform-array loads and the per-sample kernel-size branch on native GPUs.
        for (const count of [16, 32, 64]) {
            const blocks =
                Array
                    .from({length: count},
                          (_, i) => '{\n' + match[1].replace(/\bi\b/g, String(i)) + '\n}')
                    .join('\n');
            const variant =
                result.replace(loop, blocks)
                    .replace('#define kernelSize int(f.ao.x)', '#define kernelSize ' + count);
            const filename = count == 32 ? 'ssao' : 'ssao' + count;
            await fs.writeFile(path.join(root, 'resources/shaders/rhi', filename + '.frag'),
                               variant);
        }
        continue;
    }
    await fs.writeFile(path.join(root, 'resources/shaders/rhi', name + '.frag'), result);
}
await fs.writeFile(path.join(root, 'resources/shaders/rhi/ssao-composite.frag'), header + `
void main()
{
    vec4 c = texture(INPUT, INPUT_UV);
    color = vec4(c.rgb * mix(1.0, texture(auxiliaryMap, INPUT_UV).r, f.options.x), c.a);
}
`);

const shaderRoot = path.join(root, 'resources/shaders/rhi');
let vertex = await fs.readFile(path.join(shaderRoot, 'geometry.vert'), 'utf8');
const instanceDeclaration = `layout(location = 3) in vec4 model0;
layout(location = 4) in vec4 model1;
layout(location = 5) in vec4 model2;
layout(location = 6) in vec4 model3;
layout(location = 7) in vec4 coverUv;
layout(location = 8) in vec4 light0;
layout(location = 9) in vec4 light1;
`;
vertex =
    vertex.replace('void main()', instanceDeclaration + 'void main()')
        .replace('    entityLight0 = f.originLight0;',
                 `    mat4 model = mat4(model0, model1, model2, model3);
    entityLight0 = light0;`)
        .replace('entityLight1 = f.originLight1', 'entityLight1 = light1')
        .replace('entityCoverUv = f.coverUv', 'entityCoverUv = coverUv')
        .replaceAll('f.model', 'model')
        .replace('f.mvp * vec4(rasterPosition, 1)', 'f.nativeVP * model * vec4(rasterPosition, 1)');
await fs.writeFile(path.join(shaderRoot, 'geometry-instanced.vert'), vertex);
for (const name of ['shell', 'shell-basic']) {
    const regular = await fs.readFile(path.join(shaderRoot, name + '.frag'), 'utf8');
    const early = regular.replace(
        '#extension GL_GOOGLE_include_directive : require',
        '#extension GL_GOOGLE_include_directive : require\nlayout(early_fragment_tests) in;');
    await fs.writeFile(path.join(shaderRoot, name + '-early.frag'), early);
}
