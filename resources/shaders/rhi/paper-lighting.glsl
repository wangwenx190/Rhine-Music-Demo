// BEGIN SHARED LIGHTING
// Shared field and diffuse transport from frozen music-lighting.ts (MIT).
float musicRowField(vec3 origin) {
    float row=(origin.z-f.rowColumn.xyz.z-f.rowSettings.w*.62)/(.5*f.rowSettings.z*.62);
    float lane=(origin.x-f.rowColumn.xyz.x)/27.0;
    float lane2=lane*lane;
    return exp(-0.6931471805599453*row*row)*exp(-lane2*lane2*lane2);
}
float musicPrintExposure(vec3 origin) {
    vec3 distance=(origin-f.lightColumn.xyz)/vec3(6.0,3.35,5.5);
    float printLight=exp(-dot(distance,distance));
    float baseline=mix(f.settings.x,1.0,printLight);
    if(f.rowSettings.x<.5)return baseline;
    float ambient=max(.08,f.settings.x*.5);
    float fillAmount=f.rowSettings.x>1.5&&f.rowSettings.x<2.5?f.settings.z:0.0;
    float fieldAmbient=mix(ambient,baseline,fillAmount);
    float fieldExposure=mix(fieldAmbient,1.0,clamp(f.rowSettings.y*musicRowField(origin),0.0,1.0));
    float areaExposure=mix(ambient,f.settings.x,.25);
    float shelf=f.rowSettings.x<1.5?areaExposure:fieldExposure;
    return mix(shelf,baseline,f.settings.y);
}
vec3 musicSpot(vec3 position) {
    vec3 delta=f.spotPosition.xyz-position;
    float distanceSquared=max(dot(delta,delta),.01);
    float angle=dot(normalize(delta),normalize(f.spotPosition.xyz-f.spotTarget.xyz));
    float attenuation=smoothstep(cos(.32),cos(.32*(1.0-.95)),angle)/distanceSquared;
    return f.spotColor.xyz*f.optics.x*attenuation;
}
vec3 musicIrradiance(vec3 position,vec3 normal) {
    vec3 hemi=mix(f.groundColor.xyz,f.skyColor.xyz,.5*normal.y+.5)*f.lightIntensities.xyz.y;
    vec3 key=f.keyColor.xyz*f.lightIntensities.xyz.x*max(0.0,dot(normal,normalize(vec3(-6,14,-5))));
    vec3 fill=vec3(f.lightIntensities.xyz.z*max(0.0,dot(normal,normalize(vec3(7,8,-10)))));
    vec3 spot=musicSpot(position)*max(0.0,dot(normal,normalize(f.spotPosition.xyz-position)));
    return (hemi+key+fill+spot)/3.141592653589793;
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
