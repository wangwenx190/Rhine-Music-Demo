// Matte print: contain + 1/64 transparent inset, no emission or tone mapping.
VARYING vec2 paperUv;
vec3 srgbToLinear(vec3 c) {
    return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
}
vec3 printColor;
void MAIN() {
    vec2 size=vec2(textureSize(coverMap,0));
    vec2 extent=vec2(1.0-2.0/64.0)*size/max(size.x,size.y);
    vec2 uv=(paperUv-vec2(0.5))/extent+vec2(0.5);
    if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0))))discard;
    vec4 ink=texture(coverMap,clamp(uv,0.5/size,1.0-0.5/size));
    if(ink.a<0.025)discard;
    vec3 source=srgbToLinear(ink.rgb);
    printColor=source;
    BASE_COLOR=vec4(source,1.0);
    METALNESS=0.0;
    ROUGHNESS=1.0;
}
void POST_PROCESS() {
    COLOR_SUM.rgb=min(printColor*max(paperIrradiance,vec3(0.0)),printColor);
}
