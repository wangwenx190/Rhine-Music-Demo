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
void main()
{
    vec4 color = sampleBackdrop(qt_TexCoord0) * 0.022218125931;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 1.498842594660)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 1.498842594660))
            * 0.044265264577;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 3.497299408978)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 3.497299408978))
            * 0.043587921528;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 5.495756274745)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 5.495756274745))
            * 0.042394716268;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 7.494213221358)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 7.494213221358))
            * 0.040728628159;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 9.492670278205)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 9.492670278205))
            * 0.038648292172;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 11.491127474666)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 11.491127474666))
            * 0.036224575845;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 13.489584840113)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 13.489584840113))
            * 0.033536581073;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 15.488042403901)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 15.488042403901))
            * 0.030667384459;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 17.486500195373)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 17.486500195373))
            * 0.027699833847;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 19.484958243855)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 19.484958243855))
            * 0.024712692434;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 21.483416578650)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 21.483416578650))
            * 0.021777370370;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 23.481875229042)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 23.481875229042))
            * 0.018955414372;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 25.480334224291)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 25.480334224291))
            * 0.016296847421;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 27.478793593631)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 27.478793593631))
            * 0.013839372109;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 29.477253366264)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 29.477253366264))
            * 0.011608380721;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 31.475713571367)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 31.475713571367))
            * 0.009617658850;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 33.474174238079)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 33.474174238079))
            * 0.007870631147;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 35.472635395507)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 35.472635395507))
            * 0.006361978799;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 37.471097072720)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 37.471097072720))
            * 0.005079457601;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 39.469559298747)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 39.469559298747))
            * 0.004005759991;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 41.468022102576)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 41.468022102576))
            * 0.003120290085;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 43.466485513152)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 43.466485513152))
            * 0.002400752984;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 45.464949559372)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 45.464949559372))
            * 0.001824494011;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 47.463414270086)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 47.463414270086))
            * 0.001369556215;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 49.461879674094)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 49.461879674094))
            * 0.001015452794;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 51.460345800144)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 51.460345800144))
            * 0.000743673091;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 53.458812676928)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 53.458812676928))
            * 0.000537956113;
    fragColor = color * qt_Opacity;
}
