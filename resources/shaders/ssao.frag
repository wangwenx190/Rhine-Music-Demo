// Three SSAOShader, MIT: hemisphere samples, tangent basis, orthographic depth delta.
// Source noise is a scalar replicated into xyz (not a replacement RGB noise algorithm).
vec3 viewPosition(vec2 uv,float depth) {
    vec2 ndc=uv*2.0-1.0;
    ndc.y*=FRAMEBUFFER_Y_UP;
    // Prepass stores linear distance as RGB24 through the Quick texture provider.
    float z=-mix(cameraNear,cameraFar,depth);
    return vec3(ndc*projectionPlane.xy*(-z),z);
}
float linearDepth(vec2 uv) {
    return dot(texture(depthMap,uv).rgb,vec3(65536.0,256.0,1.0)/65793.0);
}
void MAIN() {
    vec2 uv=INPUT_UV;
    vec4 geometry=texture(geometryMap,uv);
    float depth=linearDepth(uv);
    // RGB24 reconstruction can round white to 1-ulp on some backends.
    if(kernelSize==0||depth>=1.0-1e-6) {FRAGCOLOR=vec4(1.0);return;}
    vec3 position=viewPosition(uv,depth);
    vec3 normal=normalize(geometry.rgb*2.0-1.0);
    vec2 sourceUv=uv;
    if(FRAMEBUFFER_Y_UP<0.0)sourceUv.y=1.0-sourceUv.y;
    vec3 random=vec3(texture(noiseMap,sourceUv*vec2(textureSize(geometryMap,0))/4.0).r);
    vec3 tangent=normalize(random-normal*dot(random,normal));
    mat3 basis=mat3(tangent,cross(normal,tangent),normal);
    float occlusion=0.0;
    for(int i=0;i<64;++i) {
        if(i>=kernelSize)break;
        vec3 samplePoint=position+basis*texelFetch(samplesMap,ivec2(i,0),0).xyz*kernelRadius;
        vec2 sampleUv=samplePoint.xy/(-samplePoint.z*projectionPlane.xy)*.5+.5;
        if(FRAMEBUFFER_Y_UP<0.0)sampleUv.y=1.0-sampleUv.y;
        float realDepth=linearDepth(clamp(sampleUv,vec2(0),vec2(1)));
        float sampleDepth=(-samplePoint.z-cameraNear)/(cameraFar-cameraNear);
        float delta=sampleDepth-realDepth;
        if(delta>minDistance&&delta<maxDistance)occlusion+=1.0;
    }
    FRAGCOLOR=vec4(vec3(1.0-clamp(occlusion/float(kernelSize),0.0,1.0)),1.0);
}
