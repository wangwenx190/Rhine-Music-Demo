import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
const root = path.resolve(import.meta.dirname, '../..');
const chain = JSON.parse(
    await fs.readFile(path.join(root, 'build/release/toolchain.json'), 'utf8'),
);
const qsb = path.join(chain.qtKit, 'bin', process.platform === 'win32' ? 'qsb.exe' : 'qsb');
const work = path.join(root, 'build/render-probe-shaders');
await fs.mkdir(work, {recursive: true});
const records = [];
for (const name of ['instanced-glass.vert',
                    'instanced-paper.vert',
                    'probe-atlas.vert',
                    'probe-atlas.frag',
]) {
    const file = path.join(root, 'tools/review', name), bytes = await fs.readFile(file);
    let s = bytes.toString('utf8'), location = 0;
    const vertex = name.endsWith('.vert');
    s = s.replace(
             /VARYING\s+(\w+)\s+(\w+);/g,
             (_, type, id) =>
                 `layout(location=${location++}) ${vertex ? 'out' : 'in'} ${type} ${id};`,
             )
            .replace('void MAIN()', 'void main()');
    const header = vertex ? `#version 440
layout(location=0)in vec3 VERTEX;layout(location=1)in vec3 NORMAL;layout(location=2)in vec2 UV0;
layout(std140,binding=0)uniform Params {mat4 INSTANCE_MODEL_MATRIX;mat4 INSTANCE_MODELVIEWPROJECTION_MATRIX;vec4 INSTANCE_DATA;};
#define POSITION gl_Position
`
                          : `#version 440
layout(location=0)out vec4 result;
layout(binding=1)uniform sampler2D atlasMap;
#define FRAGCOLOR result
`;
    const input = path.join(work, name), output = input + '.qsb';
    await fs.writeFile(input, header + s);
    execFileSync(
        qsb,
        ['--glsl', '330,300 es', '--hlsl', '50', '--msl', '12', '-o', output, input],
        {stdio: 'pipe'},
    );
    const dump = execFileSync(qsb, ['--dump', output], {encoding: 'utf8'});
    if (!['SPIRV', 'GLSL', 'HLSL', 'MSL'].every((t) => dump.includes(t)))
        throw Error('Missing shader variant');
    records.push({
        source: 'tools/review/' + name,
        sha256: crypto.createHash('sha256').update(bytes).digest('hex'),
        targets: ['SPIR-V', 'GLSL 330', 'GLSL ES 300', 'HLSL 50', 'MSL 12'],
        passed: true,
    });
}
await fs.mkdir(path.join(root, 'verification/windows/render-backend'), {recursive: true});
await fs.writeFile(
    path.join(root, 'verification/windows/render-backend/shader-variants.json'),
    JSON.stringify(
        {qtVersion: chain.qtVersion, configuration: chain.configuration, shaders: records},
        null,
        2,
        )
        + '\n',
);
console.log(
    'Four Review snippets baked for five native targets; target execution remains separate.',
);
