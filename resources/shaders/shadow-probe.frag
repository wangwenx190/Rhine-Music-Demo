// Diagnostic receiver: shows only the public shadow multiplier, not radiometry.
void MAIN() { BASE_COLOR=vec4(1.0); ROUGHNESS=1.0; }
void AMBIENT_LIGHT() {}
void DIRECTIONAL_LIGHT() { DIFFUSE+=vec3(SHADOW_CONTRIB); }
void POST_PROCESS() { COLOR_SUM=vec4(DIFFUSE.rgb,1.0); }
