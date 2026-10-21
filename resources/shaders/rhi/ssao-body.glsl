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
float decodeDepth(vec3 code)
{
    return dot(code, vec3(65536.0, 256.0, 1.0) / 65793.0);
}
float linearDepth(vec2 uv)
{
    return decodeDepth(texture(depthMap, uv).rgb);
}
void main()
{
    vec2 uv = INPUT_UV;
    vec3 depthCode = texture(depthMap, uv).rgb;
    // The source depth==1 background sentinel is exact in the stored RGB24 domain.
    if (kernelSize == 0 || all(equal(depthCode, vec3(1)))) {
        FRAGCOLOR = vec4(1.0);
        return;
    }
    float depth = decodeDepth(depthCode);
    vec4 geometry = texture(geometryMap, uv);
    vec3 position = viewPosition(uv, depth);
    // SSAOShader unpacks the stored normal without renormalizing it.
    vec3 normal = geometry.rgb * 2.0 - 1.0;
    vec2 sourceUv = uv;
    if (FRAMEBUFFER_Y_UP < 0.0)
        sourceUv.y = 1.0 - sourceUv.y;
    vec3 random = vec3(texture(noiseMap, sourceUv * vec2(textureSize(geometryMap, 0)) / 4.0).r);
    vec3 tangent = normalize(random - normal * dot(random, normal));
    mat3 basis = mat3(tangent, cross(normal, tangent), normal) * kernelRadius;
    float occlusion = 0.0;
#if KERNEL_COUNT > 0
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
#endif
#if KERNEL_COUNT > 1
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
#endif
#if KERNEL_COUNT > 2
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
#endif
#if KERNEL_COUNT > 3
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
#endif
#if KERNEL_COUNT > 4
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
#endif
#if KERNEL_COUNT > 5
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
#endif
#if KERNEL_COUNT > 6
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
#endif
#if KERNEL_COUNT > 7
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
#endif
#if KERNEL_COUNT > 8
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
#endif
#if KERNEL_COUNT > 9
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
#endif
#if KERNEL_COUNT > 10
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
#endif
#if KERNEL_COUNT > 11
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
#endif
#if KERNEL_COUNT > 12
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
#endif
#if KERNEL_COUNT > 13
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
#endif
#if KERNEL_COUNT > 14
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
#endif
#if KERNEL_COUNT > 15
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
#endif
#if KERNEL_COUNT > 16
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
#endif
#if KERNEL_COUNT > 17
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
#endif
#if KERNEL_COUNT > 18
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
#endif
#if KERNEL_COUNT > 19
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
#endif
#if KERNEL_COUNT > 20
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
#endif
#if KERNEL_COUNT > 21
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
#endif
#if KERNEL_COUNT > 22
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
#endif
#if KERNEL_COUNT > 23
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
#endif
#if KERNEL_COUNT > 24
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
#endif
#if KERNEL_COUNT > 25
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
#endif
#if KERNEL_COUNT > 26
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
#endif
#if KERNEL_COUNT > 27
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
#endif
#if KERNEL_COUNT > 28
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
#endif
#if KERNEL_COUNT > 29
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
#endif
#if KERNEL_COUNT > 30
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
#endif
#if KERNEL_COUNT > 31
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
#endif
#if KERNEL_COUNT > 32
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
#endif
#if KERNEL_COUNT > 33
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
#endif
#if KERNEL_COUNT > 34
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
#endif
#if KERNEL_COUNT > 35
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
#endif
#if KERNEL_COUNT > 36
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
#endif
#if KERNEL_COUNT > 37
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
#endif
#if KERNEL_COUNT > 38
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
#endif
#if KERNEL_COUNT > 39
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
#endif
#if KERNEL_COUNT > 40
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
#endif
#if KERNEL_COUNT > 41
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
#endif
#if KERNEL_COUNT > 42
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
#endif
#if KERNEL_COUNT > 43
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
#endif
#if KERNEL_COUNT > 44
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
#endif
#if KERNEL_COUNT > 45
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
#endif
#if KERNEL_COUNT > 46
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
#endif
#if KERNEL_COUNT > 47
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
#endif
#if KERNEL_COUNT > 48
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
#endif
#if KERNEL_COUNT > 49
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
#endif
#if KERNEL_COUNT > 50
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
#endif
#if KERNEL_COUNT > 51
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
#endif
#if KERNEL_COUNT > 52
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
#endif
#if KERNEL_COUNT > 53
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
#endif
#if KERNEL_COUNT > 54
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
#endif
#if KERNEL_COUNT > 55
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
#endif
#if KERNEL_COUNT > 56
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
#endif
#if KERNEL_COUNT > 57
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
#endif
#if KERNEL_COUNT > 58
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
#endif
#if KERNEL_COUNT > 59
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
#endif
#if KERNEL_COUNT > 60
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
#endif
#if KERNEL_COUNT > 61
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
#endif
#if KERNEL_COUNT > 62
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
#endif
#if KERNEL_COUNT > 63
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
#endif
    FRAGCOLOR = vec4(vec3(1.0 - clamp(occlusion / float(kernelSize), 0.0, 1.0)), 1.0);
}
