#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sampleStep;
    vec4 region;
    vec2 targetSize;
    vec4 pageColor;
    vec4 surfaceColor;
    float paperAlpha;
};
layout(binding = 1) uniform sampler2D source;
vec4 sampleBackdrop(vec2 uv)
{
    vec4 scene = texture(source, uv);
    vec2 paperUv = (region.xy + uv * targetSize) / region.zw;
    float t =
            clamp(length((paperUv - vec2(0.51, 0.44)) / (sqrt(2.0) * vec2(0.51, 0.56))), 0.0, 1.0);
    return mix(scene, mix(pageColor, surfaceColor, t), paperAlpha);
}
// Frozen CSS blur(10px), normalized Gaussian sigma10, radius30; paired linear taps.
void main()
{
    vec4 color = sampleBackdrop(qt_TexCoord0) * 0.039985344576529;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 1.496250070310918)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 1.496250070310918))
            * 0.078979498544094;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 3.491250893119760)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 3.491250893119760))
            * 0.075137013919887;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 5.486253465097644)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 5.486253465097644))
            * 0.068685509955355;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 7.481258784121465)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 7.481258784121465))
            * 0.060332030124521;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 9.476267845873439)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 9.476267845873439))
            * 0.050921635031570;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 11.471281643048561)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 11.471281643048561))
            * 0.041297930791582;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 13.466301164567101)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 13.466301164567101))
            * 0.032182942594409;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 15.461327394793493)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 15.461327394793493))
            * 0.024098758038693;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 17.456361312762919)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 17.456361312762919))
            * 0.017339435777756;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 19.451403891416867)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 19.451403891416867))
            * 0.011987995753853;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 21.446456096848884)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 21.446456096848884))
            * 0.007963966758459;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 23.441518887561841)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 23.441518887561841))
            * 0.005083741370702;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 25.436593213737808)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 25.436593213737808))
            * 0.003118232640085;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 27.431680016521753)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 27.431680016521753))
            * 0.001837826583963;
    color += (sampleBackdrop(qt_TexCoord0 + sampleStep * 29.426780227320165)
              + sampleBackdrop(qt_TexCoord0 - sampleStep * 29.426780227320165))
            * 0.001040809826805;
    fragColor = color * qt_Opacity;
}
