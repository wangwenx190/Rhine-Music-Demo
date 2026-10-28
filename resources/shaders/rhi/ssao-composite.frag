#version 440
#extension GL_GOOGLE_include_directive : require
#include "post-input.glsl"

void main()
{
    vec4 c = texture(INPUT, INPUT_UV);
    color = vec4(c.rgb * mix(1.0, texture(auxiliaryMap, INPUT_UV).r, f.options.x), c.a);
}
