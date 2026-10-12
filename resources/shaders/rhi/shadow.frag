#version 440
layout(location=0) in float canonicalDepth;
layout(location=0) out vec4 color;
void main() { color=vec4(canonicalDepth,canonicalDepth,canonicalDepth,1); }
