// Frozen SSAOBlurShader: symmetric 5x5 box, no bilateral/depth weighting.
void MAIN() {
    float result=0.0;
    for(int i=-2;i<=2;++i)for(int j=-2;j<=2;++j)
        result+=texture(INPUT,INPUT_UV+vec2(i,j)/INPUT_SIZE).r;
    FRAGCOLOR=vec4(vec3(result/25.0),1.0);
}
