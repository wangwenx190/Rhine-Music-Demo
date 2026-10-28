// Optional observers inspect immutable submission data at explicit lifecycle boundaries.
#pragma once
#include "RhiItem.h"
#include "DrawUniform.h"
class QObject;
namespace yalahoo::rhi {
bool opticalProbesAvailable();
QString inspectedFragment(int probe, QString fragment);
void applySceneInspection(Input &next, const Input &previous, const QObject &item);
namespace renderDetail {
void appendFogUniform(QJsonArray &fogUniforms, const Uniform &submitted);
void recordOpaqueUniform(const QByteArray &bytes, quint32 offset, QJsonArray &captureKinds,
                         QJsonArray &fogUniforms);
}
}
