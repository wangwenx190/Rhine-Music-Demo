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
// Standard transfer/ACES fit constants retain their source precision and operation order.
const float SrgbDecodeKnee = .04045;
const float SrgbLinearSlope = 12.92;
const float SrgbOffset = .055;
const float SrgbScale = 1.055;
const float SrgbExponent = 2.4;
const float UnpremultiplyEpsilon = .000001;

vec3 srgbToLinear(vec3 c)
{
    return mix(c / SrgbLinearSlope,
               pow((max(c, vec3(0)) + SrgbOffset) / SrgbScale, vec3(SrgbExponent)),
               step(vec3(SrgbDecodeKnee), c));
}
// ST2084 maps absolute luminance (nits) to the PQ signal domain.
vec3 pq(vec3 nits)
{
    const float PqPeakNits = 10000.0;
    const float PqM1 = 2610.0 / 16384.0;
    const float PqM2 = 2523.0 / 32.0;
    const float PqC1 = 3424.0 / 4096.0;
    const float PqC2 = 2413.0 / 128.0;
    const float PqC3 = 2392.0 / 128.0;
    vec3 v = pow(clamp(nits / PqPeakNits, 0.0, 1.0), vec3(PqM1));
    return pow((PqC1 + PqC2 * v) / (1.0 + PqC3 * v), vec3(PqM2));
}
void main()
{
    vec4 encoded = texture(source, qt_TexCoord0);
    vec3 linear = srgbToLinear(encoded.rgb / max(encoded.a, UnpremultiplyEpsilon));
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
