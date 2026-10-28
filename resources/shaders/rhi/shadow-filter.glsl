// Frozen Three .183.2: PCFSoftShadowMap is converted to PCFShadowMap.
// Use the same hardware bilinear depth comparison as the source sampler2DShadow.
float compareShadow(vec2 uv, float depth)
{
    return texture(shadowMap, vec3(uv, depth));
}
float shadowFactor()
{
    if (f.draw.y < .5)
        return 1.0;
    vec3 coord = shadowCoord.xyz / shadowCoord.w * .5 + .5;
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
