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
// Frozen SSAOBlurShader: symmetric 5x5 box, no bilateral/depth weighting.
void main()
{
    float result = 0.0;
    for (int i = -2; i <= 2; ++i)
        for (int j = -2; j <= 2; ++j)
            result += texture(INPUT, INPUT_UV + vec2(i, j) / INPUT_SIZE).r;
    FRAGCOLOR = vec4(vec3(result / 25.0), 1.0);
}
