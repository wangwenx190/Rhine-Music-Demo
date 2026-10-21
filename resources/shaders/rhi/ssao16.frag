#version 440
#extension GL_GOOGLE_include_directive : require
#define KERNEL_COUNT 16
#include "post-input.glsl"
#include "ssao-body.glsl"
