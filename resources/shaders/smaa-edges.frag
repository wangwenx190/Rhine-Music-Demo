// Derived from locked Three.js (MIT); resources/licenses/three-LICENSE.
#define SMAA_THRESHOLD 0.1
#define resolution (1.0 / INPUT_SIZE)
vec2 frameUv(vec2 uv)
{
    return FRAMEBUFFER_Y_UP > 0.0 ? uv : vec2(uv.x, 1.0 - uv.y);
}
vec4 readFrame(sampler2D tex, vec2 uv)
{
    return texture(tex, frameUv(uv));
}
vec4 readFrame(sampler2D tex, vec2 uv, float bias)
{
    return textureLod(tex, frameUv(uv), 0.0);
}
vec2 vUv;
vec4 vOffset[3];
void SMAAEdgeDetectionVS(vec2 texcoord)
{
    vOffset[0] = texcoord.xyxy
            + resolution.xyxy
                    * vec4(-1.0, 0.0, 0.0, 1.0); // WebGL port note: Changed sign in W component
    vOffset[1] = texcoord.xyxy
            + resolution.xyxy
                    * vec4(1.0, 0.0, 0.0, -1.0); // WebGL port note: Changed sign in W component
    vOffset[2] = texcoord.xyxy
            + resolution.xyxy
                    * vec4(-2.0, 0.0, 0.0, 2.0); // WebGL port note: Changed sign in W component
}

vec4 SMAAColorEdgeDetectionPS(vec2 texcoord, vec4 offset[3], sampler2D colorTex)
{
    vec2 threshold = vec2(SMAA_THRESHOLD, SMAA_THRESHOLD);

    // Calculate color deltas:
    vec4 delta;
    vec3 C = readFrame(colorTex, texcoord).rgb;

    vec3 Cleft = readFrame(colorTex, offset[0].xy).rgb;
    vec3 t = abs(C - Cleft);
    delta.x = max(max(t.r, t.g), t.b);

    vec3 Ctop = readFrame(colorTex, offset[0].zw).rgb;
    t = abs(C - Ctop);
    delta.y = max(max(t.r, t.g), t.b);

    // We do the usual threshold:
    vec2 edges = step(threshold, delta.xy);

    // Then discard if there is no edge:
    if (dot(edges, vec2(1.0, 1.0)) == 0.0)
        return vec4(0.0);

    // Calculate right and bottom deltas:
    vec3 Cright = readFrame(colorTex, offset[1].xy).rgb;
    t = abs(C - Cright);
    delta.z = max(max(t.r, t.g), t.b);

    vec3 Cbottom = readFrame(colorTex, offset[1].zw).rgb;
    t = abs(C - Cbottom);
    delta.w = max(max(t.r, t.g), t.b);

    // Calculate the maximum delta in the direct neighborhood:
    float maxDelta = max(max(max(delta.x, delta.y), delta.z), delta.w);

    // Calculate left-left and top-top deltas:
    vec3 Cleftleft = readFrame(colorTex, offset[2].xy).rgb;
    t = abs(C - Cleftleft);
    delta.z = max(max(t.r, t.g), t.b);

    vec3 Ctoptop = readFrame(colorTex, offset[2].zw).rgb;
    t = abs(C - Ctoptop);
    delta.w = max(max(t.r, t.g), t.b);

    // Calculate the final maximum delta:
    maxDelta = max(max(maxDelta, delta.z), delta.w);

    // Local contrast adaptation in action:
    edges.xy *= step(0.5 * maxDelta, delta.xy);

    return vec4(edges, 0.0, 0.0);
}

void MAIN()
{
    vUv = frameUv(INPUT_UV);
    SMAAEdgeDetectionVS(vUv);

    FRAGCOLOR = SMAAColorEdgeDetectionPS(vUv, vOffset, INPUT);
}
