// std140; byte layout asserted in RhiItem.cpp. Same interface in every stage.
layout(std140,binding=0) uniform Frame {
    mat4 mvp; mat4 model; mat4 view; mat4 lightMvp; mat4 lightVP;
    vec4 background; vec4 rowSettings; vec4 rowColumn; vec4 lightColumn;
    vec4 spotPosition; vec4 spotTarget; vec4 spotColor;
    vec4 keyColor; vec4 skyColor; vec4 groundColor; vec4 lightIntensities;
    vec4 settings; // printAmbient, rowDetail, rowFill, environmentIntensity
    vec4 clip; // near, far, framebuffer Y up, clip depth zero to one
    vec4 draw; // kind (-1 background, 0 paper, 1 geometry, 2 receiver), shadow enabled, size, radius
    vec4 optics; // spotIntensity, shadow strength, normalBias, depth bias
} f;
