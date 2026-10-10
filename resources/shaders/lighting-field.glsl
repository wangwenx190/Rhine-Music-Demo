// Shared field, diffuse and shell transport from frozen music-lighting.ts (MIT).
float musicRowField(vec3 origin) {
    float row=(origin.z-rowColumn.z-rowSettings.w*.62)/(.5*rowSettings.z*.62);
    float lane=(origin.x-rowColumn.x)/27.0;
    float lane2=lane*lane;
    return exp(-0.6931471805599453*row*row)*exp(-lane2*lane2*lane2);
}
float musicPrintExposure(vec3 origin) {
    vec3 distance=(origin-lightColumn)/vec3(6.0,3.35,5.5);
    float printLight=exp(-dot(distance,distance));
    float baseline=mix(printAmbient,1.0,printLight);
    if(rowSettings.x<.5)return baseline;
    float ambient=max(.08,printAmbient*.5);
    float fillAmount=rowSettings.x>1.5&&rowSettings.x<2.5?rowFill:0.0;
    float fieldAmbient=mix(ambient,baseline,fillAmount);
    float fieldExposure=mix(fieldAmbient,1.0,clamp(rowSettings.y*musicRowField(origin),0.0,1.0));
    float areaExposure=mix(ambient,printAmbient,.25);
    float shelf=rowSettings.x<1.5?areaExposure:fieldExposure;
    return mix(shelf,baseline,rowDetail);
}
vec3 musicSpot(vec3 position) {
    vec3 delta=spotPosition-position;
    float distanceSquared=max(dot(delta,delta),.01);
    float angle=dot(normalize(delta),normalize(spotPosition-spotTarget));
    float attenuation=smoothstep(cos(.32),cos(.32*(1.0-.95)),angle)/distanceSquared;
    return spotColor*spotIntensity*attenuation;
}
vec3 musicIrradiance(vec3 position,vec3 normal) {
    vec3 hemi=mix(groundColor,skyColor,.5*normal.y+.5)*lightIntensities.y;
    vec3 key=keyColor*lightIntensities.x*max(0.0,dot(normal,normalize(vec3(-6,14,-5))));
    vec3 fill=vec3(lightIntensities.z*max(0.0,dot(normal,normalize(vec3(7,8,-10)))));
    vec3 spot=musicSpot(position)*max(0.0,dot(normal,normalize(spotPosition-position)));
    return (hemi+key+fill+spot)/3.141592653589793;
}
vec3 musicShell(vec3 original,vec3 origin,vec3 local,vec3 normal,vec3 view,int surface) {
    float laneDistance=abs(origin.x-lightColumn.x);
    float core=exp(-pow(laneDistance/3.2,4.0));
    float spill=exp(-pow(laneDistance/7.2,4.0));
    float laneLight=.58*core+.42*spill;
    float rowLight=exp(-pow((origin.z-lightColumn.z)/5.5,2.0));
    float hot=exp(-pow((origin.z-lightColumn.z)/1.1,2.0));
    float neighbor=exp(-pow((origin.z-lightColumn.z)/2.4,2.0));
    float guided=.58*core*mix(.12,1.0,rowLight)+.42*spill*neighbor;
    float edgeTransport=exp(-max(0.0,local.x+2.225)*1.7);
    float panelHeight=clamp((local.y-.175)/3.35,0.0,1.0);
    float topRim=exp(-max(0.0,3.525-local.y)*24.0);
    float grazing=1.0-clamp(abs(dot(normal,view)),0.0,1.0);
    bool spine=surface==1;
    vec3 selected=original*mix(.96,1.04,guided);
    if(surface<2) {
        float edgeScatter=(spine?.30:.14)*edgeTransport+(spine?.016:.003);
        selected+=scatterColor*scatterStrength*guided*edgeScatter*mix(1.0,.62,panelHeight);
        float ribbon=topRim*(.22+.78*edgeTransport)+(spine?.18:.035)*edgeTransport*grazing;
        selected+=scatterColor*scatterStrength*laneLight*hot*ribbon*.8;
    }
    if(rowSettings.x<.5)return selected;
    float gain=rowSettings.x>1.5?musicRowField(origin)*rowSettings.y:0.0;
    float fillAmount=rowSettings.x>1.5&&rowSettings.x<2.5?rowFill:0.0;
    vec3 shelf=original*mix(mix(.56,mix(.96,1.04,guided),fillAmount),1.08,clamp(gain,0.0,1.0));
    if(surface<2) {
        float panel=(spine?.30:.12)*edgeTransport+(spine?.05:.025);
        float rim=topRim*(.55+.45*edgeTransport)+(spine?.24:.07)*edgeTransport*grazing;
        float scatter=panel*mix(1.0,.78,panelHeight)+rim*1.25;
        shelf+=scatterColor*scatterStrength*gain*scatter*(rowSettings.x>2.5?.86:1.0);
    }
    return mix(shelf,selected,rowDetail);
}
