#version 440
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location = 0) in vec3 position;
layout(location = 3) in vec4 model0;
layout(location = 4) in vec4 model1;
layout(location = 5) in vec4 model2;
layout(location = 6) in vec4 model3;
layout(location = 0) out float canonicalDepth;
void main()
{
    mat4 model = mat4(model0, model1, model2, model3);
    vec4 world = model * vec4(position, 1);
    vec4 canonical = f.lightVP * world;
    canonicalDepth = canonical.z / canonical.w * .5 + .5;
    gl_Position = f.lightMvp * world;
}
