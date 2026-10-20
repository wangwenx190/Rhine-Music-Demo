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
layout(location = 9) in vec4 shadowCoord;
layout(location = 6) flat in vec4 entityLight0;
layout(location = 7) flat in vec4 entityLight1;
layout(location = 8) flat in vec4 entityCoverUv;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 depthOut;
#include "paper-lighting.glsl"
#include "fog.glsl"
vec3 srgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((c + .055) / 1.055, vec3(2.4)), step(vec3(.04045), c));
}
#include "shadow-filter.glsl"
#include "floor-lighting.glsl"
// Tile-local coordinates at every level prevent cross-cover filtering. The mip
// chain is reduced per cover in linear light, then uploaded by dirty region.
vec4 coverLevel(vec2 uv, float level)
{
    float divisor = exp2(level);
    vec2 atlas = vec2(textureSize(coverMap, 0));
    vec2 origin = floor(entityCoverUv.xy * atlas / divisor + .5);
    vec2 size = max(vec2(1.0), floor(entityCoverUv.zw * atlas / divisor));
    vec2 pixel = origin + clamp(uv * size, vec2(.5), size - .5);
    return textureLod(coverMap, pixel / vec2(textureSize(coverMap, int(level))), level);
}
vec4 coverTrilinear(vec2 uv, float lod)
{
    float low = floor(lod);
    return mix(coverLevel(uv, low), coverLevel(uv, ceil(lod)), fract(lod));
}
vec4 filteredCover(vec2 uv, vec2 dx, vec2 dy)
{
    vec2 size = entityCoverUv.zw * vec2(textureSize(coverMap, 0));
    vec2 x = dx * size, y = dy * size;
    float a = dot(x, x), b = dot(x, y), c = dot(y, y);
    float root = sqrt(max(0.0, (a - c) * (a - c) + 4.0 * b * b));
    float major = sqrt(max(0.0, .5 * (a + c + root)));
    float minor = sqrt(max(0.0, .5 * (a + c - root)));
    // A bounded quadrature covers the long footprint. For more extreme grazing
    // angles, the mip footprint expands so taps remain overlapping, without aliasing.
    float footprint = max(1.0, max(minor, major / 32.0));
    int taps = clamp(int(ceil(major / footprint)), 1, 32);
    vec2 axis = abs(b) > .000001 ? normalize(vec2(b, .5 * (c - a + root)))
                                 : (a >= c ? vec2(1, 0) : vec2(0, 1));
    vec2 stepUv = (dx * axis.x + dy * axis.y) / float(taps);
    float maxLod = floor(log2(max(size.x, size.y)));
    float lod = clamp(log2(footprint), 0.0, maxLod);
    vec4 sum = vec4(0);
    for (int tap = 0; tap < taps; ++tap)
        sum += coverTrilinear(uv + (float(tap) + .5 - .5 * float(taps)) * stepUv, lod);
    return sum / float(taps);
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
    if (f.draw.x > 5.5) {
        vec3 outgoing = floorRadiance(normalize(worldNormal), normalize(f.camera.xyz - world));
        color = vec4(sceneFog(outgoing, viewDepth), 1);
        return;
    }
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
    vec2 coverDx = dFdx(uv), coverDy = dFdy(uv);
    if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1))))
        discard;
    vec2 tileUv = entityCoverUv.xy + clamp(uv, .5 / size, 1.0 - .5 / size) * entityCoverUv.zw;
    vec4 ink = f.fog.w > .5 ? filteredCover(uv, coverDx, coverDy) : texture(coverMap, tileUv);
    if (ink.a < .025)
        discard;
    // Keep transparent helper lanes finite as well as the surviving fragments.
    ink.rgb /= max(ink.a, .025);
    vec3 source = srgbToLinear(ink.rgb), n = normalize(worldNormal);
    if (f.areaTarget.w < .5) {
        color = vec4(sceneFog(source, viewDepth), 1);
        return;
    }
    vec3 irradiance =
            musicIrradiance(world, n) + textureCubeUV(environmentMap, n, 1.0).rgb * f.settings.w;
    float keyCosine = max(0.0, dot(n, normalize(vec3(-6, 14, -5))));
    if (keyCosine > 0.0)
        irradiance -= f.keyColor.rgb * f.lightIntensities.x * keyCosine * (1.0 - shadowFactor())
                / 3.141592653589793;
    vec3 outgoing =
            min(source * max(irradiance, vec3(0)), source) * musicPrintExposure(f.model[3].xyz);
    color = vec4(sceneFog(outgoing, viewDepth), 1);
}
