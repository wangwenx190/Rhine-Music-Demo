#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 fadeEdges;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    vec2 fade = max(fadeEdges, vec2(0.000001));
    vec2 edge = min(qt_TexCoord0, vec2(1.0) - qt_TexCoord0) / fade;
    float alpha = clamp(min(edge.x, edge.y), 0.0, 1.0);
    fragColor = texture(source, qt_TexCoord0) * (alpha * qt_Opacity);
}
