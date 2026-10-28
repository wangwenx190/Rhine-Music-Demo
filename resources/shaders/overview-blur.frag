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
// CSS blur(12px): sigma=12 logical pixels, Gaussian truncated at 3 sigma.
// Adjacent taps share a linear lookup: pairWeight=w[i]+w[i+1],
// pairOffset=i+w[i+1]/pairWeight. This preserves the discrete kernel.
// Normalized discrete Gaussian, paired for bilinear sampling. Each pair is (offset, weight).
// Keeping the original accumulation order preserves the authored separable kernel.
const float GaussianCenterWeight = 0.033323361743;
const vec2 GaussianPairs[18] =
        vec2[](vec2(1.497395856881, 0.066071592777), vec2(3.493923910234, 0.063820659262),
               vec2(5.490452549525, 0.059960466207), vec2(7.486982109308, 0.054793105596),
               vec2(9.483512923783, 0.048701689303), vec2(11.480045326666, 0.042103608678),
               vec2(13.476579651064, 0.035403955832), vec2(15.473116229340, 0.028956190306),
               vec2(17.469655392993, 0.023034999218), vec2(19.466197472529, 0.017823463970),
               vec2(21.462742797336, 0.013413840430), vec2(23.459291695556, 0.009819090229),
               vec2(25.455844493967, 0.006991114826), vec2(27.452401517856, 0.004841485825),
               vec2(29.448963090898, 0.003261128504), vec2(31.445529535040, 0.002136555360),
               vec2(33.442101170375, 0.001361499066), vec2(35.438678315034, 0.000843873740));
#define sampleBackdrop(uv) texture(source, uv)
#include "gaussian-blur.glsl"
