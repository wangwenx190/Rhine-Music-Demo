#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sampleStep;
};
layout(binding = 1) uniform sampler2D source;
// Frozen music.css backdrop-filter blur(8px), sigma 8, truncated at 3 sigma.
void main()
{
    vec4 color = texture(source, qt_TexCoord0) * 0.049976753601;
    color += (texture(source, qt_TexCoord0 + sampleStep * 1.494140893206)
              + texture(source, qt_TexCoord0 - sampleStep * 1.494140893206))
            * 0.098026961961;
    color += (texture(source, qt_TexCoord0 + sampleStep * 3.486331531380)
              + texture(source, qt_TexCoord0 - sampleStep * 3.486331531380))
            * 0.090687786847;
    color += (texture(source, qt_TexCoord0 + sampleStep * 5.478528837539)
              + texture(source, qt_TexCoord0 - sampleStep * 5.478528837539))
            * 0.078834188951;
    color += (texture(source, qt_TexCoord0 + sampleStep * 7.470736606633)
              + texture(source, qt_TexCoord0 - sampleStep * 7.470736606633))
            * 0.064393618225;
    color += (texture(source, qt_TexCoord0 + sampleStep * 9.462958613244)
              + texture(source, qt_TexCoord0 - sampleStep * 9.462958613244))
            * 0.049423478595;
    color += (texture(source, qt_TexCoord0 + sampleStep * 11.455198604302)
              + texture(source, qt_TexCoord0 - sampleStep * 11.455198604302))
            * 0.035643950166;
    color += (texture(source, qt_TexCoord0 + sampleStep * 13.447460291920)
              + texture(source, qt_TexCoord0 - sampleStep * 13.447460291920))
            * 0.024154614723;
    color += (texture(source, qt_TexCoord0 + sampleStep * 15.439747346367)
              + texture(source, qt_TexCoord0 - sampleStep * 15.439747346367))
            * 0.015380691117;
    color += (texture(source, qt_TexCoord0 + sampleStep * 17.432063389212)
              + texture(source, qt_TexCoord0 - sampleStep * 17.432063389212))
            * 0.009202644943;
    color += (texture(source, qt_TexCoord0 + sampleStep * 19.424411986670)
              + texture(source, qt_TexCoord0 - sampleStep * 19.424411986670))
            * 0.005173806053;
    color += (texture(source, qt_TexCoord0 + sampleStep * 21.416796643162)
              + texture(source, qt_TexCoord0 - sampleStep * 21.416796643162))
            * 0.002733177391;
    color += (texture(source, qt_TexCoord0 + sampleStep * 23.409220795132)
              + texture(source, qt_TexCoord0 - sampleStep * 23.409220795132))
            * 0.001356704228;
    fragColor = color * qt_Opacity;
}
