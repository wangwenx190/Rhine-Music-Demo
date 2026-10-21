// Describes renderer-owned attachments and buffers for diagnostics. Byte estimates remain
// distinct from driver allocation and transient readback ownership.

#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "PixelStorage.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace yalahoo::rhi {
// Descriptor payload estimate, not measured VRAM. Includes full mip chains and MSAA storage.
class ResourceInventory final
{
    Q_DISABLE_COPY_MOVE(ResourceInventory)
public:
    ResourceInventory() = default;
    void plannedTexture(const QString &name, QSize size, QRhiTexture::Format format,
                        int samples = 1, bool mips = false)
    {
        const auto extent = textureExtent(size, mips);
        add(name, size, int(format), samples, extent.levels,
            extent.pixels * bytesPerPixel(format) * samples);
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
        const auto extent = textureExtent(size, texture->flags().testFlag(QRhiTexture::MipMapped));
        add(name, size, int(texture->format()), texture->sampleCount(), extent.levels,
            extent.pixels * bytesPerPixel(texture->format()) * texture->sampleCount());
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
