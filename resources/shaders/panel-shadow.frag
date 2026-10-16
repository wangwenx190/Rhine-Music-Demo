#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 bounds;
    vec2 viewportSize;
};
// CSS box-shadow 0 24px 90px rgba(0,0,0,.13): rectangle convolved with sigma=45.
// Analytic Gaussian integral avoids a separate shadow capture and blur targets.
vec2 errorFunction(vec2 x)
{
    vec2 a = abs(x);
    vec2 t = 1.0 / (1.0 + 0.3275911 * a);
    vec2 p = (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t
              + 0.254829592)
            * t;
    return sign(x) * (1.0 - p * exp(-a * a));
}
void main()
{
    vec2 pixel = qt_TexCoord0 * viewportSize;
    vec2 lo = (pixel - bounds.xy) / (45.0 * sqrt(2.0));
    vec2 hi = (pixel - bounds.xy - bounds.zw) / (45.0 * sqrt(2.0));
    vec2 integral = 0.5 * (errorFunction(lo) - errorFunction(hi));
    fragColor = vec4(0.0, 0.0, 0.0, integral.x * integral.y * 0.13 * qt_Opacity);
}
