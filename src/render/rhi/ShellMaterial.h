#pragma once
#include <QVector3D>
#include <QVector4D>

namespace yalahoo::rhi {
struct ShellMaterial
{
    QVector3D tint, attenuation;
    QVector4D transport;
    QVector4D coat;
};
// Source parameters before the fixed, shared R07 optical calibration.
ShellMaterial shellMaterial(int surface, float warmth, float clarity, float environment);
}
