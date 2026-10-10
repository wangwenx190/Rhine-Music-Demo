
vec3 musicBRDF(vec3 n,vec3 v,vec3 l,float r,vec3 f0,bool multiscatter) {
    vec3 h=normalize(l+v);
    float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.0),nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
    float a=r*r,aa=a*a;
    float gv=nl*sqrt(aa+(1.0-aa)*nv*nv),gl=nv*sqrt(aa+(1.0-aa)*nl*nl);
    float visibility=.5/max(gv+gl,1e-6),d=nh*nh*(aa-1.0)+1.0;
    float distribution=aa/(3.141592653589793*d*d);
    vec3 fresnel=f0+(1.0-f0)*exp2((-5.55473*vh-6.98316)*vh);
    vec3 result=fresnel*visibility*distribution;
    if(multiscatter){
        vec2 fv=texture(dfgMap,vec2(r,nv)).rg,fl=texture(dfgMap,vec2(r,nl)).rg;
        vec3 sv=f0*fv.x+fv.y,sl=f0*fl.x+fl.y,average=f0+(1.0-f0)*.047619;
        float compensation=(1.0-fv.x-fv.y)*(1.0-fl.x-fl.y);
        result+=sv*sl*average/(1.0-compensation*average+1e-6)*compensation;
    }
    return result*nl;
}
vec3 musicSpecular(vec3 n,vec3 v,vec3 position,float r,vec3 f0,bool multiscatter) {
    return keyColor*lightIntensities.x*musicBRDF(n,v,normalize(vec3(-6,14,-5)),r,f0,multiscatter)
        +vec3(lightIntensities.z)*musicBRDF(n,v,normalize(vec3(7,8,-10)),r,f0,multiscatter)
        +musicSpot(position)*musicBRDF(n,v,normalize(spotPosition-position),r,f0,multiscatter);
}
