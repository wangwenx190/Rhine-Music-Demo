#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(binding = 1) uniform sampler2D coverMap;
layout(binding = 2) uniform sampler2D environmentMap;
layout(binding = 3) uniform sampler2D shadowMap;
layout(location = 0) in vec2 paperUv;
layout(location = 1) in vec3 world;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in float viewDepth;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 depthOut;
#include "paper-lighting.glsl"
vec3 srgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((c + .055) / 1.055, vec3(2.4)), step(vec3(.04045), c));
}
// Frozen Three .183.2: PCFSoftShadowMap is converted to PCFShadowMap.
// Implement its bilinear depth comparisons explicitly for a portable float color target.
float compareShadow(vec2 uv, float depth)
{
    vec2 p = uv * f.draw.z - .5, a = floor(p), w = fract(p);
    vec2 delta = vec2(1.0 / f.draw.z);
    vec2 q = (a + .5) * delta;
    float s00 = step(depth, texture(shadowMap, q).r);
    float s10 = step(depth, texture(shadowMap, q + vec2(delta.x, 0)).r);
    float s01 = step(depth, texture(shadowMap, q + vec2(0, delta.y)).r);
    float s11 = step(depth, texture(shadowMap, q + delta).r);
    return mix(mix(s00, s10, w.x), mix(s01, s11, w.x), w.y);
}
float shadowFactor()
{
    if (f.draw.y < .5)
        return 1.0;
    vec4 p = f.lightVP * vec4(world + normalize(worldNormal) * f.optics.z, 1);
    vec3 coord = p.xyz / p.w * .5 + .5;
    coord.z += f.optics.w;
    if (any(lessThan(coord.xy, vec2(0))) || any(greaterThan(coord.xy, vec2(1))) || coord.z > 1)
        return 1.0;
    if (f.clip.z < .5)
        coord.y = 1.0 - coord.y;
    // WebGL uses bottom-left fragment coordinates; preserve IGN through backend flips.
    vec2 pixel = gl_FragCoord.xy;
    // framebuffer height is carried in lightIntensities.w, independent of cover dimensions.
    if (f.clip.z < .5)
        pixel.y = f.lightIntensities.w - gl_FragCoord.y;
    float phi = fract(52.9829189 * fract(dot(pixel, vec2(.06711056, .00583715)))) * 6.28318530718;
    float sum = 0;
    for (int i = 0; i < 5; ++i) {
        float radius = sqrt((float(i) + .5) / 5.0);
        float theta = float(i) * 2.399963229728653 + phi;
        vec2 offset = vec2(cos(theta), sin(theta)) * radius * f.draw.w / f.draw.z;
        if (f.clip.z < .5)
            offset.y = -offset.y;
        sum += compareShadow(coord.xy + offset, coord.z);
    }
    return mix(1.0, sum * .2, f.optics.y);
}
vec3 packDepth(float depth)
{
    float code = min(16777215.0, floor(clamp(depth, 0.0, 1.0) * 16777215.0 + .5));
    return vec3(floor(code / 65536.0), mod(floor(code / 256.0), 256.0), mod(code, 256.0)) / 255.0;
}
void main()
{
    if (f.draw.x < -.5) {
        color = vec4(f.background.rgb, 1);
        normalOut = color;
        depthOut = vec4(1);
        return;
    }
    normalOut = vec4(normalize(viewNormal) * .5 + .5, 1);
    depthOut = vec4(packDepth((viewDepth - f.clip.x) / (f.clip.y - f.clip.x)), 1);
    if (f.draw.x > 1.5) {
        color = vec4(vec3(shadowFactor()), 1);
        return;
    }
    if (f.draw.x > .5) {
        color = normalOut;
        return;
    }
    vec2 size = vec2(textureSize(coverMap, 0));
    vec2 extent = (1.0 - 2.0 / 64.0) * size / max(size.x, size.y);
    vec2 uv = (paperUv - .5) / extent + .5;
    if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1))))
        discard;
    vec4 ink = texture(coverMap, clamp(uv, .5 / size, 1.0 - .5 / size));
    if (ink.a < .025)
        discard;
    vec3 source = srgbToLinear(ink.rgb), n = normalize(worldNormal);
    vec3 irradiance =
            musicIrradiance(world, n) + textureCubeUV(environmentMap, n, 1.0).rgb * f.settings.w;
    irradiance -= f.keyColor.rgb * f.lightIntensities.x
            * max(0.0, dot(n, normalize(vec3(-6, 14, -5)))) * (1.0 - shadowFactor())
            / 3.141592653589793;
    color = vec4(
            min(source * max(irradiance, vec3(0)), source) * musicPrintExposure(f.model[3].xyz), 1);
}
