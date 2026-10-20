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
// Normalized discrete Gaussian, paired for bilinear sampling. Each pair is (offset, weight).
// Keeping the original accumulation order preserves the authored separable kernel.
const float GaussianCenterWeight = 0.039985344576529;
const vec2 GaussianPairs[15] = vec2[](
        vec2(1.496250070310918, 0.078979498544094), vec2(3.491250893119760, 0.075137013919887),
        vec2(5.486253465097644, 0.068685509955355), vec2(7.481258784121465, 0.060332030124521),
        vec2(9.476267845873439, 0.050921635031570), vec2(11.471281643048561, 0.041297930791582),
        vec2(13.466301164567101, 0.032182942594409), vec2(15.461327394793493, 0.024098758038693),
        vec2(17.456361312762919, 0.017339435777756), vec2(19.451403891416867, 0.011987995753853),
        vec2(21.446456096848884, 0.007963966758459), vec2(23.441518887561841, 0.005083741370702),
        vec2(25.436593213737808, 0.003118232640085), vec2(27.431680016521753, 0.001837826583963),
        vec2(29.426780227320165, 0.001040809826805));
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
    fragColor = color * qt_Opacity;
}
