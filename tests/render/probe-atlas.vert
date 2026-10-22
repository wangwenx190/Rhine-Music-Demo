VARYING vec2 atlasUv;
void MAIN()
{
    float slot = INSTANCE_DATA.x;
    vec2 tile = vec2(mod(slot, 8.0), floor(slot / 8.0));
    atlasUv = (tile + clamp(UV0, vec2(.5 / 256.0), vec2(1.0 - .5 / 256.0))) / 8.0;
    POSITION = INSTANCE_MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
