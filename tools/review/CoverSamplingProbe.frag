#version 440
#extension GL_GOOGLE_include_directive : require
layout(binding = 0) uniform sampler2D coverMap;
layout(std140, binding = 1) uniform Probes
{
    vec4 requests[12];
    vec4 derivatives[12];
}
f;
layout(location = 0) out vec4 color;
const vec4 entityCoverUv = vec4(0, 0, .5, 1);
#include "../../resources/shaders/rhi/cover-sampling.glsl"
void main()
{
    int index = int(gl_FragCoord.x);
    vec4 request = f.requests[index];
    vec4 derivatives = f.derivatives[index];
    color = request.w > .5 ? filteredCover(request.xy, derivatives.xy, derivatives.zw)
                           : coverTrilinear(request.xy, request.z);
}
