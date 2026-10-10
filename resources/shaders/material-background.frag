void MAIN() {
    vec3 c=backgroundSrgb;
    vec3 linear=mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
    FRAGCOLOR=vec4(linear,1.0);
}
