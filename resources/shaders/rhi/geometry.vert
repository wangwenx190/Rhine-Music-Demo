#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
invariant gl_Position;
layout(binding = 1) uniform sampler2D coverMap;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 0) centroid out vec2 paperUv;
layout(location = 1) out vec3 world;
layout(location = 2) out vec3 worldNormal;
layout(location = 3) out vec3 viewNormal;
layout(location = 4) out float viewDepth;
layout(location = 5) out vec3 localPosition;
layout(location = 6) flat out vec4 entityLight0;
layout(location = 7) flat out vec4 entityLight1;
layout(location = 8) flat out vec4 entityCoverUv;
void main()
{
    entityLight0 = f.originLight0;
    entityLight1 = f.originLight1;
    entityCoverUv = f.coverUv;
    vec3 rasterPosition = position;
    paperUv = uv;
    if (f.draw.x == 0.0) {
        // Rasterize the exact visible contain rectangle. Discarding the UV inset after
        // interpolation would bypass geometric MSAA and leave its boundary stair-stepped.
        vec2 size = f.paperSize.xy;
        vec2 extent = (1.0 - 2.0 / 64.0) * size / max(size.x, size.y);
        rasterPosition.xy = vec2(.17, 1.85) + (position.xy - vec2(.17, 1.85)) * extent;
        paperUv = .5 + (uv - .5) * extent;
    }
    vec4 p = f.model * vec4(rasterPosition, 1);
    world = p.xyz;
    worldNormal = normalize(transpose(inverse(mat3(f.model))) * normal);
    viewNormal = normalize(mat3(f.view) * worldNormal);
    viewDepth = -(f.view * p).z;
    localPosition = position;
    gl_Position = f.draw.x < -.5 ? vec4(position, 1) : f.nativeVP * p;
}
