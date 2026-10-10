// Three.js ShaderChunk derivation (MIT; resources/licenses/three-LICENSE).
VARYING vec3 localPosition;
VARYING vec3 worldPosition;
VARYING vec3 worldNormal;
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

float w0( float a ) {

		return ( 1.0 / 6.0 ) * ( a * ( a * ( - a + 3.0 ) - 3.0 ) + 1.0 );

	}

	float w1( float a ) {

		return ( 1.0 / 6.0 ) * ( a *  a * ( 3.0 * a - 6.0 ) + 4.0 );

	}

	float w2( float a ){

		return ( 1.0 / 6.0 ) * ( a * ( a * ( - 3.0 * a + 3.0 ) + 3.0 ) + 1.0 );

	}

	float w3( float a ) {

		return ( 1.0 / 6.0 ) * ( a * a * a );

	}

	// g0 and g1 are the two amplitude functions
	float g0( float a ) {

		return w0( a ) + w1( a );

	}

	float g1( float a ) {

		return w2( a ) + w3( a );

	}

	// h0 and h1 are the two offset functions
	float h0( float a ) {

		return - 1.0 + w1( a ) / ( w0( a ) + w1( a ) );

	}

	float h1( float a ) {

		return 1.0 + w3( a ) / ( w2( a ) + w3( a ) );

	}

	vec4 bicubic( sampler2D tex, vec2 uv, vec4 texelSize, float lod ) {

		uv = uv * texelSize.zw + 0.5;

		vec2 iuv = floor( uv );
		vec2 fuv = fract( uv );

		float g0x = g0( fuv.x );
		float g1x = g1( fuv.x );
		float h0x = h0( fuv.x );
		float h1x = h1( fuv.x );
		float h0y = h0( fuv.y );
		float h1y = h1( fuv.y );

		vec2 p0 = ( vec2( iuv.x + h0x, iuv.y + h0y ) - 0.5 ) * texelSize.xy;
		vec2 p1 = ( vec2( iuv.x + h1x, iuv.y + h0y ) - 0.5 ) * texelSize.xy;
		vec2 p2 = ( vec2( iuv.x + h0x, iuv.y + h1y ) - 0.5 ) * texelSize.xy;
		vec2 p3 = ( vec2( iuv.x + h1x, iuv.y + h1y ) - 0.5 ) * texelSize.xy;

		return g0( fuv.y ) * ( g0x * textureLod( tex, p0, lod ) + g1x * textureLod( tex, p1, lod ) ) +
			g1( fuv.y ) * ( g0x * textureLod( tex, p2, lod ) + g1x * textureLod( tex, p3, lod ) );

	}

	vec4 textureBicubic( sampler2D texSampler, vec2 uv, float lod ) {

		lod = clamp(lod, 0.0, floor(log2(float(max(textureSize(texSampler, 0).x, textureSize(texSampler, 0).y)))));
		vec2 fLodSize = vec2( textureSize( texSampler, int( lod ) ) );
		vec2 cLodSize = vec2( textureSize( texSampler, int( ceil(lod) ) ) );
		vec2 fLodSizeInv = 1.0 / fLodSize;
		vec2 cLodSizeInv = 1.0 / cLodSize;
		vec4 fSample = bicubic( texSampler, uv, vec4( fLodSizeInv, fLodSize ), floor( lod ) );
		vec4 cSample = bicubic( texSampler, uv, vec4( cLodSizeInv, cLodSize ), ceil( lod ) );
		return mix( fSample, cSample, fract( lod ) );

	}

	vec3 RRTAndODTFit( vec3 v ) {

	vec3 a = v * ( v + 0.0245786 ) - 0.000090537;
	vec3 b = v * ( 0.983729 * v + 0.4329510 ) + 0.238081;
	return a / b;

}

// this implementation of ACES is modified to accommodate a brighter viewing environment.
// the scale factor of 1/0.6 is subjective. see discussion in #19621.

vec3 ACESFilmicToneMapping( vec3 color ) {

	// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
	const mat3 ACESInputMat = mat3(
		vec3( 0.59719, 0.07600, 0.02840 ), // transposed from source
		vec3( 0.35458, 0.90834, 0.13383 ),
		vec3( 0.04823, 0.01566, 0.83777 )
	);

	// ODT_SAT => XYZ => D60_2_D65 => sRGB
	const mat3 ACESOutputMat = mat3(
		vec3(  1.60475, -0.10208, -0.00327 ), // transposed from source
		vec3( -0.53108,  1.10813, -0.07276 ),
		vec3( -0.07367, -0.00605,  1.07602 )
	);

	color *= exposure / 0.6;

	color = ACESInputMat * color;

	// Apply RRT and ODT
	color = RRTAndODTFit( color );

	color = ACESOutputMat * color;

	// Clamp to [0, 1]
	return clamp(color,0.0,1.0);

}


vec3 environmentBRDF(float dotNV, float r, vec3 f0) {
    vec2 fab = texture(dfgMap, vec2(r, dotNV)).rg;
    return f0*fab.x + fab.y;
}
void MAIN() {
    vec3 n = normalize(worldNormal);
    vec3 v = normalize(CAMERA_POSITION-worldPosition);
    float nv = max(0.0,dot(n,v));
    vec3 tint = mix(nightTint.rgb, dayTint.rgb, warmth);
    vec3 attenuation = mix(nightAttenuation.rgb, dayAttenuation.rgb, warmth);
    vec3 f0=vec3(pow((1.46-1.0)/(1.46+1.0),2.0));
    vec3 dxy=max(abs(dFdx(n)),abs(dFdy(n)));
    float geometryRoughness=max(dxy.x,max(dxy.y,dxy.z));
    float r=min(1.0,max(roughness,0.0525)+geometryRoughness);
    float cr=min(1.0,max(coatRoughness,0.0525)+geometryRoughness);
    vec3 reflectDir=normalize(mix(reflect(-v,n),n,pow(r,4.0)));
    vec3 irradiance=textureCubeUV(environmentMap,n,1.0).rgb*environmentIntensity;
    vec3 radiance=textureCubeUV(environmentMap,reflectDir,r).rgb*environmentIntensity;
    vec2 fab=texture(dfgMap,vec2(r,nv)).rg;
    vec3 singleScatter=f0*fab.x+fab.y;
    float ems=1.0-fab.x-fab.y;
    vec3 favg=f0+(1.0-f0)*0.047619;
    vec3 multiScatter=singleScatter*favg/(1.0-ems*favg)*ems;
    vec3 diffuse=tint*(1.0-singleScatter-multiScatter)*irradiance;
    // Fixed diffuse input; the world-space light field is integrated in R08.
    diffuse+=tint*paperIrradiance;
    vec3 specular=radiance*singleScatter+multiScatter*irradiance;
    vec3 ray=normalize(refract(-v,n,1.0/1.46))*thickness;
    vec4 projected=VIEWPROJECTION_MATRIX*vec4(worldPosition+ray,1.0);
    vec4 here=VIEWPROJECTION_MATRIX*vec4(worldPosition,1.0);
    // Native projection and FRAGCOORD share backend-corrected framebuffer axes.
    vec2 uv=FRAGCOORD.xy/vec2(textureSize(SCREEN_MIP_TEXTURE,0))+(projected.xy/projected.w-here.xy/here.w)*0.5;
    vec2 edge=abs(localPosition.xy-vec2(0.17,1.85))-vec2(1.53);
    float proximity=1.0-smoothstep(0.015,0.15,max(edge.x,edge.y));
    float zoned=surface==0?mix(1.0,0.16,proximity):1.0;
    // Quick3D generates the full mip chain: derive its last level from the actual capture dimensions.
    float maxLod=floor(log2(float(max(textureSize(SCREEN_MIP_TEXTURE,0).x,textureSize(SCREEN_MIP_TEXTURE,0).y))));
    float lod=clamp(log2(float(textureSize(SCREEN_MIP_TEXTURE,0).x))*r*0.92*zoned,0.0,maxLod);
    vec3 sampleColor=textureBicubic(SCREEN_MIP_TEXTURE,clamp(uv,vec2(0.0),vec2(1.0)),lod).rgb;
    vec3 transmittance=tint*exp(log(attenuation)*length(ray)/attenuationDistance);
    vec3 transmitted=(1.0-environmentBRDF(nv,r,f0))*transmittance*sampleColor;
    vec3 outgoing=mix(diffuse,transmitted,transmission)+specular;
    vec3 coatDir=normalize(mix(reflect(-v,n),n,pow(cr,4.0)));
    vec3 coat=textureCubeUV(environmentMap,coatDir,cr).rgb*environmentIntensity*environmentBRDF(nv,cr,vec3(0.04));
    float fresnel=0.04+0.96*exp2((-5.55473*nv-6.98316)*nv);
    outgoing=outgoing*(1.0-clearcoat*fresnel)+clearcoat*coat;
    FRAGCOLOR=vec4(ACESFilmicToneMapping(outgoing),1.0);
}
