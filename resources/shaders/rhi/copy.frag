#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 depthOut;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Output
{
    vec4 options;
}
f;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D depthMap;
void main()
{
    vec2 coord = f.options.x > .5 ? vec2(uv.x, 1.0 - uv.y) : uv;
    color = texture(colorMap, coord);
    normalOut = texture(normalMap, coord);
    depthOut = texture(depthMap, coord);
}
