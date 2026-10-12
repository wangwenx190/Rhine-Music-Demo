#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=0) out float canonicalDepth;
void main() {
    vec4 p=f.model*vec4(position,1);
    vec4 canonical=f.lightVP*p;
    canonicalDepth=canonical.z/canonical.w*.5+.5;
    gl_Position=f.lightMvp*vec4(position,1);
}
