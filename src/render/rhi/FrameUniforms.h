// Prepares material templates and caches per-instance light fields on the render thread.
// Input is borrowed only for a call; cached values retain no QObject or QRhi pointers.

#pragma once
#include "DrawUniform.h"
#include "RhiItem.h"

namespace yalahoo::rhi {
class FrameUniforms final
{
public:
    void prepare(const Input &input, const QMatrix4x4 &clipCorrection, bool yUp, bool zeroToOne,
                 QSize targetSize, int shadowSize, const QVector<QMatrix4x4> &entities,
                 const QVector<bool> &visible, bool batched);
    Uniform uniform(const Input &input, const QMatrix4x4 &model, int kind, int entity,
                    const QVector<QVector4D> &coverUv) const;
    QVector4D originLight(int entity) const
    {
        return entityLight0[entity];
    }
    QVector4D secondaryLight(int entity) const
    {
        return entityLight1[entity];
    }

private:
    Uniform uniformTemplate(const Input &input, int kind, bool yUp, bool zeroToOne,
                            QSize targetSize, int shadowSize) const;
    std::array<Uniform, UniformBaseCount> uniformBases;
    QVector<QVector4D> entityLight0, entityLight1;
    QVector<QMatrix4x4> lightEntities;
    QVector<QMatrix4x4> lightModels;
    QVector4D cachedRowSettings, cachedPrintSettings;
    QVector3D cachedRowColumn, cachedLightColumn;
    QMatrix4x4 nativeVP, nativeLightVP, canonicalLightVP;
};
}
