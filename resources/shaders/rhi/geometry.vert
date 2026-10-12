#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec2 paperUv;
layout(location = 1) out vec3 world;
layout(location = 2) out vec3 worldNormal;
layout(location = 3) out vec3 viewNormal;
layout(location = 4) out float viewDepth;
void main()
{
    vec4 p = f.model * vec4(position, 1);
    world = p.xyz;
    worldNormal = normalize(mat3(f.model) * normal);
    viewNormal = normalize(mat3(f.view) * worldNormal);
    viewDepth = -(f.view * p).z;
    paperUv = uv;
    gl_Position = f.draw.x < -.5 ? vec4(position, 1) : f.mvp * vec4(position, 1);
}
