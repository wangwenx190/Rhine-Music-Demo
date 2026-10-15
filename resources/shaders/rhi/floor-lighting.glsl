// MeshStandardMaterial transport from locked Three 0.183.2 (MIT).
// LTC/GGX helpers share the existing source-derived shell equations; no musicShell modulation.
layout(binding = 5) uniform sampler2D dfgMap;
layout(binding = 6) uniform sampler2D ltc1Map;
layout(binding = 7) uniform sampler2D ltc2Map;
float keyShadow;
float LTC_ClippedSphereFormFactor(const in vec3 f)
{

    // Real-Time Area Lighting: a Journey from Research to Production (p.102)
    // An approximation of the form factor of a horizon-clipped rectangle.

    float l = length(f);

    return max((l * l + f.z) / (l + 1.0), 0.0);
}

vec3 LTC_EdgeVectorFormFactor(const in vec3 v1, const in vec3 v2)
{

    float x = dot(v1, v2);

    float y = abs(x);

    // rational polynomial approximation to theta / sin( theta ) / 2PI
    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

vec3 LTC_Evaluate(const in vec3 N, const in vec3 V, const in vec3 P, const in mat3 mInv,
                  const in vec3 rectCoords[4])
{

    // bail if point is on back side of plane of light
    // assumes ccw winding order of light vertices
    vec3 v1 = rectCoords[1] - rectCoords[0];
    vec3 v2 = rectCoords[3] - rectCoords[0];
    vec3 lightNormal = cross(v1, v2);

    if (dot(lightNormal, P - rectCoords[0]) < 0.0)
        return vec3(0.0);

    // construct orthonormal basis around N
    vec3 T1, T2;
    vec3 tangent = V - N * dot(V, N);
    if (dot(tangent, tangent) == 0.0) {
        // At normal incidence the tangent orientation is arbitrary; choose a finite basis.
        vec3 axis = abs(N.x) <= abs(N.y) && abs(N.x) <= abs(N.z) ? vec3(1, 0, 0)
                : abs(N.y) <= abs(N.z)                           ? vec3(0, 1, 0)
                                                                 : vec3(0, 0, 1);
        tangent = cross(axis, N);
    }
    T1 = normalize(tangent);
    T2 = -cross(
            N,
            T1); // negated from paper; possibly due to a different handedness of world coordinate system

    // compute transform
    mat3 mat = mInv * transpose(mat3(T1, T2, N));

    // transform rect
    vec3 coords[4];
    coords[0] = mat * (rectCoords[0] - P);
    coords[1] = mat * (rectCoords[1] - P);
    coords[2] = mat * (rectCoords[2] - P);
    coords[3] = mat * (rectCoords[3] - P);

    // project rect onto sphere
    coords[0] = normalize(coords[0]);
    coords[1] = normalize(coords[1]);
    coords[2] = normalize(coords[2]);
    coords[3] = normalize(coords[3]);

    // calculate vector form factor
    vec3 vectorFormFactor = vec3(0.0);
    vectorFormFactor += LTC_EdgeVectorFormFactor(coords[0], coords[1]);
    vectorFormFactor += LTC_EdgeVectorFormFactor(coords[1], coords[2]);
    vectorFormFactor += LTC_EdgeVectorFormFactor(coords[2], coords[3]);
    vectorFormFactor += LTC_EdgeVectorFormFactor(coords[3], coords[0]);

    // adjust for horizon clipping
    float result = LTC_ClippedSphereFormFactor(vectorFormFactor);

    return vec3(result);
}

void musicArea(vec3 n, vec3 v, vec3 position, float r, vec3 f0, out vec3 diffuse, out vec3 specular)
{
    vec3 z = normalize(f.areaPosition.xyz - f.areaTarget.xyz);
    vec3 x = normalize(cross(vec3(0, 1, 0), z)), y = cross(z, x);
    vec3 hw = x * 19.5, hh = y * (.09 * f.rowSettings.z);
    vec3 rect[4];
    rect[0] = f.areaPosition.xyz + hw - hh;
    rect[1] = f.areaPosition.xyz - hw - hh;
    rect[2] = f.areaPosition.xyz - hw + hh;
    rect[3] = f.areaPosition.xyz + hw + hh;
    vec2 uv = vec2(r, sqrt(1.0 - clamp(dot(n, v), 0.0, 1.0))) * (63.0 / 64.0) + .5 / 64.0;
    vec4 t1 = texture(ltc1Map, uv), t2 = texture(ltc2Map, uv);
    mat3 mInv = mat3(vec3(t1.x, 0, t1.y), vec3(0, 1, 0), vec3(t1.z, 0, t1.w));
    vec3 energy = f.spotColor.xyz * f.areaPosition.w;
    diffuse = energy * LTC_Evaluate(n, v, position, mat3(1.0), rect);
    specular = energy * (f0 * t2.x + (1.0 - f0) * t2.y) * LTC_Evaluate(n, v, position, mInv, rect);
}

vec3 musicBRDF(vec3 n, vec3 v, vec3 l, float r, vec3 f0, bool multiscatter)
{
    float nl = clamp(dot(n, l), 0.0, 1.0), nv = clamp(dot(n, v), 0.0, 1.0);
    if (nl == 0.0)
        return vec3(0);
    vec3 h = normalize(l + v);
    float nh = clamp(dot(n, h), 0.0, 1.0), vh = clamp(dot(v, h), 0.0, 1.0);
    float a = r * r, aa = a * a;
    float gv = nl * sqrt(aa + (1.0 - aa) * nv * nv), gl = nv * sqrt(aa + (1.0 - aa) * nl * nl);
    float visibility = .5 / max(gv + gl, 1e-6), d = nh * nh * (aa - 1.0) + 1.0;
    float distribution = aa / (3.141592653589793 * d * d);
    vec3 fresnel = f0 + (1.0 - f0) * exp2((-5.55473 * vh - 6.98316) * vh);
    vec3 result = fresnel * visibility * distribution;
    if (multiscatter) {
        vec2 fv = texture(dfgMap, vec2(r, nv)).rg, fl = texture(dfgMap, vec2(r, nl)).rg;
        vec3 sv = f0 * fv.x + fv.y, sl = f0 * fl.x + fl.y, average = f0 + (1.0 - f0) * .047619;
        float compensation = (1.0 - fv.x - fv.y) * (1.0 - fl.x - fl.y);
        result += sv * sl * average / (1.0 - compensation * average + 1e-6) * compensation;
    }
    return result * nl;
}
vec3 musicSpecular(vec3 n, vec3 v, vec3 position, float r, vec3 f0, bool multiscatter)
{
    return f.keyColor.xyz * f.lightIntensities.xyz.x * keyShadow
            * musicBRDF(n, v, normalize(vec3(-6, 14, -5)), r, f0, multiscatter)
            + vec3(f.lightIntensities.xyz.z)
            * musicBRDF(n, v, normalize(vec3(7, 8, -10)), r, f0, multiscatter)
            + musicSpot(position)
            * musicBRDF(n, v, normalize(f.spotPosition.xyz - position), r, f0, multiscatter);
}

vec3 floorRadiance(vec3 n, vec3 v)
{
    keyShadow = shadowFactor();
    float r = f.material.y; // Constant planar normal: source geometry roughness is zero.
    vec3 f0 = vec3(.04); // Standard dielectric, metalness=0, no coat/transmission/emission.
    float nv = clamp(dot(n, v), 0.0, 1.0);
    vec3 direct = musicIrradiance(world, n);
    direct -= f.keyColor.rgb * f.lightIntensities.x * max(0.0, dot(n, normalize(vec3(-6, 14, -5))))
            * (1.0 - keyShadow) / 3.141592653589793;
    vec3 areaDiffuse = vec3(0), areaSpecular = vec3(0);
    if (f.areaPosition.w > 0.0)
        musicArea(n, v, world, r, f0, areaDiffuse, areaSpecular);
    vec3 irradiance = textureCubeUV(environmentMap, n, 1.0).rgb * f.settings.w;
    vec3 reflected = normalize(mix(reflect(-v, n), n, pow(r, 4.0)));
    vec3 radiance = textureCubeUV(environmentMap, reflected, r).rgb * f.settings.w;
    vec2 fab = texture(dfgMap, vec2(r, nv)).rg;
    vec3 singleScatter = f0 * fab.x + fab.y;
    float ems = 1.0 - fab.x - fab.y;
    vec3 average = f0 + (1.0 - f0) * .047619;
    vec3 multiScatter = singleScatter * average / (1.0 - ems * average) * ems;
    vec3 diffuse =
            f.tint.rgb * (direct + areaDiffuse + (1.0 - singleScatter - multiScatter) * irradiance);
    vec3 specular = musicSpecular(n, v, world, r, f0, true) + areaSpecular
            + radiance * singleScatter + multiScatter * irradiance;
    return diffuse + specular;
}
