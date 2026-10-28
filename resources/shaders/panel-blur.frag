#version 440
#extension GL_GOOGLE_include_directive : require
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
// Normalized discrete Gaussian, paired for bilinear sampling. Each pair is (offset, weight).
// Keeping the original accumulation order preserves the authored separable kernel.
const float GaussianCenterWeight = 0.049976753601;
const vec2 GaussianPairs[12] =
        vec2[](vec2(1.494140893206, 0.098026961961), vec2(3.486331531380, 0.090687786847),
               vec2(5.478528837539, 0.078834188951), vec2(7.470736606633, 0.064393618225),
               vec2(9.462958613244, 0.049423478595), vec2(11.455198604302, 0.035643950166),
               vec2(13.447460291920, 0.024154614723), vec2(15.439747346367, 0.015380691117),
               vec2(17.432063389212, 0.009202644943), vec2(19.424411986670, 0.005173806053),
               vec2(21.416796643162, 0.002733177391), vec2(23.409220795132, 0.001356704228));
#define sampleBackdrop(uv) texture(source, uv)
#include "gaussian-blur.glsl"
