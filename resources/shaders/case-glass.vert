VARYING vec3 localPosition;
VARYING vec3 worldPosition;
VARYING vec3 worldNormal;
VARYING vec3 lightOrigin;
void MAIN() {
    localPosition=VERTEX;
    lightOrigin=(MODEL_MATRIX*vec4(0,0,0,1)).xyz;
    worldPosition=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;
    worldNormal=normalize(NORMAL_MATRIX*NORMAL);
    POSITION=MODELVIEWPROJECTION_MATRIX*vec4(VERTEX,1.0);
}
