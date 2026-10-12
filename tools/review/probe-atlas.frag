VARYING vec2 atlasUv;
void MAIN()
{
    FRAGCOLOR = texture(atlasMap, atlasUv);
}
