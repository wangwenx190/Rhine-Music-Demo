// Adapt R09/R09B algorithms with explicit bindings and loop-invariant AO math.
import fs from 'node:fs/promises';
import path from 'node:path';
const root = path.resolve(import.meta.dirname, '../..');
const header =
    '#version 440\n#extension GL_GOOGLE_include_directive : require\n#include "post-input.glsl"\n';
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
        const blocks = Array
                           .from({length: 64}, (_, i) => '#if KERNEL_COUNT > ' + i + '\n{\n'
                                                         + match[1].replace(/\bi\b/g, String(i))
                                                         + '\n}\n#endif')
                           .join('\n');
        await fs.writeFile(path.join(root, 'resources/shaders/rhi/ssao-body.glsl'),
                           result.replace(loop, blocks).slice(header.length));
        for (const count of [16, 32, 64]) {
            const filename = count == 32 ? 'ssao' : 'ssao' + count;
            const entry =
                header.replace('#include "post-input.glsl"',
                               '#define KERNEL_COUNT ' + count + '\n#include "post-input.glsl"');
            await fs.writeFile(path.join(root, 'resources/shaders/rhi', filename + '.frag'),
                               entry + '#include "ssao-body.glsl"\n');
        }
        continue;
    }
    await fs.writeFile(path.join(root, 'resources/shaders/rhi', name + '.frag'),
                       header + '#define MAIN main\n#include "../' + name + '.frag"\n');
}
await fs.writeFile(path.join(root, 'resources/shaders/rhi/ssao-composite.frag'), header + `
void main()
{
    vec4 c = texture(INPUT, INPUT_UV);
    color = vec4(c.rgb * mix(1.0, texture(auxiliaryMap, INPUT_UV).r, f.options.x), c.a);
}
`);
