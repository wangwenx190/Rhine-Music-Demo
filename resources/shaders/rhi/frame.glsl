// std140; byte layout asserted in RhiItem.cpp. Same interface in every stage.
layout(std140, binding = 0) uniform Frame
{
    mat4 mvp; // Reserved ABI slot; geometry reuses nativeVP * world position.
    mat4 model;
    mat4 view;
    mat4 lightMvp;
    mat4 lightVP;
    vec4 background;
    vec4 rowSettings;
    vec4 rowColumn;
    vec4 lightColumn;
    vec4 spotPosition;
    vec4 spotTarget;
    vec4 spotColor;
    vec4 keyColor;
    vec4 skyColor;
    vec4 groundColor;
    vec4 lightIntensities;
    vec4 settings; // printAmbient, rowDetail, rowFill, environmentIntensity
    vec4 clip; // near, far, framebuffer Y up, clip depth zero to one
    vec4 draw; // kind (-1 background, 0 paper, 1 geometry, 2 receiver, 6 product floor), shadow enabled, size, radius
    vec4 optics; // spotIntensity, shadow strength, normalBias, depth bias
    mat4 nativeVP;
    vec4 camera; // world position, exposure
    vec4 tint;
    vec4 attenuation;
    vec4 material; // transmission, roughness, thickness, attenuation distance
    vec4 coat; // clearcoat, roughness, surface, environment intensity
    vec4 areaPosition; // intensity in w
    vec4 areaTarget;
    vec4 scatter; // strength in w
    vec4 coverUv; // normalized atlas rectangle; full image for standalone covers
    vec4 paperSize; // xy: cover pixels, z: painted cover, w: Review-only optical math selector
    vec4 originLight0; // row field, print exposure, lane light, guided light (per entity)
    vec4 originLight1; // hot ribbon; remaining components reserved
    vec4 fog; // near, far, enabled; color shares the authored linear background
}
f;
