// Independent opaque override reproduces SSAOPass MeshNormalMaterial, including glass.
void MAIN()
{
    float depth = texture(DEPTH_TEXTURE, INPUT_UV).r;
    float viewZ = cameraNear * cameraFar / ((cameraFar - cameraNear) * depth - cameraFar);
    float linearDepth = (-viewZ - cameraNear) / (cameraFar - cameraNear);
    // Qt's RGBA16F normal/roughness target stores signed world normals directly.
    vec3 normal = texture(NORMAL_ROUGHNESS_TEXTURE, INPUT_UV).xyz;
    normal = normalize(mat3(VIEW_MATRIX) * normal);
    // View3D's Quick texture provider can quantize the final color to RGBA8.
    // Carry linear depth as three exact bytes, with opaque alpha, independently
    // of normals. This keeps 24-bit distance precision through that public link.
    // Adding .5 to the largest odd integer rounds to 2^24 in float32.
    // Clamp after rounding as well, so the far plane remains RGB white.
    float code = min(16777215.0, floor(clamp(linearDepth, 0.0, 1.0) * 16777215.0 + .5));
    vec3 bytes = vec3(floor(code / 65536.0), mod(floor(code / 256.0), 256.0), mod(code, 256.0));
    FRAGCOLOR =
            vec4(packedDepth ? bytes / 255.0 : (depth >= 1.0 ? backgroundLinear : normal * .5 + .5),
                 1.0);
}
