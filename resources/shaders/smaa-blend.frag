// Derived from locked Three.js (MIT); resources/licenses/three-LICENSE.
#define resolution (1.0/INPUT_SIZE)
vec2 frameUv(vec2 uv) { return FRAMEBUFFER_Y_UP>0.0?uv:vec2(uv.x,1.0-uv.y); }
vec4 readFrame(sampler2D tex,vec2 uv) { return texture(tex,frameUv(uv)); }
vec4 readFrame(sampler2D tex,vec2 uv,float bias) { return textureLod(tex,frameUv(uv),0.0); }
vec2 vUv;
vec4 vOffset[ 2 ];
void SMAANeighborhoodBlendingVS( vec2 texcoord ) {
			vOffset[ 0 ] = texcoord.xyxy + resolution.xyxy * vec4( -1.0, 0.0, 0.0, 1.0 ); // WebGL port note: Changed sign in W component
			vOffset[ 1 ] = texcoord.xyxy + resolution.xyxy * vec4( 1.0, 0.0, 0.0, -1.0 ); // WebGL port note: Changed sign in W component
		}



		vec4 SMAANeighborhoodBlendingPS( vec2 texcoord, vec4 offset[ 2 ], sampler2D colorTex, sampler2D blendTex ) {
			// Fetch the blending weights for current pixel:
			vec4 a;
			a.xz = readFrame( blendTex, texcoord ).xz;
			a.y = readFrame( blendTex, offset[ 1 ].zw ).g;
			a.w = readFrame( blendTex, offset[ 1 ].xy ).a;

			// Is there any blending weight with a value greater than 0.0?
			if ( dot(a, vec4( 1.0, 1.0, 1.0, 1.0 )) < 1e-5 ) {
				return readFrame( colorTex, texcoord, 0.0 );
			} else {
				// Up to 4 lines can be crossing a pixel (one through each edge). We
				// favor blending by choosing the line with the maximum weight for each
				// direction:
				vec2 chosenOffset;
				chosenOffset.x = a.a > a.b ? a.a : -a.b; // left vs. right
				chosenOffset.y = a.g > a.r ? -a.g : a.r; // top vs. bottom // WebGL port note: Changed signs

				// Then we go in the direction that has the maximum weight:
				if ( abs( chosenOffset.x ) > abs( chosenOffset.y )) { // horizontal vs. vertical
					chosenOffset.y = 0.0;
				} else {
					chosenOffset.x = 0.0;
				}

				// Fetch the opposite color and lerp by hand:
				vec4 C = readFrame( colorTex, texcoord, 0.0 );
				texcoord += sign( chosenOffset ) * resolution;
				vec4 Cop = readFrame( colorTex, texcoord, 0.0 );
				float s = abs( chosenOffset.x ) > abs( chosenOffset.y ) ? abs( chosenOffset.x ) : abs( chosenOffset.y );

				// WebGL port note: Added gamma correction
				C.xyz = pow(C.xyz, vec3(2.2));
				Cop.xyz = pow(Cop.xyz, vec3(2.2));
				vec4 mixed = mix(C, Cop, s);
				mixed.xyz = pow(mixed.xyz, vec3(1.0 / 2.2));

				return mixed;
			}
		}

		void MAIN() {
 vUv=frameUv(INPUT_UV);
 SMAANeighborhoodBlendingVS(vUv);

			FRAGCOLOR = SMAANeighborhoodBlendingPS( vUv, vOffset, INPUT, weightMap );

		}
