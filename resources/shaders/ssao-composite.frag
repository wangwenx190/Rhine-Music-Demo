void MAIN()
{
    vec4 color = texture(INPUT, INPUT_UV);
    vec4 geometry = texture(geometryMap, INPUT_UV);
    if (channel == 1)
        FRAGCOLOR = vec4(vec3(texture(rawMap, INPUT_UV).r), 1);
    else if (channel == 2)
        FRAGCOLOR = vec4(vec3(texture(blurMap, INPUT_UV).r), 1);
    else if (channel == 3) {
        float depth = dot(texture(depthMap, INPUT_UV).rgb, vec3(65536.0, 256.0, 1.0) / 65793.0);
        float viewZ = -mix(cameraNear, cameraFar, depth);
        float hardwareDepth =
                (cameraFar + cameraNear * cameraFar / viewZ) / (cameraFar - cameraNear);
        FRAGCOLOR = vec4(vec3(hardwareDepth), 1);
    } else if (channel == 4)
        FRAGCOLOR = vec4(geometry.rgb, 1);
    else if (channel == 5)
        FRAGCOLOR = vec4(color.rgb, 1);
    else
        FRAGCOLOR = vec4(color.rgb * mix(1.0, texture(blurMap, INPUT_UV).r, aoStrength), color.a);
}
