#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 0, std140) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    float sourceHeight;
    float viewportHeight;
    float fadeHeight;
    float blurAmount;
    float textureHeight;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    float y = qt_TexCoord0.y * sourceHeight;
    float fade = max(fadeHeight, 0.000001);
    float alpha = clamp(min(y, viewportHeight - y) / fade, 0.0, 1.0);
    vec4 sharp = texture(source, qt_TexCoord0);
    vec4 ink = sharp;
    if (blurAmount > 0.0) {
        float pixel = 1.0 / max(textureHeight, 1.0);
        float sigma = 29.0 * 0.035 * 0.2 / sourceHeight;
        vec4 smear = vec4(0.0);
        float total = 0.0;
        for (int i = -4; i <= 4; ++i) {
            float offset = float(i) * pixel;
            float weight = exp(-0.5 * offset * offset / (sigma * sigma));
            smear += texture(source, qt_TexCoord0 + vec2(0.0, offset)) * weight;
            total += weight;
        }
        ink = mix(sharp, smear / total, blurAmount);
    }
    fragColor = ink * alpha * qt_Opacity;
}
