// Three fog_fragment: both transmission capture and composer render in linear sRGB.
// Fog follows material shading, before postprocessing and the single OutputPass.
vec3 sceneFog(vec3 radiance, float viewDepth)
{
    if (f.fog.z < .5)
        return radiance;
    return mix(radiance, f.background.rgb, smoothstep(f.fog.x, f.fog.y, viewDepth));
}
