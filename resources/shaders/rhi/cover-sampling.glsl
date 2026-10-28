// Stored texels are sRGB-encoded premultiplied RGBA8. Decode each texel to
// linear premultiplied color before any spatial, mip or footprint interpolation.
vec4 coverTexel(ivec2 pixel, int level)
{
    vec4 ink = texelFetch(coverMap, pixel, level);
    if (ink.a == 0.0)
        return vec4(0);
    const float SrgbLinearSlope = 12.92;
    const float SrgbOffset = .055;
    const float SrgbScale = 1.055;
    const float SrgbExponent = 2.4;
    const float SrgbDecodeKnee = .04045;
    vec3 encoded = ink.rgb / ink.a;
    vec3 linear = mix(encoded / SrgbLinearSlope,
                      pow((encoded + SrgbOffset) / SrgbScale, vec3(SrgbExponent)),
                      step(vec3(SrgbDecodeKnee), encoded));
    return vec4(linear * ink.a, ink.a);
}
vec4 coverLevel(vec2 uv, int level)
{
    float divisor = exp2(float(level));
    vec2 atlas = vec2(textureSize(coverMap, 0));
    ivec2 origin = ivec2(floor(entityCoverUv.xy * atlas / divisor + .5));
    ivec2 size = max(ivec2(1), ivec2(floor(entityCoverUv.zw * atlas / divisor)));
    if (all(equal(size, ivec2(1))))
        return coverTexel(origin, level);
    vec2 pixel = clamp(uv * vec2(size) - .5, vec2(0), vec2(size - 1));
    ivec2 first = ivec2(floor(pixel));
    ivec2 last = min(first + 1, size - 1);
    vec2 weight = fract(pixel);
    vec4 top = mix(coverTexel(origin + first, level),
                   coverTexel(origin + ivec2(last.x, first.y), level), weight.x);
    vec4 bottom = mix(coverTexel(origin + ivec2(first.x, last.y), level),
                      coverTexel(origin + last, level), weight.x);
    return mix(top, bottom, weight.y);
}
vec4 coverTrilinear(vec2 uv, float lod)
{
    int low = int(floor(lod));
    float weight = fract(lod);
    vec4 first = coverLevel(uv, low);
    if (weight == 0.0)
        return first;
    return mix(first, coverLevel(uv, low + 1), weight);
}
vec4 filteredCover(vec2 uv, vec2 dx, vec2 dy)
{
    vec2 size = entityCoverUv.zw * vec2(textureSize(coverMap, 0));
    vec2 x = dx * size, y = dy * size;
    float a = dot(x, x), b = dot(x, y), c = dot(y, y);
    float root = sqrt(max(0.0, (a - c) * (a - c) + 4.0 * b * b));
    float major = sqrt(max(0.0, .5 * (a + c + root)));
    float minor = sqrt(max(0.0, .5 * (a + c - root)));
    // Overlapping long-axis quadrature remains bounded at grazing angles.
    const float MaximumFootprintTaps = 32.0;
    float footprint = max(1.0, max(minor, major / MaximumFootprintTaps));
    int taps = clamp(int(ceil(major / footprint)), 1, int(MaximumFootprintTaps));
    const float AxisEpsilon = .000001;
    vec2 axis = abs(b) > AxisEpsilon ? normalize(vec2(b, .5 * (c - a + root)))
                                     : (a >= c ? vec2(1, 0) : vec2(0, 1));
    vec2 stepUv = (dx * axis.x + dy * axis.y) / float(taps);
    float maxLod = floor(log2(max(size.x, size.y)));
    float lod = clamp(log2(footprint), 0.0, maxLod);
    vec4 sum = vec4(0);
    for (int tap = 0; tap < taps; ++tap)
        sum += coverTrilinear(uv + (float(tap) + .5 - .5 * float(taps)) * stepUv, lod);
    return sum / float(taps);
}
