// Derived from locked Three.js (MIT); resources/licenses/three-LICENSE.
void MAIN()
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