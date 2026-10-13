#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Post
{
    vec4 size;
    vec4 clip;
    vec4 ao;
    vec4 dof;
    vec4 options;
    vec4 kernel[64];
}
f;
layout(binding = 2) uniform sampler2D depthMap;
layout(binding = 3) uniform sampler2D geometryMap;
layout(binding = 4) uniform sampler2D samplesMap;
layout(binding = 5) uniform sampler2D noiseMap;
layout(binding = 6) uniform sampler2D auxiliaryMap;
layout(binding = 7) uniform sampler2D areaMap;
layout(binding = 8) uniform sampler2D searchMap;
#define INPUT colorMap
#define INPUT_UV (f.clip.z > 0.0 ? uv : vec2(uv.x, 1.0 - uv.y))
#define INPUT_SIZE f.size.xy
#define FRAMEBUFFER_Y_UP f.clip.z
#define cameraNear f.clip.x
#define cameraFar f.clip.y
#define projectionPlane vec3(f.size.zw, 0)
#define kernelSize 64
#define kernelRadius f.ao.y
#define minDistance f.ao.z
#define maxDistance f.ao.w
#define focus f.dof.x
#define aperture f.dof.y
#define maxblur f.dof.z
#define aspect f.dof.w
#define weightMap auxiliaryMap
#define FRAGCOLOR color
// Three SSAOShader, MIT: hemisphere samples, tangent basis, orthographic depth delta.
// Source noise is a scalar replicated into xyz (not a replacement RGB noise algorithm).
vec3 viewPosition(vec2 uv, float depth)
{
    vec2 ndc = uv * 2.0 - 1.0;
    ndc.y *= FRAMEBUFFER_Y_UP;
    // Prepass stores linear distance as RGB24 through the Quick texture provider.
    float z = -mix(cameraNear, cameraFar, depth);
    return vec3(ndc * projectionPlane.xy * (-z), z);
}
float linearDepth(vec2 uv)
{
    return dot(texture(depthMap, uv).rgb, vec3(65536.0, 256.0, 1.0) / 65793.0);
}
void main()
{
    vec2 uv = INPUT_UV;
    vec4 geometry = texture(geometryMap, uv);
    float depth = linearDepth(uv);
    // RGB24 reconstruction can round white to 1-ulp on some backends.
    if (kernelSize == 0 || depth >= 1.0 - 1e-6) {
        FRAGCOLOR = vec4(1.0);
        return;
    }
    vec3 position = viewPosition(uv, depth);
    vec3 normal = normalize(geometry.rgb * 2.0 - 1.0);
    vec2 sourceUv = uv;
    if (FRAMEBUFFER_Y_UP < 0.0)
        sourceUv.y = 1.0 - sourceUv.y;
    vec3 random = vec3(texture(noiseMap, sourceUv * vec2(textureSize(geometryMap, 0)) / 4.0).r);
    vec3 tangent = normalize(random - normal * dot(random, normal));
    mat3 basis = mat3(tangent, cross(normal, tangent), normal) * kernelRadius;
    float occlusion = 0.0;
    {

        vec3 samplePoint = position + basis * f.kernel[0].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[1].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[2].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[3].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[4].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[5].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[6].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[7].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[8].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[9].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[10].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[11].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[12].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[13].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[14].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[15].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[16].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[17].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[18].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[19].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[20].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[21].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[22].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[23].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[24].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[25].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[26].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[27].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[28].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[29].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[30].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[31].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[32].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[33].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[34].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[35].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[36].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[37].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[38].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[39].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[40].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[41].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[42].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[43].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[44].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[45].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[46].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[47].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[48].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[49].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[50].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[51].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[52].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[53].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[54].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[55].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[56].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[57].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[58].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[59].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[60].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[61].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[62].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    {

        vec3 samplePoint = position + basis * f.kernel[63].xyz;
        vec2 sampleUv = samplePoint.xy / (-samplePoint.z * projectionPlane.xy) * .5 + .5;
        if (FRAMEBUFFER_Y_UP < 0.0)
            sampleUv.y = 1.0 - sampleUv.y;
        float realDepth = linearDepth(clamp(sampleUv, vec2(0), vec2(1)));
        float sampleDepth = (-samplePoint.z - cameraNear) / (cameraFar - cameraNear);
        float delta = sampleDepth - realDepth;
        if (delta > minDistance && delta < maxDistance)
            occlusion += 1.0;
    }
    FRAGCOLOR = vec4(vec3(1.0 - clamp(occlusion / float(kernelSize), 0.0, 1.0)), 1.0);
}
