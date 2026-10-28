// MeshStandardMaterial transport from locked Three 0.183.2 (MIT).
// LTC/GGX helpers share the existing source-derived shell equations; no musicShell modulation.
layout(binding = 5) uniform sampler2D dfgMap;
layout(binding = 6) uniform sampler2D ltc1Map;
layout(binding = 7) uniform sampler2D ltc2Map;
float keyShadow;
#include "shell-lighting.glsl"
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
