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
// Effective source parameters, copied unchanged to the optical uniforms.
float lidRoughness(float warmth, float clarity);
ShellMaterial shellMaterial(int surface, float warmth, float clarity, float environment);
}
