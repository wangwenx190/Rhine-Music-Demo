#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
// Preserve the shared native stage signature prefix even for unused prepass inputs.
layout(location = 0) centroid in vec2 paperUv;
layout(location = 1) in vec3 world;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 viewNormal;
layout(location = 4) in float viewDepth;
layout(location = 0) out vec4 normalOut;
layout(location = 1) out vec4 depthOut;
void main()
{
    if (f.draw.x < -.5) {
        normalOut = vec4(f.background.rgb, 1);
        depthOut = vec4(1);
        return;
    }
    normalOut = vec4(normalize(viewNormal) * .5 + .5, 1);
    float depth = (viewDepth - f.clip.x) / (f.clip.y - f.clip.x);
    float code = min(16777215.0, floor(clamp(depth, 0.0, 1.0) * 16777215.0 + .5));
    depthOut = vec4(vec3(floor(code / 65536.0), mod(floor(code / 256.0), 256.0), mod(code, 256.0))
                            / 255.0,
                    1);
}
