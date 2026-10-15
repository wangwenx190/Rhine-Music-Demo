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
// CSS blur(12px): sigma=12 logical pixels, Gaussian truncated at 3 sigma.
// Adjacent taps share a linear lookup: pairWeight=w[i]+w[i+1],
// pairOffset=i+w[i+1]/pairWeight. This preserves the discrete kernel.
void main()
{
    vec4 color = texture(source, qt_TexCoord0) * 0.033323361743;
    color += (texture(source, qt_TexCoord0 + sampleStep * 1.497395856881)
              + texture(source, qt_TexCoord0 - sampleStep * 1.497395856881))
            * 0.066071592777;
    color += (texture(source, qt_TexCoord0 + sampleStep * 3.493923910234)
              + texture(source, qt_TexCoord0 - sampleStep * 3.493923910234))
            * 0.063820659262;
    color += (texture(source, qt_TexCoord0 + sampleStep * 5.490452549525)
              + texture(source, qt_TexCoord0 - sampleStep * 5.490452549525))
            * 0.059960466207;
    color += (texture(source, qt_TexCoord0 + sampleStep * 7.486982109308)
              + texture(source, qt_TexCoord0 - sampleStep * 7.486982109308))
            * 0.054793105596;
    color += (texture(source, qt_TexCoord0 + sampleStep * 9.483512923783)
              + texture(source, qt_TexCoord0 - sampleStep * 9.483512923783))
            * 0.048701689303;
    color += (texture(source, qt_TexCoord0 + sampleStep * 11.480045326666)
              + texture(source, qt_TexCoord0 - sampleStep * 11.480045326666))
            * 0.042103608678;
    color += (texture(source, qt_TexCoord0 + sampleStep * 13.476579651064)
              + texture(source, qt_TexCoord0 - sampleStep * 13.476579651064))
            * 0.035403955832;
    color += (texture(source, qt_TexCoord0 + sampleStep * 15.473116229340)
              + texture(source, qt_TexCoord0 - sampleStep * 15.473116229340))
            * 0.028956190306;
    color += (texture(source, qt_TexCoord0 + sampleStep * 17.469655392993)
              + texture(source, qt_TexCoord0 - sampleStep * 17.469655392993))
            * 0.023034999218;
    color += (texture(source, qt_TexCoord0 + sampleStep * 19.466197472529)
              + texture(source, qt_TexCoord0 - sampleStep * 19.466197472529))
            * 0.017823463970;
    color += (texture(source, qt_TexCoord0 + sampleStep * 21.462742797336)
              + texture(source, qt_TexCoord0 - sampleStep * 21.462742797336))
            * 0.013413840430;
    color += (texture(source, qt_TexCoord0 + sampleStep * 23.459291695556)
              + texture(source, qt_TexCoord0 - sampleStep * 23.459291695556))
            * 0.009819090229;
    color += (texture(source, qt_TexCoord0 + sampleStep * 25.455844493967)
              + texture(source, qt_TexCoord0 - sampleStep * 25.455844493967))
            * 0.006991114826;
    color += (texture(source, qt_TexCoord0 + sampleStep * 27.452401517856)
              + texture(source, qt_TexCoord0 - sampleStep * 27.452401517856))
            * 0.004841485825;
    color += (texture(source, qt_TexCoord0 + sampleStep * 29.448963090898)
              + texture(source, qt_TexCoord0 - sampleStep * 29.448963090898))
            * 0.003261128504;
    color += (texture(source, qt_TexCoord0 + sampleStep * 31.445529535040)
              + texture(source, qt_TexCoord0 - sampleStep * 31.445529535040))
            * 0.002136555360;
    color += (texture(source, qt_TexCoord0 + sampleStep * 33.442101170375)
              + texture(source, qt_TexCoord0 - sampleStep * 33.442101170375))
            * 0.001361499066;
    color += (texture(source, qt_TexCoord0 + sampleStep * 35.438678315034)
              + texture(source, qt_TexCoord0 - sampleStep * 35.438678315034))
            * 0.000843873740;
    fragColor = color * qt_Opacity;
}
