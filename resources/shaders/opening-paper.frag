#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 pageColor;
    vec4 surfaceColor;
};
void main()
{
    vec2 radius = sqrt(2.0) * vec2(0.51, 0.56);
    float t = clamp(length((qt_TexCoord0 - vec2(0.51, 0.44)) / radius), 0.0, 1.0);
    fragColor = mix(pageColor, surfaceColor, t) * qt_Opacity;
}
