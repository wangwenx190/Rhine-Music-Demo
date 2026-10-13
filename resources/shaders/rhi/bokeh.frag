#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D colorMap;
layout(std140, binding = 1) uniform Post
{
    vec4 size;
    vec4 clip;
    vec4 ao;
    vec4 dof;
    vec4 options;
    vec4 kernel[64];
}
f;
layout(binding = 2) uniform sampler2D depthMap;
layout(binding = 3) uniform sampler2D geometryMap;
layout(binding = 4) uniform sampler2D samplesMap;
layout(binding = 5) uniform sampler2D noiseMap;
layout(binding = 6) uniform sampler2D auxiliaryMap;
layout(binding = 7) uniform sampler2D areaMap;
layout(binding = 8) uniform sampler2D searchMap;
#define INPUT colorMap
#define INPUT_UV (f.clip.z > 0.0 ? uv : vec2(uv.x, 1.0 - uv.y))
#define INPUT_SIZE f.size.xy
#define FRAMEBUFFER_Y_UP f.clip.z
#define cameraNear f.clip.x
#define cameraFar f.clip.y
#define projectionPlane vec3(f.size.zw, 0)
#define kernelSize int(f.ao.x)
#define kernelRadius f.ao.y
#define minDistance f.ao.z
#define maxDistance f.ao.w
#define focus f.dof.x
#define aperture f.dof.y
#define maxblur f.dof.z
#define aspect f.dof.w
#define weightMap auxiliaryMap
#define FRAGCOLOR color
// Derived from locked Three.js (MIT); resources/licenses/three-LICENSE.
void main()
{

    vec2 aspectcorrect = vec2(1.0, aspect);

    float viewZ = -mix(cameraNear, cameraFar,
                       dot(texture(depthMap, INPUT_UV).rgb, vec3(65536.0, 256.0, 1.0) / 65793.0));

    float factor = (focus + viewZ); // viewZ is <= 0, so this is a difference equation

    vec2 dofblur = vec2(clamp(factor * aperture, -maxblur, maxblur));

    vec2 dofblur9 = dofblur * 0.9;
    vec2 dofblur7 = dofblur * 0.7;
    vec2 dofblur4 = dofblur * 0.4;

    vec4 col = vec4(0.0);

    col += texture(INPUT, INPUT_UV.xy);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.0, 0.4) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.15, 0.37) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.29, 0.29) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.37, 0.15) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.40, 0.0) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.37, -0.15) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.29, -0.29) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.15, -0.37) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.0, -0.4) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.15, 0.37) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.29, 0.29) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.37, 0.15) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.4, 0.0) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.37, -0.15) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.29, -0.29) * aspectcorrect) * dofblur);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.15, -0.37) * aspectcorrect) * dofblur);

    col += texture(INPUT, INPUT_UV.xy + (vec2(0.15, 0.37) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.37, 0.15) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.37, -0.15) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.15, -0.37) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.15, 0.37) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.37, 0.15) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.37, -0.15) * aspectcorrect) * dofblur9);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.15, -0.37) * aspectcorrect) * dofblur9);

    col += texture(INPUT, INPUT_UV.xy + (vec2(0.29, 0.29) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.40, 0.0) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.29, -0.29) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.0, -0.4) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.29, 0.29) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.4, 0.0) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.29, -0.29) * aspectcorrect) * dofblur7);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.0, 0.4) * aspectcorrect) * dofblur7);

    col += texture(INPUT, INPUT_UV.xy + (vec2(0.29, 0.29) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.4, 0.0) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.29, -0.29) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.0, -0.4) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.29, 0.29) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.4, 0.0) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(-0.29, -0.29) * aspectcorrect) * dofblur4);
    col += texture(INPUT, INPUT_UV.xy + (vec2(0.0, 0.4) * aspectcorrect) * dofblur4);

    FRAGCOLOR = col / 41.0;
    FRAGCOLOR.a = 1.0;
}