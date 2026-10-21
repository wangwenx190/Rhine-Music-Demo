// Ordered paired Gaussian accumulation; the caller supplies its authored kernel and sampler.
void main()
{
    vec4 color = sampleBackdrop(qt_TexCoord0) * GaussianCenterWeight;
    for (int i = 0; i < GaussianPairs.length(); ++i) {
        color += (sampleBackdrop(qt_TexCoord0 + sampleStep * GaussianPairs[i].x)
                  + sampleBackdrop(qt_TexCoord0 - sampleStep * GaussianPairs[i].x))
                * GaussianPairs[i].y;
    }
    fragColor = color * qt_Opacity;
}
