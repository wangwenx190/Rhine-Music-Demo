#version 440
#ifndef AREA_ENABLED
#  define AREA_ENABLED 1
#endif
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(binding = 2) uniform sampler2D environmentMap;
layout(binding = 3) uniform sampler2DShadow shadowMap;
layout(binding = 4) uniform sampler2D captureMap;
layout(binding = 5) uniform sampler2D dfgMap;
layout(binding = 6) uniform sampler2D ltc1Map;
layout(binding = 7) uniform sampler2D ltc2Map;
layout(location = 0) centroid in vec2 paperUv;
layout(location = 1) in vec3 world;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in float viewDepth;
layout(location = 5) in vec3 localPosition;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 depthOut;
#include "shadow-filter.glsl"
float keyShadow;
// BEGIN SHARED LIGHTING
// Shared field, diffuse and shell transport from frozen music-lighting.ts (MIT).
float musicRowField(vec3 origin)
{
    return f.originLight0.x;
}
float musicPrintExposure(vec3 origin)
{
    return f.originLight0.y;
}
vec3 musicSpot(vec3 position)
{
    vec3 delta = f.spotPosition.xyz - position;
    float distanceSquared = max(dot(delta, delta), .01);
    float angle = dot(normalize(delta), normalize(f.spotPosition.xyz - f.spotTarget.xyz));
    float attenuation = smoothstep(cos(.32), cos(.32 * (1.0 - .95)), angle) / distanceSquared;
    return f.spotColor.xyz * f.optics.x * attenuation;
}
vec3 musicIrradiance(vec3 position, vec3 normal)
{
    vec3 hemi =
            mix(f.groundColor.xyz, f.skyColor.xyz, .5 * normal.y + .5) * f.lightIntensities.xyz.y;
    vec3 key = f.keyColor.xyz * f.lightIntensities.xyz.x * keyShadow
            * max(0.0, dot(normal, normalize(vec3(-6, 14, -5))));
    vec3 fill = vec3(f.lightIntensities.xyz.z * max(0.0, dot(normal, normalize(vec3(7, 8, -10)))));
    vec3 spot =
            musicSpot(position) * max(0.0, dot(normal, normalize(f.spotPosition.xyz - position)));
    return (hemi + key + fill + spot) / 3.141592653589793;
}
vec3 musicShell(vec3 original, vec3 origin, vec3 local, vec3 normal, vec3 view, int shellSurface)
{
    float laneLight = f.originLight0.z;
    float guided = f.originLight0.w;
    float hot = f.originLight1.x;
    float edgeTransport = exp(-max(0.0, local.x + 2.225) * 1.7);
    float panelHeight = clamp((local.y - .175) / 3.35, 0.0, 1.0);
    float topRim = exp(-max(0.0, 3.525 - local.y) * 24.0);
    float grazing = 1.0 - clamp(abs(dot(normal, view)), 0.0, 1.0);
    bool spine = shellSurface == 1;
    vec3 selected = original * mix(.96, 1.04, guided);
    if (shellSurface < 2) {
        float edgeScatter = (spine ? .30 : .14) * edgeTransport + (spine ? .016 : .003);
        selected += f.scatter.xyz * f.scatter.w * guided * edgeScatter * mix(1.0, .62, panelHeight);
        float ribbon = topRim * (.22 + .78 * edgeTransport)
                + (spine ? .18 : .035) * edgeTransport * grazing;
        selected += f.scatter.xyz * f.scatter.w * laneLight * hot * ribbon * .8;
    }
    if (f.rowSettings.x < .5)
        return selected;
    float gain = f.rowSettings.x > 1.5 ? musicRowField(origin) * f.rowSettings.y : 0.0;
    float fillAmount = f.rowSettings.x > 1.5 && f.rowSettings.x < 2.5 ? f.settings.z : 0.0;
    vec3 shelf = original
            * mix(mix(.56, mix(.96, 1.04, guided), fillAmount), 1.08, clamp(gain, 0.0, 1.0));
    if (shellSurface < 2) {
        float panel = (spine ? .30 : .12) * edgeTransport + (spine ? .05 : .025);
        float rim = topRim * (.55 + .45 * edgeTransport)
                + (spine ? .24 : .07) * edgeTransport * grazing;
        float scatter = panel * mix(1.0, .78, panelHeight) + rim * 1.25;
        shelf += f.scatter.xyz * f.scatter.w * gain * scatter * (f.rowSettings.x > 2.5 ? .86 : 1.0);
    }
    return mix(shelf, selected, f.settings.y);
}

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
    T1 = normalize(V - N * dot(V, N));
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

    /*
	// alternate method of adjusting for horizon clipping (see reference)
	// refactoring required
	float len = length( vectorFormFactor );
	float z = vectorFormFactor.z / len;

	const float LUT_SIZE = 64.0;
	const float LUT_SCALE = ( LUT_SIZE - 1.0 ) / LUT_SIZE;
	const float LUT_BIAS = 0.5 / LUT_SIZE;

	// tabulated horizon-clipped sphere, apparently...
	vec2 uv = vec2( z * 0.5 + 0.5, len );
	uv = uv * LUT_SCALE + LUT_BIAS;

	float scale = texture2D( ltc_2, uv ).w;

	float result = len * scale;
*/

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
    vec3 h = normalize(l + v);
    float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 0.0), nh = max(dot(n, h), 0.0),
          vh = max(dot(v, h), 0.0);
    if (nl == 0.0)
        return vec3(0);
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

// END SHARED LIGHTING
// Three.js ShaderChunk derivation (MIT; resources/licenses/three-LICENSE).
#define CUBEUV_MAX_MIP 8.0
#define CUBEUV_TEXEL_WIDTH (1.0 / 768.0)
#define CUBEUV_TEXEL_HEIGHT (1.0 / 1024.0)

#define cubeUV_minMipLevel 4.0
#define cubeUV_minTileSize 16.0

// These shader functions convert between the UV coordinates of a single face of
// a cubemap, the 0-5 integer index of a cube face, and the direction vector for
// sampling a textureCube (not generally normalized ).

float getFace(vec3 direction)
{

    vec3 absDirection = abs(direction);

    float face = -1.0;

    if (absDirection.x > absDirection.z) {

        if (absDirection.x > absDirection.y)

            face = direction.x > 0.0 ? 0.0 : 3.0;

        else

            face = direction.y > 0.0 ? 1.0 : 4.0;

    } else {

        if (absDirection.z > absDirection.y)

            face = direction.z > 0.0 ? 2.0 : 5.0;

        else

            face = direction.y > 0.0 ? 1.0 : 4.0;
    }

    return face;
}

// RH coordinate system; PMREM face-indexing convention
vec2 getUV(vec3 direction, float face)
{

    vec2 uv;

    if (face == 0.0) {

        uv = vec2(direction.z, direction.y) / abs(direction.x); // pos x

    } else if (face == 1.0) {

        uv = vec2(-direction.x, -direction.z) / abs(direction.y); // pos y

    } else if (face == 2.0) {

        uv = vec2(-direction.x, direction.y) / abs(direction.z); // pos z

    } else if (face == 3.0) {

        uv = vec2(-direction.z, direction.y) / abs(direction.x); // neg x

    } else if (face == 4.0) {

        uv = vec2(-direction.x, direction.z) / abs(direction.y); // neg y

    } else {

        uv = vec2(direction.x, direction.y) / abs(direction.z); // neg z
    }

    return 0.5 * (uv + 1.0);
}

vec3 bilinearCubeUV(sampler2D envMap, vec3 direction, float mipInt)
{

    float face = getFace(direction);

    float filterInt = max(cubeUV_minMipLevel - mipInt, 0.0);

    mipInt = max(mipInt, cubeUV_minMipLevel);

    float faceSize = exp2(mipInt);

    vec2 uv = getUV(direction, face) * (faceSize - 2.0) + 1.0; // #25071

    if (face > 2.0) {

        uv.y += faceSize;

        face -= 3.0;
    }

    uv.x += face * faceSize;

    uv.x += filterInt * 3.0 * cubeUV_minTileSize;

    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - faceSize);

    uv.x *= CUBEUV_TEXEL_WIDTH;
    uv.y *= CUBEUV_TEXEL_HEIGHT;

#ifdef texture2DGradEXT

    return texture2DGradEXT(envMap, uv, vec2(0.0), vec2(0.0)).rgb; // disable anisotropic filtering

#else

    return textureLod(envMap, uv, 0.0).rgb;

#endif
}

// These defines must match with PMREMGenerator

#define cubeUV_r0 1.0
#define cubeUV_m0 -2.0
#define cubeUV_r1 0.8
#define cubeUV_m1 -1.0
#define cubeUV_r4 0.4
#define cubeUV_m4 2.0
#define cubeUV_r5 0.305
#define cubeUV_m5 3.0
#define cubeUV_r6 0.21
#define cubeUV_m6 4.0

float roughnessToMip(float pmremRoughness)
{

    float mip = 0.0;

    if (pmremRoughness >= cubeUV_r1) {

        mip = (cubeUV_r0 - pmremRoughness) * (cubeUV_m1 - cubeUV_m0) / (cubeUV_r0 - cubeUV_r1)
                + cubeUV_m0;

    } else if (pmremRoughness >= cubeUV_r4) {

        mip = (cubeUV_r1 - pmremRoughness) * (cubeUV_m4 - cubeUV_m1) / (cubeUV_r1 - cubeUV_r4)
                + cubeUV_m1;

    } else if (pmremRoughness >= cubeUV_r5) {

        mip = (cubeUV_r4 - pmremRoughness) * (cubeUV_m5 - cubeUV_m4) / (cubeUV_r4 - cubeUV_r5)
                + cubeUV_m4;

    } else if (pmremRoughness >= cubeUV_r6) {

        mip = (cubeUV_r5 - pmremRoughness) * (cubeUV_m6 - cubeUV_m5) / (cubeUV_r5 - cubeUV_r6)
                + cubeUV_m5;

    } else {

        mip = -2.0 * log2(1.16 * pmremRoughness); // 1.16 = 1.79^0.25
    }

    return mip;
}

vec4 textureCubeUV(sampler2D envMap, vec3 sampleDir, float pmremRoughness)
{

    float mip = clamp(roughnessToMip(pmremRoughness), cubeUV_m0, CUBEUV_MAX_MIP);

    float mipF = fract(mip);

    float mipInt = floor(mip);

    vec3 color0 = bilinearCubeUV(envMap, sampleDir, mipInt);

    if (mipF == 0.0) {

        return vec4(color0, 1.0);

    } else {

        vec3 color1 = bilinearCubeUV(envMap, sampleDir, mipInt + 1.0);

        return vec4(mix(color0, color1, mipF), 1.0);
    }
}

float w0(float a)
{

    return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a)
{

    return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a)
{

    return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a)
{

    return (1.0 / 6.0) * (a * a * a);
}

// g0 and g1 are the two amplitude functions
float g0(float a)
{

    return w0(a) + w1(a);
}

float g1(float a)
{

    return w2(a) + w3(a);
}

// h0 and h1 are the two offset functions
float h0(float a)
{

    return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a)
{

    return 1.0 + w3(a) / (w2(a) + w3(a));
}

vec4 bicubic(sampler2D tex, vec2 uv, vec4 texelSize, float lod)
{

    uv = uv * texelSize.zw + 0.5;

    vec2 iuv = floor(uv);
    vec2 fuv = fract(uv);

    float g0x = g0(fuv.x);
    float g1x = g1(fuv.x);
    float h0x = h0(fuv.x);
    float h1x = h1(fuv.x);
    float h0y = h0(fuv.y);
    float h1y = h1(fuv.y);

    vec2 p0 = (vec2(iuv.x + h0x, iuv.y + h0y) - 0.5) * texelSize.xy;
    vec2 p1 = (vec2(iuv.x + h1x, iuv.y + h0y) - 0.5) * texelSize.xy;
    vec2 p2 = (vec2(iuv.x + h0x, iuv.y + h1y) - 0.5) * texelSize.xy;
    vec2 p3 = (vec2(iuv.x + h1x, iuv.y + h1y) - 0.5) * texelSize.xy;

    return g0(fuv.y) * (g0x * textureLod(tex, p0, lod) + g1x * textureLod(tex, p1, lod))
            + g1(fuv.y) * (g0x * textureLod(tex, p2, lod) + g1x * textureLod(tex, p3, lod));
}

vec4 textureBicubic(sampler2D texSampler, vec2 uv, float lod)
{

    lod = clamp(
            lod, 0.0,
            floor(log2(float(max(textureSize(texSampler, 0).x, textureSize(texSampler, 0).y)))));
    vec2 fLodSize = vec2(textureSize(texSampler, int(lod)));
    vec2 cLodSize = vec2(textureSize(texSampler, int(ceil(lod))));
    vec2 fLodSizeInv = 1.0 / fLodSize;
    vec2 cLodSizeInv = 1.0 / cLodSize;
    vec4 fSample = bicubic(texSampler, uv, vec4(fLodSizeInv, fLodSize), floor(lod));
    vec4 cSample = bicubic(texSampler, uv, vec4(cLodSizeInv, cLodSize), ceil(lod));
    return mix(fSample, cSample, fract(lod));
}

vec3 RRTAndODTFit(vec3 v)
{

    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

// this implementation of ACES is modified to accommodate a brighter viewing environment.
// the scale factor of 1/0.6 is subjective. see discussion in #19621.

vec3 ACESFilmicToneMapping(vec3 color)
{

    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    const mat3 ACESInputMat =
            mat3(vec3(0.59719, 0.07600, 0.02840), // transposed from source
                 vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));

    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    const mat3 ACESOutputMat =
            mat3(vec3(1.60475, -0.10208, -0.00327), // transposed from source
                 vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));

    color *= f.camera.w / 0.6;

    color = ACESInputMat * color;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = ACESOutputMat * color;

    // Clamp to [0, 1]
    return clamp(color, 0.0, 1.0);
}

vec3 environmentBRDF(float dotNV, float r, vec3 f0)
{
    vec2 fab = texture(dfgMap, vec2(r, dotNV)).rg;
    return f0 * fab.x + fab.y;
}
void main()
{
    vec3 n = normalize(worldNormal);
    keyShadow = (f.areaTarget.w > .5) && dot(n, normalize(vec3(-6, 14, -5))) > 0.0 ? shadowFactor()
                                                                                   : 1.0;
    vec3 v = normalize(f.camera.xyz - world);
    float nv = max(0.0, dot(n, v));
    vec3 tint = f.tint.rgb;
    vec3 attenuation = f.attenuation.rgb;
    vec3 f0 = vec3(pow((1.46 - 1.0) / (1.46 + 1.0), 2.0));
    vec3 dxy = max(abs(dFdx(n)), abs(dFdy(n)));
    float geometryRoughness = max(dxy.x, max(dxy.y, dxy.z));
    float r = min(1.0, max(f.material.y, 0.0525) + geometryRoughness);
    float cr = min(1.0, max(f.coat.y, 0.0525) + geometryRoughness);
    vec3 reflectDir = normalize(mix(reflect(-v, n), n, pow(r, 4.0)));
    vec3 irradiance = textureCubeUV(environmentMap, n, 1.0).rgb * f.coat.w;
    vec3 radiance = textureCubeUV(environmentMap, reflectDir, r).rgb * f.coat.w;
    vec2 fab = texture(dfgMap, vec2(r, nv)).rg;
    vec3 singleScatter = f0 * fab.x + fab.y;
    float ems = 1.0 - fab.x - fab.y;
    vec3 favg = f0 + (1.0 - f0) * 0.047619;
    vec3 multiScatter = singleScatter * favg / (1.0 - ems * favg) * ems;
    vec3 diffuse = tint * (1.0 - singleScatter - multiScatter) * irradiance;
    vec3 direct = (f.areaTarget.w > .5) ? musicIrradiance(world, n) : vec3(1);
    vec3 areaDiffuse = vec3(0), areaSpecular = vec3(0);
#if AREA_ENABLED
    if ((f.areaTarget.w > .5) && f.areaPosition.w > 0.0)
        musicArea(n, v, world, r, f0, areaDiffuse, areaSpecular);
#endif
    diffuse += tint * (direct + areaDiffuse);
    vec3 specular = radiance * singleScatter + multiScatter * irradiance;
    if ((f.areaTarget.w > .5))
        specular += musicSpecular(n, v, world, r, f0, true) + areaSpecular;
    vec3 ray = normalize(refract(-v, n, 1.0 / 1.46)) * f.material.z;
    vec4 projected = f.nativeVP * vec4(world + ray, 1.0);
    vec4 here = f.nativeVP * vec4(world, 1.0);
    // Native projection and gl_FragCoord share backend-corrected framebuffer axes.
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(captureMap, 0))
            + (projected.xy / projected.w - here.xy / here.w) * 0.5;
    vec2 edge = abs(localPosition.xy - vec2(0.17, 1.85)) - vec2(1.53);
    float proximity = 1.0 - smoothstep(0.015, 0.15, max(edge.x, edge.y));
    float zoned = int(f.coat.z) == 0 ? mix(1.0, 0.16, proximity) : 1.0;
    // Quick3D generates the full mip chain: derive its last level from the actual capture dimensions.
    float maxLod =
            floor(log2(float(max(textureSize(captureMap, 0).x, textureSize(captureMap, 0).y))));
    float lod = clamp(log2(float(textureSize(captureMap, 0).x)) * r * 0.92 * zoned, 0.0, maxLod);
    vec3 sampleColor = textureBicubic(captureMap, clamp(uv, vec2(0.0), vec2(1.0)), lod).rgb;
    vec3 transmittance = tint * exp(log(attenuation) * length(ray) / f.material.w);
    vec3 transmitted = (1.0 - environmentBRDF(nv, r, f0)) * transmittance * sampleColor;
    vec3 outgoing = mix(diffuse, transmitted, f.material.x) + specular;
    vec3 coatDir = normalize(mix(reflect(-v, n), n, pow(cr, 4.0)));
    vec3 coat = textureCubeUV(environmentMap, coatDir, cr).rgb * f.coat.w
            * environmentBRDF(nv, cr, vec3(0.04));
    if ((f.areaTarget.w > .5)) {
        coat += musicSpecular(n, v, world, cr, vec3(.04), false);
#if AREA_ENABLED
        if (f.areaPosition.w > 0.0) {
            vec3 unused, areaCoat;
            musicArea(n, v, world, cr, vec3(.04), unused, areaCoat);
            coat += areaCoat;
        }
#endif
    }
    float fresnel = 0.04 + 0.96 * exp2((-5.55473 * nv - 6.98316) * nv);
    outgoing = outgoing * (1.0 - f.coat.x * fresnel) + f.coat.x * coat;
    if ((f.areaTarget.w > .5))
        outgoing = musicShell(outgoing, f.model[3].xyz, localPosition, n, v, int(f.coat.z));
    // RenderPass into a linear target does not tone-map individual materials.
    color = vec4((f.areaTarget.w > .5) ? outgoing : ACESFilmicToneMapping(outgoing), 1.0);
    normalOut = vec4(normalize(viewNormal) * .5 + .5, 1);
    depthOut = vec4(packDepth((viewDepth - f.clip.x) / (f.clip.y - f.clip.x)), 1);
}
