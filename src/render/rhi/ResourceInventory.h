// Describes renderer-owned attachments and buffers for diagnostics. Byte estimates remain
// distinct from driver allocation and transient readback ownership.

#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "RhiApi.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace yalahoo::rhi {
namespace pixelStorage {
// Uncompressed format payload bytes; backend-private depth uses a conservative upper bound.
inline constexpr auto HalfRgbaOrDepthStencilBytes = 8;
inline constexpr auto FloatRgbaBytes = 16;
inline constexpr auto RgbaOrFloatScalarBytes = 4;
inline constexpr auto HalfScalarBytes = 2;
}

// Descriptor payload estimate, not measured VRAM. Includes full mip chains and MSAA storage.
class ResourceInventory final
{
    Q_DISABLE_COPY_MOVE(ResourceInventory)
public:
    ResourceInventory() = default;
    void plannedTexture(const QString &name, QSize size, QRhiTexture::Format format,
                        int samples = 1, bool mips = false)
    {
        qint64 pixels = qint64(size.width()) * size.height();
        int levels = 1;
        if (mips) {
            auto mip = size;
            while (mip.width() > 1 || mip.height() > 1) {
                mip = { qMax(1, mip.width() / pixelStorage::HalfScalarBytes),
                        qMax(1, mip.height() / pixelStorage::HalfScalarBytes) };
                pixels += qint64(mip.width()) * mip.height();
                ++levels;
            }
        }
        add(name, size, int(format), samples, levels, pixels * bytesPerPixel(format) * samples);
    }
    void plannedBuffer(const QString &name, qint64 bytes)
    {
        add(name, { }, -1, 1, 1, bytes);
    }
    void texture(const QString &name, QRhiTexture *texture)
    {
        if (!texture || m_seen.contains(texture))
            return;
        m_seen.insert(texture);
        const auto size = texture->pixelSize();
        qint64 pixels = qint64(size.width()) * size.height();
        int levels = 1;
        if (texture->flags().testFlag(QRhiTexture::MipMapped)) {
            auto mip = size;
            while (mip.width() > 1 || mip.height() > 1) {
                mip = { qMax(1, mip.width() / pixelStorage::HalfScalarBytes),
                        qMax(1, mip.height() / pixelStorage::HalfScalarBytes) };
                pixels += qint64(mip.width()) * mip.height();
                ++levels;
            }
        }
        add(name, size, int(texture->format()), texture->sampleCount(), levels,
            pixels * bytesPerPixel(texture->format()) * texture->sampleCount());
    }
    void renderBuffer(const QString &name, QRhiRenderBuffer *buffer)
    {
        if (!buffer || m_seen.contains(buffer))
            return;
        m_seen.insert(buffer);
        const auto size = buffer->pixelSize();
        add(name, size, int(buffer->backingFormat()), buffer->sampleCount(), 1,
            qint64(size.width()) * size.height() * bytesPerPixel(buffer->backingFormat())
                    * buffer->sampleCount());
    }
    void buffer(const QString &name, QRhiBuffer *buffer)
    {
        if (buffer)
            add(name, { }, -1, 1, 1, buffer->size());
    }
    QJsonArray entries() const
    {
        return m_entries;
    }
    qint64 bytes() const
    {
        return m_bytes;
    }

private:
    static int bytesPerPixel(QRhiTexture::Format format)
    {
        switch (format) {
        case QRhiTexture::UnknownFormat:
            // QRhiRenderBuffer may not expose its backend depth format. D32FS8 is the
            // conservative supported depth/stencil upper bound; report format=0 explicitly.
            return pixelStorage::HalfRgbaOrDepthStencilBytes;
        case QRhiTexture::RGBA16F:
        case QRhiTexture::D32FS8:
            return pixelStorage::HalfRgbaOrDepthStencilBytes;
        case QRhiTexture::RGBA32F:
            return pixelStorage::FloatRgbaBytes;
        case QRhiTexture::D16:
        case QRhiTexture::R16F:
            return pixelStorage::HalfScalarBytes;
        case QRhiTexture::RGBA8:
        case QRhiTexture::BGRA8:
        case QRhiTexture::R32F:
        case QRhiTexture::D32F:
        case QRhiTexture::D24S8:
        case QRhiTexture::D24:
            return pixelStorage::RgbaOrFloatScalarBytes;
        default:
            require(false, "Cannot estimate selected attachment format");
            return 0;
        }
    }
    void add(const QString &name, QSize size, int format, int samples, int levels, qint64 bytes)
    {
        require(bytes >= 0 && samples > 0, "Invalid resource plan");
        m_bytes += bytes;
        m_entries.append(QJsonObject{ { u"name"_s, name },
                                      { u"width"_s, size.width() },
                                      { u"height"_s, size.height() },
                                      { u"format"_s, format },
                                      { u"samples"_s, samples },
                                      { u"mipLevels"_s, levels },
                                      { u"estimatedBytes"_s, bytes } });
    }
    QSet<const void *> m_seen;
    QJsonArray m_entries;
    qint64 m_bytes = 0;
};
}
