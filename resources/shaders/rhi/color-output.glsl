// Standard transfer/ACES fit constants retain their source precision and operation order.
const float SrgbLinearSlope = 12.92;
const float SrgbOffset = .055;
const float SrgbScale = 1.055;
const float SrgbExponent = 2.4;
const float SrgbEncodeKnee = .0031308;
const float AcesExposureNormalization = .6;
const float AcesNumeratorLinear = .0245786;
const float AcesNumeratorBias = .000090537;
const float AcesDenominatorQuadratic = .983729;
const float AcesDenominatorLinear = .4329510;
const float AcesDenominatorBias = .238081;

vec3 linearToSrgb(vec3 c)
{
    return mix(c * SrgbLinearSlope,
               SrgbScale * pow(max(c, vec3(0)), vec3(1.0 / SrgbExponent)) - SrgbOffset,
               step(vec3(SrgbEncodeKnee), c));
}
// Frozen Three ACESFilm, shared with the verified R07 output.
vec3 aces(vec3 c, float exposure)
{
    const mat3 inputMat = mat3(vec3(.59719, .07600, .02840), vec3(.35458, .90834, .13383),
                               vec3(.04823, .01566, .83777));
    const mat3 outputMat = mat3(vec3(1.60475, -.10208, -.00327), vec3(-.53108, 1.10813, -.07276),
                                vec3(-.07367, -.00605, 1.07602));
    c = inputMat * (c * exposure / AcesExposureNormalization);
    c = (c * (c + AcesNumeratorLinear) - AcesNumeratorBias)
            / (c * (AcesDenominatorQuadratic * c + AcesDenominatorLinear) + AcesDenominatorBias);
    return clamp(outputMat * c, 0.0, 1.0);
}
