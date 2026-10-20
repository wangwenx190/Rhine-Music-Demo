#include "RenderData.h"
#include "Logging.h"
#include "FixedTextureData.h"
#include <QFile>
#include <QtEndian>
#include <QSize>
#include <QImage>
#include <cstring>

namespace {
// Review lookups use the same bundled payload dimensions as production RHI.
constexpr auto SourceRgbaFloatBytes = 16;

}

using namespace Qt::StringLiterals;

void FixedTextureData::setAsset(const QString &asset)
{
    if (asset == m_asset)
        return;
    if (asset == "smaa-area"_L1 || asset == "smaa-search"_L1) {
        QImage image(":/assets/postprocess/"_L1 + asset + ".png"_L1);
        const QSize expected = asset == "smaa-area"_L1
                ? QSize(yalahoo::rhi::renderData::SmaaAreaWidth,
                        yalahoo::rhi::renderData::SmaaAreaHeight)
                : QSize(yalahoo::rhi::renderData::SmaaSearchWidth,
                        yalahoo::rhi::renderData::SmaaSearchHeight);
        if (image.size() != expected) {
            qCWarning(yalahoo::logRender, "Invalid SMAA lookup texture");
            return;
        }
        // SMAAPass sets flipY=false: decoded PNG row zero is lookup UV.y=0.
        image = image.convertToFormat(QImage::Format_RGBA8888);
        m_asset = asset;
        setSize(expected);
        setFormat(QQuick3DTextureData::RGBA8);
        setHasTransparency(false);
        setTextureData(
                QByteArray(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()));
        Q_EMIT assetChanged();
        return;
    }
    QSize size;
    if (asset == "room-pmrem"_L1)
        size = { yalahoo::rhi::renderData::EnvironmentWidth,
                 yalahoo::rhi::renderData::EnvironmentHeight };
    else if (asset == "dfg"_L1)
        size = { yalahoo::rhi::renderData::DfgSide, yalahoo::rhi::renderData::DfgSide };
    else if (asset == "ltc1"_L1 || asset == "ltc2"_L1)
        size = { yalahoo::rhi::renderData::LtcSide, yalahoo::rhi::renderData::LtcSide };
    else if (asset.startsWith(u"ssao-kernel-"_s)
             && (asset.endsWith(u"-16"_s) || asset.endsWith(u"-32"_s) || asset.endsWith(u"-64"_s)))
        size = { asset.section('-'_L1, -1).toInt(), 1 };
    else if (asset.startsWith(u"ssao-noise-"_s)
             && (asset.endsWith(u"-16"_s) || asset.endsWith(u"-32"_s) || asset.endsWith(u"-64"_s)))
        size = { yalahoo::rhi::renderData::NoiseSide, yalahoo::rhi::renderData::NoiseSide };
    else {
        qCWarning(yalahoo::logRender, "Unknown fixed material texture");
        return;
    }
    QFile file(":/assets/environment/"_L1 + asset + ".rgba32f"_L1);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(yalahoo::logRender, "Missing fixed material texture");
        return;
    }
    auto bytes = file.readAll();
    if (bytes.size() != size.width() * size.height() * SourceRgbaFloatBytes) {
        qCWarning(yalahoo::logRender, "Invalid fixed material texture size");
        return;
    }
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    constexpr auto FloatScalarBytes = 4;
    for (qsizetype i = 0; i < bytes.size(); i += FloatScalarBytes) {
        quint32 word = qFromLittleEndian<quint32>(bytes.constData() + i);
        std::memcpy(bytes.data() + i, &word, FloatScalarBytes);
    }
#endif
    m_asset = asset;
    setSize(size);
    setFormat(QQuick3DTextureData::RGBA32F);
    setHasTransparency(false);
    setTextureData(bytes);
    Q_EMIT assetChanged();
}
