vec3 linearToSrgb(vec3 c)
{
    return mix(c * 12.92, 1.055 * pow(max(c, vec3(0)), vec3(1.0 / 2.4)) - .055,
               step(vec3(.0031308), c));
}
// Frozen Three ACESFilm, shared with the verified R07 output.
vec3 aces(vec3 c, float exposure)
{
    const mat3 inputMat = mat3(vec3(.59719, .07600, .02840), vec3(.35458, .90834, .13383),
                               vec3(.04823, .01566, .83777));
    const mat3 outputMat = mat3(vec3(1.60475, -.10208, -.00327), vec3(-.53108, 1.10813, -.07276),
                                vec3(-.07367, -.00605, 1.07602));
    c = inputMat * (c * exposure / .6);
    c = (c * (c + .0245786) - .000090537) / (c * (.983729 * c + .4329510) + .238081);
    return clamp(outputMat * c, 0.0, 1.0);
}
