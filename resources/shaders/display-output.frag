#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    float whiteScale;
    int hdr10;
};
layout(binding = 1) uniform sampler2D source;
vec3 srgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((max(c, vec3(0)) + .055) / 1.055, vec3(2.4)), step(vec3(.04045), c));
}
vec3 pq(vec3 nits)
{
    vec3 v = pow(clamp(nits / 10000.0, 0.0, 1.0), vec3(2610.0 / 16384.0));
    return pow((3424.0 / 4096.0 + 2413.0 / 128.0 * v) / (1.0 + 2392.0 / 128.0 * v),
               vec3(2523.0 / 32.0));
}
void main()
{
    vec4 encoded = texture(source, qt_TexCoord0);
    vec3 linear = srgbToLinear(encoded.rgb / max(encoded.a, .000001));
    if (hdr10 != 0) {
        // D65 linear Rec.709 -> Rec.2020, then absolute ST2084.
        linear = mat3(vec3(.627404, .069097, .016391), vec3(.329283, .919540, .088013),
                      vec3(.043313, .011362, .895595))
                * linear;
        linear = pq(linear * whiteScale);
    } else {
        linear *= whiteScale;
    }
    fragColor = vec4(linear * encoded.a, encoded.a) * qt_Opacity;
}
