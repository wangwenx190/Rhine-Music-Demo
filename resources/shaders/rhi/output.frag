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
// Frozen Three ACESFilm, shared with the verified R07 output.
vec3 aces(vec3 c)
{
    const mat3 inputMat = mat3(vec3(.59719, .07600, .02840), vec3(.35458, .90834, .13383),
                               vec3(.04823, .01566, .83777));
    const mat3 outputMat = mat3(vec3(1.60475, -.10208, -.00327), vec3(-.53108, 1.10813, -.07276),
                                vec3(-.07367, -.00605, 1.07602));
    c = inputMat * (c * f.options.w / .6);
    c = (c * (c + .0245786) - .000090537) / (c * (.983729 * c + .4329510) + .238081);
    return clamp(outputMat * c, 0.0, 1.0);
}
void main()
{
    vec2 coord = uv;
    if (f.options.x > .5)
        coord.y = 1.0 - coord.y;
    vec3 c = texture(colorMap, coord).rgb;
    if (f.options.z > .5)
        c = aces(c);
    color = vec4(f.options.y > .5 ? c : linearToSrgb(c), 1);
}
