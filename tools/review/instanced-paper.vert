// R10 instanced variant of resources/shaders/album-paper.vert; optical math unchanged.
VARYING vec2 paperUv;
VARYING vec3 lightOrigin;
VARYING vec3 printWorld;
VARYING vec3 printNormal;
void MAIN()
{
    paperUv = UV0;
    lightOrigin = (INSTANCE_MODEL_MATRIX * vec4(-17.0 / 3.06, -185.0 / 3.06, -.027, 1.0)).xyz;
    printWorld = (INSTANCE_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    printNormal = normalize(transpose(inverse(mat3(INSTANCE_MODEL_MATRIX))) * NORMAL);
    POSITION = INSTANCE_MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
