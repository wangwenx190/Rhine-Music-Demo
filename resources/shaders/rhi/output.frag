#version 440
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Output
{
    vec4 options;
    vec4 hdr;
}
f;
#include "color-output.glsl"
void main()
{
    vec2 coord = uv;
    if (f.options.x > .5)
        coord.y = 1.0 - coord.y;
    vec3 c = texture(colorMap, coord).rgb;
    if (f.options.z > .5) {
        vec3 mapped = aces(c, f.options.w);
        if (f.hdr.x > 1.0) {
            // Keep the authored SDR base and extend only scene highlights above
            // reference white. A bounded shoulder follows current headroom.
            float range = f.hdr.x - 1.0;
            vec3 highlight = max(c * f.options.w - 1.0, vec3(0));
            mapped += range * (1.0 - exp(-highlight / range));
        }
        c = mapped;
    }
    color = vec4(f.options.y > .5 ? c : linearToSrgb(c), 1);
}
