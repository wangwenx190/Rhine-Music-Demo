// BEGIN SHARED LIGHTING
// Shared field, diffuse and shell transport from frozen music-lighting.ts (MIT).
float musicRowField(vec3 origin)
{
    float row = (origin.z - rowColumn.z - rowSettings.w * .62) / (.5 * rowSettings.z * .62);
    float lane = (origin.x - rowColumn.x) / 27.0;
    float lane2 = lane * lane;
    return exp(-0.6931471805599453 * row * row) * exp(-lane2 * lane2 * lane2);
}
float musicPrintExposure(vec3 origin)
{
    vec3 distance = (origin - lightColumn) / vec3(6.0, 3.35, 5.5);
    float printLight = exp(-dot(distance, distance));
    float baseline = mix(printAmbient, 1.0, printLight);
    if (rowSettings.x < .5)
        return baseline;
    float ambient = max(.08, printAmbient * .5);
    float fillAmount = rowSettings.x > 1.5 && rowSettings.x < 2.5 ? rowFill : 0.0;
    float fieldAmbient = mix(ambient, baseline, fillAmount);
    float fieldExposure =
            mix(fieldAmbient, 1.0, clamp(rowSettings.y * musicRowField(origin), 0.0, 1.0));
    float areaExposure = mix(ambient, printAmbient, .25);
    float shelf = rowSettings.x < 1.5 ? areaExposure : fieldExposure;
    return mix(shelf, baseline, rowDetail);
}
vec3 musicSpot(vec3 position)
{
    vec3 delta = spotPosition - position;
    float distanceSquared = max(dot(delta, delta), .01);
    float angle = dot(normalize(delta), normalize(spotPosition - spotTarget));
    float attenuation = smoothstep(cos(.32), cos(.32 * (1.0 - .95)), angle) / distanceSquared;
    return spotColor * spotIntensity * attenuation;
}
vec3 musicIrradiance(vec3 position, vec3 normal)
{
    vec3 hemi = mix(groundColor, skyColor, .5 * normal.y + .5) * lightIntensities.y;
    vec3 key = keyColor * lightIntensities.x * max(0.0, dot(normal, normalize(vec3(-6, 14, -5))));
    vec3 fill = vec3(lightIntensities.z * max(0.0, dot(normal, normalize(vec3(7, 8, -10)))));
    vec3 spot = musicSpot(position) * max(0.0, dot(normal, normalize(spotPosition - position)));
    return (hemi + key + fill + spot) / 3.141592653589793;
}
vec3 musicShell(vec3 original, vec3 origin, vec3 local, vec3 normal, vec3 view, int surface)
{
    float laneDistance = abs(origin.x - lightColumn.x);
    float core = exp(-pow(laneDistance / 3.2, 4.0));
    float spill = exp(-pow(laneDistance / 7.2, 4.0));
    float laneLight = .58 * core + .42 * spill;
    float rowLight = exp(-pow((origin.z - lightColumn.z) / 5.5, 2.0));
    float hot = exp(-pow((origin.z - lightColumn.z) / 1.1, 2.0));
    float neighbor = exp(-pow((origin.z - lightColumn.z) / 2.4, 2.0));
    float guided = .58 * core * mix(.12, 1.0, rowLight) + .42 * spill * neighbor;
    float edgeTransport = exp(-max(0.0, local.x + 2.225) * 1.7);
    float panelHeight = clamp((local.y - .175) / 3.35, 0.0, 1.0);
    float topRim = exp(-max(0.0, 3.525 - local.y) * 24.0);
    float grazing = 1.0 - clamp(abs(dot(normal, view)), 0.0, 1.0);
    bool spine = surface == 1;
    vec3 selected = original * mix(.96, 1.04, guided);
    if (surface < 2) {
        float edgeScatter = (spine ? .30 : .14) * edgeTransport + (spine ? .016 : .003);
        selected +=
                scatterColor * scatterStrength * guided * edgeScatter * mix(1.0, .62, panelHeight);
        float ribbon = topRim * (.22 + .78 * edgeTransport)
                + (spine ? .18 : .035) * edgeTransport * grazing;
        selected += scatterColor * scatterStrength * laneLight * hot * ribbon * .8;
    }
    if (rowSettings.x < .5)
        return selected;
    float gain = rowSettings.x > 1.5 ? musicRowField(origin) * rowSettings.y : 0.0;
    float fillAmount = rowSettings.x > 1.5 && rowSettings.x < 2.5 ? rowFill : 0.0;
    vec3 shelf = original
            * mix(mix(.56, mix(.96, 1.04, guided), fillAmount), 1.08, clamp(gain, 0.0, 1.0));
    if (surface < 2) {
        float panel = (spine ? .30 : .12) * edgeTransport + (spine ? .05 : .025);
        float rim = topRim * (.55 + .45 * edgeTransport)
                + (spine ? .24 : .07) * edgeTransport * grazing;
        float scatter = panel * mix(1.0, .78, panelHeight) + rim * 1.25;
        shelf +=
                scatterColor * scatterStrength * gain * scatter * (rowSettings.x > 2.5 ? .86 : 1.0);
    }
    return mix(shelf, selected, rowDetail);
}

// The rational LTC fit approximates theta/sin(theta)/(2*pi); epsilon protects grazing angles.
const float LtcNumeratorConstant = 0.8543985;
const float LtcNumeratorLinear = 0.4965155;
const float LtcNumeratorQuadratic = 0.0145206;
const float LtcDenominatorConstant = 3.4175940;
const float LtcDenominatorLinear = 4.1616724;
const float LtcAngularEpsilon = 1e-7;

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
    float a = LtcNumeratorConstant + (LtcNumeratorLinear + LtcNumeratorQuadratic * y) * y;
    float b = LtcDenominatorConstant + (LtcDenominatorLinear + y) * y;
    float v = a / b;

    float theta_sintheta =
            (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, LtcAngularEpsilon)) - v;

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
    vec3 z = normalize(areaPosition - areaTarget);
    vec3 x = normalize(cross(vec3(0, 1, 0), z)), y = cross(z, x);
    vec3 hw = x * 19.5, hh = y * (.09 * rowSettings.z);
    vec3 rect[4];
    rect[0] = areaPosition + hw - hh;
    rect[1] = areaPosition - hw - hh;
    rect[2] = areaPosition - hw + hh;
    rect[3] = areaPosition + hw + hh;
    vec2 uv = vec2(r, sqrt(1.0 - clamp(dot(n, v), 0.0, 1.0))) * (63.0 / 64.0) + .5 / 64.0;
    vec4 t1 = texture(ltc1Map, uv), t2 = texture(ltc2Map, uv);
    mat3 mInv = mat3(vec3(t1.x, 0, t1.y), vec3(0, 1, 0), vec3(t1.z, 0, t1.w));
    vec3 energy = spotColor * areaIntensity;
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
    return keyColor * lightIntensities.x
            * musicBRDF(n, v, normalize(vec3(-6, 14, -5)), r, f0, multiscatter)
            + vec3(lightIntensities.z)
            * musicBRDF(n, v, normalize(vec3(7, 8, -10)), r, f0, multiscatter)
            + musicSpot(position)
            * musicBRDF(n, v, normalize(spotPosition - position), r, f0, multiscatter);
}

// END SHARED LIGHTING
// Three.js ShaderChunk derivation (MIT; resources/licenses/three-LICENSE).
VARYING vec3 localPosition;
VARYING vec3 worldPosition;
VARYING vec3 worldNormal;
VARYING vec3 lightOrigin;
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

float roughnessToMip(float roughness)
{

    float mip = 0.0;

    if (roughness >= cubeUV_r1) {

        mip = (cubeUV_r0 - roughness) * (cubeUV_m1 - cubeUV_m0) / (cubeUV_r0 - cubeUV_r1)
                + cubeUV_m0;

    } else if (roughness >= cubeUV_r4) {

        mip = (cubeUV_r1 - roughness) * (cubeUV_m4 - cubeUV_m1) / (cubeUV_r1 - cubeUV_r4)
                + cubeUV_m1;

    } else if (roughness >= cubeUV_r5) {

        mip = (cubeUV_r4 - roughness) * (cubeUV_m5 - cubeUV_m4) / (cubeUV_r4 - cubeUV_r5)
                + cubeUV_m4;

    } else if (roughness >= cubeUV_r6) {

        mip = (cubeUV_r5 - roughness) * (cubeUV_m6 - cubeUV_m5) / (cubeUV_r5 - cubeUV_r6)
                + cubeUV_m5;

    } else {

        mip = -2.0 * log2(1.16 * roughness); // 1.16 = 1.79^0.25
    }

    return mip;
}

vec4 textureCubeUV(sampler2D envMap, vec3 sampleDir, float roughness)
{

    float mip = clamp(roughnessToMip(roughness), cubeUV_m0, CUBEUV_MAX_MIP);

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

    color *= exposure / 0.6;

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
float musicGeometryRoughness(vec3 normal)
{
    // Three nonPerturbedNormal is in view space; max-component derivatives are not rotation invariant.
    vec3 geometryNormal = normalize(mat3(VIEW_MATRIX) * normal);
    vec3 dxy = max(abs(dFdx(geometryNormal)), abs(dFdy(geometryNormal)));
    return max(dxy.x, max(dxy.y, dxy.z));
}
void MAIN()
{
    vec3 n = normalize(worldNormal);
    vec3 v = normalize(CAMERA_POSITION - worldPosition);
    float nv = clamp(dot(n, v), 0.0, 1.0);
    vec3 tint = mix(nightTint.rgb, dayTint.rgb, warmth);
    vec3 attenuation = mix(nightAttenuation.rgb, dayAttenuation.rgb, warmth);
    vec3 f0 = vec3(pow((1.46 - 1.0) / (1.46 + 1.0), 2.0));
    float geometryRoughness = musicGeometryRoughness(n);
    float r = min(1.0, max(roughness, 0.0525) + geometryRoughness);
    float cr = min(1.0, max(coatRoughness, 0.0525) + geometryRoughness);
    vec3 reflectDir = normalize(mix(reflect(-v, n), n, pow(r, 4.0)));
    vec3 irradiance = textureCubeUV(environmentMap, n, 1.0).rgb * environmentIntensity;
    vec3 radiance = textureCubeUV(environmentMap, reflectDir, r).rgb * environmentIntensity;
    vec2 fab = texture(dfgMap, vec2(r, nv)).rg;
    vec3 singleScatter = f0 * fab.x + fab.y;
    float ems = 1.0 - fab.x - fab.y;
    vec3 favg = f0 + (1.0 - f0) * 0.047619;
    vec3 multiScatter = singleScatter * favg / (1.0 - ems * favg) * ems;
    vec3 diffuse = tint * (1.0 - singleScatter - multiScatter) * irradiance;
    vec3 direct = lightingEnabled ? musicIrradiance(worldPosition, n) : paperIrradiance;
    vec3 areaDiffuse = vec3(0), areaSpecular = vec3(0);
    if (lightingEnabled && areaIntensity > 0.0)
        musicArea(n, v, worldPosition, r, f0, areaDiffuse, areaSpecular);
    diffuse += tint * (direct + areaDiffuse);
    vec3 specular = radiance * singleScatter + multiScatter * irradiance;
    if (lightingEnabled)
        specular += musicSpecular(n, v, worldPosition, r, f0, true) + areaSpecular;
    vec3 ray = normalize(refract(-v, n, 1.0 / 1.46)) * thickness;
    vec4 projected = VIEWPROJECTION_MATRIX * vec4(worldPosition + ray, 1.0);
    vec4 here = VIEWPROJECTION_MATRIX * vec4(worldPosition, 1.0);
    // Native projection and FRAGCOORD share backend-corrected framebuffer axes.
    vec2 uv = FRAGCOORD.xy / vec2(textureSize(SCREEN_MIP_TEXTURE, 0))
            + (projected.xy / projected.w - here.xy / here.w) * 0.5;
    vec2 edge = abs(localPosition.xy - vec2(0.17, 1.85)) - vec2(1.53);
    float proximity = 1.0 - smoothstep(0.015, 0.15, max(edge.x, edge.y));
    float zoned = surface == 0 ? mix(1.0, 0.16, proximity) : 1.0;
    // Quick3D generates the full mip chain: derive its last level from the actual capture dimensions.
    float maxLod = floor(log2(float(
            max(textureSize(SCREEN_MIP_TEXTURE, 0).x, textureSize(SCREEN_MIP_TEXTURE, 0).y))));
    float lod = clamp(log2(float(textureSize(SCREEN_MIP_TEXTURE, 0).x)) * r * 0.92 * zoned, 0.0,
                      maxLod);
    vec3 sampleColor = textureBicubic(SCREEN_MIP_TEXTURE, clamp(uv, vec2(0.0), vec2(1.0)), lod).rgb;
    vec3 transmittance = tint * exp(log(attenuation) * length(ray) / attenuationDistance);
    vec3 transmitted = (1.0 - environmentBRDF(nv, r, f0)) * transmittance * sampleColor;
    vec3 outgoing = mix(diffuse, transmitted, transmission) + specular;
    vec3 coatDir = normalize(mix(reflect(-v, n), n, pow(cr, 4.0)));
    vec3 coat = textureCubeUV(environmentMap, coatDir, cr).rgb * environmentIntensity
            * environmentBRDF(nv, cr, vec3(0.04));
    if (lightingEnabled) {
        coat += musicSpecular(n, v, worldPosition, cr, vec3(.04), false);
        if (areaIntensity > 0.0) {
            vec3 unused, areaCoat;
            musicArea(n, v, worldPosition, cr, vec3(.04), unused, areaCoat);
            coat += areaCoat;
        }
    }
    float fresnel = 0.04 + 0.96 * exp2((-5.55473 * nv - 6.98316) * nv);
    outgoing = outgoing * (1.0 - clearcoat * fresnel) + clearcoat * coat;
    if (lightingEnabled)
        outgoing = musicShell(outgoing, lightOrigin, localPosition, n, v, surface);
    // RenderPass into a linear target does not tone-map individual materials.
    FRAGCOLOR = vec4(compositorOutput ? outgoing : ACESFilmicToneMapping(outgoing), 1.0);
}
