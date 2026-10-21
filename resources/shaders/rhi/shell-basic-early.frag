#version 440
#extension GL_GOOGLE_include_directive : require
layout(early_fragment_tests) in;
#define AREA_ENABLED 0
#include "shell-body.glsl"
