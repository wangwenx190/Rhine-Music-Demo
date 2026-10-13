#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(binding = 1) uniform sampler2D coverMap;
layout(binding = 2) uniform sampler2D environmentMap;
layout(binding = 3) uniform sampler2DShadow shadowMap;
layout(location = 0) centroid in vec2 paperUv;
layout(location = 1) in vec3 world;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in float viewDepth;
layout(location = 5) in vec3 localPosition;
layout(location = 6) flat in vec4 entityLight0;
layout(location = 7) flat in vec4 entityLight1;
layout(location = 8) flat in vec4 entityCoverUv;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 depthOut;
#include "paper-lighting.glsl"
vec3 srgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((c + .055) / 1.055, vec3(2.4)), step(vec3(.04045), c));
}
#include "shadow-filter.glsl"
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
    vec2 size = f.paperSize.xy;
    if (entityCoverUv.z <= 0.0 || entityCoverUv.w <= 0.0)
        discard;
    vec2 extent = f.paperSize.z > .5 ? vec2(1.0) : (1.0 - 2.0 / 64.0) * size / max(size.x, size.y);
    vec2 uv = (paperUv - .5) / extent + .5;
    if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1))))
        discard;
    vec2 tileUv = entityCoverUv.xy + clamp(uv, .5 / size, 1.0 - .5 / size) * entityCoverUv.zw;
    vec4 ink = texture(coverMap, tileUv);
    if (ink.a < .025)
        discard;
    ink.rgb /= ink.a;
    vec3 source = srgbToLinear(ink.rgb), n = normalize(worldNormal);
    if (f.areaTarget.w < .5) {
        color = vec4(source, 1);
        return;
    }
    vec3 irradiance =
            musicIrradiance(world, n) + textureCubeUV(environmentMap, n, 1.0).rgb * f.settings.w;
    float keyCosine = max(0.0, dot(n, normalize(vec3(-6, 14, -5))));
    if (keyCosine > 0.0)
        irradiance -= f.keyColor.rgb * f.lightIntensities.x * keyCosine * (1.0 - shadowFactor())
                / 3.141592653589793;
    color = vec4(
            min(source * max(irradiance, vec3(0)), source) * musicPrintExposure(f.model[3].xyz), 1);
}
