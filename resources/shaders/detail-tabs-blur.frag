#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sampleStep;
    float composite;
};
layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D overlay;
// Frozen CSS blur(18px), sigma18 truncated at 3 sigma, paired linear taps.
vec4 sampleBackdrop(vec2 uv)
{
    vec4 below = texture(source, uv);
    if (composite < 0.5) {
        return below;
    }
    vec4 above = texture(overlay, uv);
    return above + below * (1.0 - above.a);
}
// Normalized discrete Gaussian, paired for bilinear sampling. Each pair is (offset, weight).
// Keeping the original accumulation order preserves the authored separable kernel.
const float GaussianCenterWeight = 0.022218125931;
const vec2 GaussianPairs[27] =
        vec2[](vec2(1.498842594660, 0.044265264577), vec2(3.497299408978, 0.043587921528),
               vec2(5.495756274745, 0.042394716268), vec2(7.494213221358, 0.040728628159),
               vec2(9.492670278205, 0.038648292172), vec2(11.491127474666, 0.036224575845),
               vec2(13.489584840113, 0.033536581073), vec2(15.488042403901, 0.030667384459),
               vec2(17.486500195373, 0.027699833847), vec2(19.484958243855, 0.024712692434),
               vec2(21.483416578650, 0.021777370370), vec2(23.481875229042, 0.018955414372),
               vec2(25.480334224291, 0.016296847421), vec2(27.478793593631, 0.013839372109),
               vec2(29.477253366264, 0.011608380721), vec2(31.475713571367, 0.009617658850),
               vec2(33.474174238079, 0.007870631147), vec2(35.472635395507, 0.006361978799),
               vec2(37.471097072720, 0.005079457601), vec2(39.469559298747, 0.004005759991),
               vec2(41.468022102576, 0.003120290085), vec2(43.466485513152, 0.002400752984),
               vec2(45.464949559372, 0.001824494011), vec2(47.463414270086, 0.001369556215),
               vec2(49.461879674094, 0.001015452794), vec2(51.460345800144, 0.000743673091),
               vec2(53.458812676928, 0.000537956113));
void main()
{
    vec4 color = sampleBackdrop(qt_TexCoord0) * GaussianCenterWeight;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[0].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[0].x))
            * GaussianPairs[0].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[1].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[1].x))
            * GaussianPairs[1].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[2].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[2].x))
            * GaussianPairs[2].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[3].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[3].x))
            * GaussianPairs[3].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[4].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[4].x))
            * GaussianPairs[4].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[5].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[5].x))
            * GaussianPairs[5].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[6].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[6].x))
            * GaussianPairs[6].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[7].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[7].x))
            * GaussianPairs[7].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[8].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[8].x))
            * GaussianPairs[8].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[9].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[9].x))
            * GaussianPairs[9].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[10].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[10].x))
            * GaussianPairs[10].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[11].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[11].x))
            * GaussianPairs[11].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[12].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[12].x))
            * GaussianPairs[12].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[13].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[13].x))
            * GaussianPairs[13].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[14].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[14].x))
            * GaussianPairs[14].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[15].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[15].x))
            * GaussianPairs[15].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[16].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[16].x))
            * GaussianPairs[16].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[17].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[17].x))
            * GaussianPairs[17].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[18].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[18].x))
            * GaussianPairs[18].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[19].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[19].x))
            * GaussianPairs[19].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[20].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[20].x))
            * GaussianPairs[20].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[21].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[21].x))
            * GaussianPairs[21].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[22].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[22].x))
            * GaussianPairs[22].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[23].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[23].x))
            * GaussianPairs[23].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[24].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[24].x))
            * GaussianPairs[24].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[25].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[25].x))
            * GaussianPairs[25].y;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[26].x)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[26].x))
            * GaussianPairs[26].y;
    fragColor = color * qt_Opacity;
}
