#version 440
// Fullscreen triangle paired with the production Qt Quick display fragment interface.
layout(location = 0) out vec2 qt_TexCoord0;
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    qt_TexCoord0 = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0, 1);
}
