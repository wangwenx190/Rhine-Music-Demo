// Loads and validates immutable render assets on first use. Shared prepared values avoid
// repeated decoding while preserving explicit failure at the resource boundary.

#pragma once
#include "RhiApi.h"
#include "GeometryAudit.h"
#include <QHash>
#include <QImage>

namespace yalahoo::rhi {
struct PreparedAssets final
{
    QVector<assets::Batch> geometry;
    QHash<QString, QByteArray> bytes;
    QHash<QString, QImage> images;
    QHash<QString, QShader> shaders;
};
const PreparedAssets &preparedAssets();
}
