// BEGIN SHARED LIGHTING
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

#define CUBEUV_MAX_MIP 8.0
#define CUBEUV_TEXEL_WIDTH (1.0/768.0)
#define CUBEUV_TEXEL_HEIGHT (1.0/1024.0)



	#define cubeUV_minMipLevel 4.0
	#define cubeUV_minTileSize 16.0

	// These shader functions convert between the UV coordinates of a single face of
	// a cubemap, the 0-5 integer index of a cube face, and the direction vector for
	// sampling a textureCube (not generally normalized ).

	float getFace( vec3 direction ) {

		vec3 absDirection = abs( direction );

		float face = - 1.0;

		if ( absDirection.x > absDirection.z ) {

			if ( absDirection.x > absDirection.y )

				face = direction.x > 0.0 ? 0.0 : 3.0;

			else

				face = direction.y > 0.0 ? 1.0 : 4.0;

		} else {

			if ( absDirection.z > absDirection.y )

				face = direction.z > 0.0 ? 2.0 : 5.0;

			else

				face = direction.y > 0.0 ? 1.0 : 4.0;

		}

		return face;

	}

	// RH coordinate system; PMREM face-indexing convention
	vec2 getUV( vec3 direction, float face ) {

		vec2 uv;

		if ( face == 0.0 ) {

			uv = vec2( direction.z, direction.y ) / abs( direction.x ); // pos x

		} else if ( face == 1.0 ) {

			uv = vec2( - direction.x, - direction.z ) / abs( direction.y ); // pos y

		} else if ( face == 2.0 ) {

			uv = vec2( - direction.x, direction.y ) / abs( direction.z ); // pos z

		} else if ( face == 3.0 ) {

			uv = vec2( - direction.z, direction.y ) / abs( direction.x ); // neg x

		} else if ( face == 4.0 ) {

			uv = vec2( - direction.x, direction.z ) / abs( direction.y ); // neg y

		} else {

			uv = vec2( direction.x, direction.y ) / abs( direction.z ); // neg z

		}

		return 0.5 * ( uv + 1.0 );

	}

	vec3 bilinearCubeUV( sampler2D envMap, vec3 direction, float mipInt ) {

		float face = getFace( direction );

		float filterInt = max( cubeUV_minMipLevel - mipInt, 0.0 );

		mipInt = max( mipInt, cubeUV_minMipLevel );

		float faceSize = exp2( mipInt );

		vec2 uv = getUV( direction, face ) * ( faceSize - 2.0 ) + 1.0; // #25071

		if ( face > 2.0 ) {

			uv.y += faceSize;

			face -= 3.0;

		}

		uv.x += face * faceSize;

		uv.x += filterInt * 3.0 * cubeUV_minTileSize;

		uv.y += 4.0 * ( exp2( CUBEUV_MAX_MIP ) - faceSize );

		uv.x *= CUBEUV_TEXEL_WIDTH;
		uv.y *= CUBEUV_TEXEL_HEIGHT;

		#ifdef texture2DGradEXT

			return texture2DGradEXT( envMap, uv, vec2( 0.0 ), vec2( 0.0 ) ).rgb; // disable anisotropic filtering

		#else

			return textureLod( envMap, uv, 0.0 ).rgb;

		#endif

	}

	// These defines must match with PMREMGenerator

	#define cubeUV_r0 1.0
	#define cubeUV_m0 - 2.0
	#define cubeUV_r1 0.8
	#define cubeUV_m1 - 1.0
	#define cubeUV_r4 0.4
	#define cubeUV_m4 2.0
	#define cubeUV_r5 0.305
	#define cubeUV_m5 3.0
	#define cubeUV_r6 0.21
	#define cubeUV_m6 4.0

	float roughnessToMip( float roughness ) {

		float mip = 0.0;

		if ( roughness >= cubeUV_r1 ) {

			mip = ( cubeUV_r0 - roughness ) * ( cubeUV_m1 - cubeUV_m0 ) / ( cubeUV_r0 - cubeUV_r1 ) + cubeUV_m0;

		} else if ( roughness >= cubeUV_r4 ) {

			mip = ( cubeUV_r1 - roughness ) * ( cubeUV_m4 - cubeUV_m1 ) / ( cubeUV_r1 - cubeUV_r4 ) + cubeUV_m1;

		} else if ( roughness >= cubeUV_r5 ) {

			mip = ( cubeUV_r4 - roughness ) * ( cubeUV_m5 - cubeUV_m4 ) / ( cubeUV_r4 - cubeUV_r5 ) + cubeUV_m4;

		} else if ( roughness >= cubeUV_r6 ) {

			mip = ( cubeUV_r5 - roughness ) * ( cubeUV_m6 - cubeUV_m5 ) / ( cubeUV_r5 - cubeUV_r6 ) + cubeUV_m5;

		} else {

			mip = - 2.0 * log2( 1.16 * roughness ); // 1.16 = 1.79^0.25
		}

		return mip;

	}

	vec4 textureCubeUV( sampler2D envMap, vec3 sampleDir, float roughness ) {

		float mip = clamp( roughnessToMip( roughness ), cubeUV_m0, CUBEUV_MAX_MIP );

		float mipF = fract( mip );

		float mipInt = floor( mip );

		vec3 color0 = bilinearCubeUV( envMap, sampleDir, mipInt );

		if ( mipF == 0.0 ) {

			return vec4( color0, 1.0 );

		} else {

			vec3 color1 = bilinearCubeUV( envMap, sampleDir, mipInt + 1.0 );

			return vec4( mix( color0, color1, mipF ), 1.0 );

		}

	}

// END SHARED LIGHTING
// Matte print: contain + 1/64 transparent inset, no emission or tone mapping.
VARYING vec2 paperUv;
VARYING vec3 lightOrigin;
VARYING vec3 printWorld;
VARYING vec3 printNormal;
vec3 srgbToLinear(vec3 c) {
    return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
}
vec3 printColor;
float contactShadow;
void MAIN() {
    contactShadow=1.0;
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
// This light supplies only its public shadow multiplier; source radiometry stays in musicIrradiance.
void DIRECTIONAL_LIGHT() { contactShadow=SHADOW_CONTRIB; }
void POST_PROCESS() {
    vec3 irradiance=lightingEnabled?(musicIrradiance(printWorld,normalize(printNormal))+textureCubeUV(environmentMap,normalize(printNormal),1.0).rgb*environmentIntensity):paperIrradiance;
    if(lightingEnabled)irradiance-=keyColor*lightIntensities.x*max(0.0,dot(normalize(printNormal),normalize(vec3(-6,14,-5))))*(1.0-contactShadow)/3.141592653589793;
    COLOR_SUM.rgb=min(printColor*max(irradiance,vec3(0.0)),printColor);
    if(lightingEnabled)COLOR_SUM.rgb*=musicPrintExposure(lightOrigin);
}
