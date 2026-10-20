#include "RenderData.h"
#include "PreparedAssets.h"
#include <QDirIterator>
#include <QtEndian>
#include <QFloat16>
#include <cstring>

namespace {
// Bundled source floats are little-endian; the prepared environment converts source RGBA32F to its GPU format.
constexpr auto FloatPayloadBytes = 4;
constexpr auto SourceEnvironmentBytesPerPixel = 16;
}

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
const PreparedAssets &preparedAssets()
{
    // Called by RhiItem's GUI constructor before its renderer exists. Qt resources are immutable;
    // this bounded process cache also survives scene-graph invalidation without re-reading assets.
    static const auto assets = [] {
        PreparedAssets result;
        QDirIterator files(u":/rhi"_s, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto path = files.next();
            if (!files.fileInfo().isFile())
                continue;
            QFile file(path);
            require(file.open(QIODevice::ReadOnly), "Missing embedded RHI resource");
            auto bytes = file.readAll();
            if (path.endsWith(u".qsb"_s)) {
                const auto shader = QShader::fromSerialized(bytes);
                require(shader.isValid(), "Invalid embedded RHI shader");
                result.shaders.insert(path, shader);
            } else if (path.endsWith(u".png"_s)) {
                const auto image = QImage::fromData(bytes).convertToFormat(QImage::Format_RGBA8888);
                require(!image.isNull(), "Invalid embedded RHI lookup");
                result.images.insert(path, image);
            } else {
                if (path.endsWith(u".rgba32f"_s)) {
                    require(bytes.size() % FloatPayloadBytes == 0,
                            "Invalid embedded float payload");
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
                    for (qsizetype i = 0; i < bytes.size(); i += FloatPayloadBytes) {
                        const auto word = qFromLittleEndian<quint32>(bytes.constData() + i);
                        std::memcpy(bytes.data() + i, &word, FloatPayloadBytes);
                    }
#endif
                }
                result.bytes.insert(path, bytes);
            }
        }
        result.geometry =
                assets::readGlb(result.bytes.value(u":/rhi/models/music-case-v040.glb"_s));
        result.bytes.remove(u":/rhi/models/music-case-v040.glb"_s);
        const auto room = result.bytes.value(u":/rhi/environment/room-pmrem.rgba32f"_s);
        require(room.size()
                        == renderData::EnvironmentWidth * renderData::EnvironmentHeight
                                * SourceEnvironmentBytesPerPixel,
                "Invalid PMREM input");
        QByteArray halfBytes(room.size() / 2, '\0');
        for (qsizetype i = 0; i < room.size() / FloatPayloadBytes; ++i) {
            float value;
            std::memcpy(&value, room.constData() + i * FloatPayloadBytes, FloatPayloadBytes);
            const qfloat16 half(value);
            require(float(half) == value, "PMREM conversion changes source values");
            std::memcpy(halfBytes.data() + i * 2, &half, 2);
        }
        result.bytes.remove(u":/rhi/environment/room-pmrem.rgba32f"_s);
        result.bytes.insert(u":/rhi/environment/room-pmrem.rgba16f"_s, halfBytes);
        return result;
    }();
    return assets;
}
QShader shader(const QString &path)
{
    const auto value = preparedAssets().shaders.value(path);
    require(value.isValid(), "Missing prepared RHI shader");
    return value;
}
}
