VARYING vec3 localPosition;
VARYING vec3 worldPosition;
VARYING vec3 worldNormal;
void MAIN() {
    localPosition=VERTEX;
    worldPosition=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;
    worldNormal=normalize(NORMAL_MATRIX*NORMAL);
    POSITION=MODELVIEWPROJECTION_MATRIX*vec4(VERTEX,1.0);
}
