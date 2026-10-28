// Standard transfer/ACES fit constants retain their source precision and operation order.
const float SrgbLinearSlope = 12.92;
const float SrgbScale = 1.055;
const float SrgbExponent = 2.4;

vec3 linearToSrgb(vec3 c)
{
    return mix(SrgbLinearSlope * c,
               SrgbScale * pow(max(c, vec3(0.0)), vec3(1.0 / SrgbExponent)) - 0.055,
               step(vec3(0.0031308), c));
}
void MAIN()
{
    vec4 c = texture(INPUT, INPUT_UV);
    FRAGCOLOR = vec4(linearToSrgb(c.rgb), c.a);
}
