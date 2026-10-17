#include "FixedTextureData.h"
#include <QFile>
#include <QtEndian>
#include <QSize>
#include <QImage>
#include <cstring>

using namespace Qt::StringLiterals;

void FixedTextureData::setAsset(const QString &asset)
{
    if (asset == m_asset)
        return;
    if (asset == "smaa-area"_L1 || asset == "smaa-search"_L1) {
        QImage image(":/assets/postprocess/"_L1 + asset + ".png"_L1);
        const QSize expected = asset == "smaa-area"_L1 ? QSize(160, 560) : QSize(66, 33);
        if (image.size() != expected) {
            qWarning("Invalid SMAA lookup texture");
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
        size = { 768, 1024 };
    else if (asset == "dfg"_L1)
        size = { 16, 16 };
    else if (asset == "ltc1"_L1 || asset == "ltc2"_L1)
        size = { 64, 64 };
    else if (asset.startsWith(u"ssao-kernel-"_s)
             && (asset.endsWith(u"-16"_s) || asset.endsWith(u"-32"_s) || asset.endsWith(u"-64"_s)))
        size = { asset.section('-'_L1, -1).toInt(), 1 };
    else if (asset.startsWith(u"ssao-noise-"_s)
             && (asset.endsWith(u"-16"_s) || asset.endsWith(u"-32"_s) || asset.endsWith(u"-64"_s)))
        size = { 4, 4 };
    else {
        qWarning("Unknown fixed material texture");
        return;
    }
    QFile file(":/assets/environment/"_L1 + asset + ".rgba32f"_L1);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Missing fixed material texture");
        return;
    }
    auto bytes = file.readAll();
    if (bytes.size() != size.width() * size.height() * 16) {
        qWarning("Invalid fixed material texture size");
        return;
    }
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    for (qsizetype i = 0; i < bytes.size(); i += 4) {
        quint32 word = qFromLittleEndian<quint32>(bytes.constData() + i);
        std::memcpy(bytes.data() + i, &word, 4);
    }
#endif
    m_asset = asset;
    setSize(size);
    setFormat(QQuick3DTextureData::RGBA32F);
    setHasTransparency(false);
    setTextureData(bytes);
    Q_EMIT assetChanged();
}
