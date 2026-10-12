#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
};
void main()
{
    fragColor = vec4(mix(vec3(0.95, 0.93, 0.87), vec3(0.88, 0.91, 0.87), qt_TexCoord0.y), 1.0)
            * qt_Opacity;
}
