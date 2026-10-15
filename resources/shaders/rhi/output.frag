#version 440
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Output
{
    vec4 options;
}
f;
#include "color-output.glsl"
void main()
{
    vec2 coord = uv;
    if (f.options.x > .5)
        coord.y = 1.0 - coord.y;
    vec3 c = texture(colorMap, coord).rgb;
    if (f.options.z > .5)
        c = aces(c, f.options.w);
    color = vec4(f.options.y > .5 ? c : linearToSrgb(c), 1);
}
