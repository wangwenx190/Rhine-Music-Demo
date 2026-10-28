// Defines the frame.glsl ABI and material selectors shared by GPU uploads and captures.
// Packing is explicit and checked at compile time; these records own no native resources.

#pragma once
#include "RenderData.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <cstring>

namespace yalahoo::rhi {
// Draw kinds are the production frame.glsl material selectors, including the shadow sentinel.
enum DrawKind {
    ShadowKind = -1,
    PaperKind,
    PreviewPaperKind,
    ReviewFloorKind,
    LidKind,
    SpineKind,
    TrayKind,
    ProductFloorKind
};
constexpr int InstanceMatrixSecondOffsetBytes = 32;
constexpr int InstanceMatrixThirdOffsetBytes = 48;
constexpr int InstanceUvOffsetBytes = 64;
constexpr int InstanceLightOffsetBytes = 80;
constexpr int InstanceSecondaryLightOffsetBytes = 96;
constexpr int VectorScalars = 4;
constexpr int MatrixScalars = 16;
constexpr int UniformBytes = 832;
constexpr int BackgroundOffsetBytes = 320;
constexpr int OpticsOffsetBytes = 544;
constexpr int NativeVpOffsetBytes = 560;
constexpr int ScatterOffsetBytes = 736;
constexpr int FogOffsetBytes = 816;
constexpr int UniformBaseCount = 8;
// Exactly matches frame.glsl; avoid compiler-dependent QVector3D/std140 packing.
struct alignas(renderData::VectorBytes) Uniform final
{
    float mvp[MatrixScalars], model[MatrixScalars], view[MatrixScalars], lightMvp[MatrixScalars],
            lightVP[MatrixScalars];
    float background[VectorScalars], rowSettings[VectorScalars], rowColumn[VectorScalars],
            lightColumn[VectorScalars], spotPosition[VectorScalars], spotTarget[VectorScalars],
            spotColor[VectorScalars];
    float keyColor[VectorScalars], skyColor[VectorScalars], groundColor[VectorScalars],
            lightIntensities[VectorScalars], settings[VectorScalars], clip[VectorScalars],
            draw[VectorScalars], optics[VectorScalars];
    float nativeVP[MatrixScalars], camera[VectorScalars], tint[VectorScalars],
            attenuation[VectorScalars], material[VectorScalars], coat[VectorScalars],
            areaPosition[VectorScalars], areaTarget[VectorScalars], scatter[VectorScalars],
            coverUv[VectorScalars], paperSize[VectorScalars], originLight0[VectorScalars],
            originLight1[VectorScalars], fog[VectorScalars];
};
static_assert(sizeof(Uniform) == UniformBytes
              && offsetof(Uniform, background) == BackgroundOffsetBytes
              && offsetof(Uniform, optics) == OpticsOffsetBytes
              && offsetof(Uniform, nativeVP) == NativeVpOffsetBytes
              && offsetof(Uniform, scatter) == ScatterOffsetBytes
              && offsetof(Uniform, fog) == FogOffsetBytes);
inline void copy(float *to, const QMatrix4x4 &m)
{
    std::memcpy(to, m.constData(), renderData::MatrixBytes);
}
inline void copy(float *to, QVector3D v, float w = 0)
{
    to[0] = v.x();
    to[1] = v.y();
    to[2] = v.z();
    to[3] = w;
}
inline void copy(float *to, QVector4D v)
{
    to[0] = v.x();
    to[1] = v.y();
    to[2] = v.z();
    to[3] = v.w();
}
}
