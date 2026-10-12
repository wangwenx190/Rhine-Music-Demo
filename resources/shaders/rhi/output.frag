#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Output
{
    vec4 options;
}
f;
vec3 linearToSrgb(vec3 c)
{
    return mix(c * 12.92, 1.055 * pow(max(c, vec3(0)), vec3(1.0 / 2.4)) - .055,
               step(vec3(.0031308), c));
}
void main()
{
    vec2 coord = uv;
    if (f.options.x > .5)
        coord.y = 1.0 - coord.y;
    vec3 c = texture(colorMap, coord).rgb;
    color = vec4(f.options.y > .5 ? c : linearToSrgb(c), 1);
}
