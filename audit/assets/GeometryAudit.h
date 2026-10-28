// Independently inspects imported Qt meshes and compares their source geometry.
#pragma once
#include "GeometryData.h"
#include <QJsonObject>
namespace yalahoo::assets {
Batch readQtMesh(const QByteArray &bytes);
QJsonObject compareGeometry(const QVector<Batch> &original, const QVector<Batch> &imported);
}
