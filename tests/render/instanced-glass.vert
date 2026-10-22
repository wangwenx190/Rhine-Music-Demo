// R10 instanced variant of resources/shaders/case-glass.vert; optical math unchanged.
VARYING vec3 localPosition;
VARYING vec3 worldPosition;
VARYING vec3 worldNormal;
VARYING vec3 lightOrigin;
void MAIN()
{
    localPosition = VERTEX;
    lightOrigin = (INSTANCE_MODEL_MATRIX * vec4(0, 0, 0, 1)).xyz;
    worldPosition = (INSTANCE_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    worldNormal = normalize(transpose(inverse(mat3(INSTANCE_MODEL_MATRIX))) * NORMAL);
    POSITION = INSTANCE_MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
