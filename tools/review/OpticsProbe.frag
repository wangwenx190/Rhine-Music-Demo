// Appended to the generated Review-only copy of the production shell shader.
// This executes the same functions with exact mathematical inputs, not reference images.
void main()
{
    vec3 result;
    int probe = int(f.paperSize.w);
    if (probe == 10) {
        productionShellMain();
        color = any(isnan(color)) || any(isinf(color)) ? vec4(0, 1, 0, 1) : vec4(1);
        return;
    } else if (probe == 1) {
        vec3 rect[4];
        rect[0] = vec3(1, -1, 1);
        rect[1] = vec3(-1, -1, 1);
        rect[2] = vec3(-1, 1, 1);
        rect[3] = vec3(1, 1, 1);
        result = LTC_Evaluate(vec3(0, 0, 1), vec3(0, 0, 1), vec3(0), mat3(1), rect);
    } else if (probe == 2) {
        result = musicBRDF(vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, -1), .4, vec3(.04), true);
    } else if (probe == 5 || probe == 6) {
        keyShadow = probe == 5 ? 0.0 : 1.0;
        vec3 n = normalize(vec3(-6, 14, -5));
        result = musicSpecular(n, n, vec3(0), .4, vec3(.04), true);
    } else if (probe == 7 || probe == 8) {
        keyShadow = probe == 7 ? 0.0 : 1.0;
        result = musicIrradiance(vec3(0), normalize(vec3(-6, 14, -5)));
    } else if (probe == 9) {
        vec2 delta = gl_FragCoord.xy - vec2(480.5, 360.5);
        result = vec3(32.0 * musicGeometryRoughness(normalize(vec3(delta * .01, 1))));
    } else {
        result = musicBRDF(vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1), .4, vec3(.04), probe == 4);
    }
    color = vec4(result, 1);
    normalOut = vec4(0, 0, 1, 1);
    depthOut = vec4(1);
}
