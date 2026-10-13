#pragma once
#include "RhiApi.h"
#include "GeometryAudit.h"
#include <QHash>
#include <QImage>

namespace yalahoo::rhi {
struct PreparedAssets
{
    QVector<assets::Batch> geometry;
    QHash<QString, QByteArray> bytes;
    QHash<QString, QImage> images;
    QHash<QString, QShader> shaders;
};
const PreparedAssets &preparedAssets();
}
