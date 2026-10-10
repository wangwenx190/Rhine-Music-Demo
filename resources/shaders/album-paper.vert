VARYING vec2 paperUv;
VARYING vec3 lightOrigin;
VARYING vec3 printWorld;
VARYING vec3 printNormal;
void MAIN() {
    paperUv=UV0;
    lightOrigin=(MODEL_MATRIX*vec4(-17.0/3.06,-185.0/3.06,-.027,1.0)).xyz;
    printWorld=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;
    printNormal=normalize(NORMAL_MATRIX*NORMAL);
    POSITION=MODELVIEWPROJECTION_MATRIX*vec4(VERTEX,1.0);
}
