#include "PreparedAssets.h"
#include <QDirIterator>
#include <QtEndian>
#include <QFloat16>
#include <cstring>

namespace yalahoo::rhi {
const PreparedAssets &preparedAssets()
{
    // Called by RhiItem's GUI constructor before its renderer exists. Qt resources are immutable;
    // this bounded process cache also survives scene-graph invalidation without re-reading assets.
    static const auto assets = [] {
        PreparedAssets result;
        QDirIterator files(":/rhi", QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto path = files.next();
            if (!files.fileInfo().isFile())
                continue;
            QFile file(path);
            require(file.open(QIODevice::ReadOnly), "Missing embedded RHI resource");
            auto bytes = file.readAll();
            if (path.endsWith(".qsb")) {
                const auto shader = QShader::fromSerialized(bytes);
                require(shader.isValid(), "Invalid embedded RHI shader");
                result.shaders.insert(path, shader);
            } else if (path.endsWith(".png")) {
                const auto image = QImage::fromData(bytes).convertToFormat(QImage::Format_RGBA8888);
                require(!image.isNull(), "Invalid embedded RHI lookup");
                result.images.insert(path, image);
            } else {
                if (path.endsWith(".rgba32f")) {
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
                    for (qsizetype i = 0; i < bytes.size(); i += 4) {
                        const auto word = qFromLittleEndian<quint32>(bytes.constData() + i);
                        std::memcpy(bytes.data() + i, &word, 4);
                    }
#endif
                }
                result.bytes.insert(path, bytes);
            }
        }
        result.geometry = assets::readGlb(result.bytes.value(":/rhi/models/music-case-v040.glb"));
        result.bytes.remove(":/rhi/models/music-case-v040.glb");
        const auto room = result.bytes.value(":/rhi/environment/room-pmrem.rgba32f");
        require(room.size() == 768 * 1024 * 16, "Invalid PMREM input");
        QByteArray halfBytes(room.size() / 2, '\0');
        for (qsizetype i = 0; i < room.size() / 4; ++i) {
            float value;
            std::memcpy(&value, room.constData() + i * 4, 4);
            const qfloat16 half(value);
            require(float(half) == value, "PMREM conversion changes source values");
            std::memcpy(halfBytes.data() + i * 2, &half, 2);
        }
        result.bytes.remove(":/rhi/environment/room-pmrem.rgba32f");
        result.bytes.insert(":/rhi/environment/room-pmrem.rgba16f", halfBytes);
        return result;
    }();
    return assets;
}
QShader shader(const char *path)
{
    const auto value = preparedAssets().shaders.value(QString::fromLatin1(path));
    require(value.isValid(), "Missing prepared RHI shader");
    return value;
}
}
