vec3 linearToSrgb(vec3 c) {
    return mix(12.92*c,1.055*pow(max(c,vec3(0.0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));
}
void MAIN() {
    vec4 c=texture(INPUT,INPUT_UV);
    FRAGCOLOR=vec4(rawChannel?c.rgb:linearToSrgb(c.rgb),c.a);
}
