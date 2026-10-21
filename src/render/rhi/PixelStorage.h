// Maps uncompressed RHI payload formats to bytes; unknown backend depth uses its declared bound.
// Mip geometry is independent of the scalar storage representation.

#pragma once
#include "RhiApi.h"

namespace yalahoo::rhi {
namespace pixelStorage {
// Uncompressed format payload bytes; backend-private depth uses a conservative upper bound.
inline constexpr auto HalfRgbaOrDepthStencilBytes = 8;
inline constexpr auto FloatRgbaBytes = 16;
inline constexpr auto RgbaOrFloatScalarBytes = 4;
inline constexpr auto HalfScalarBytes = 2;
}

inline int bytesPerPixel(QRhiTexture::Format format)
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

struct TextureExtent final
{
    qint64 pixels;
    int levels;
};
inline TextureExtent textureExtent(QSize size, bool mipmapped)
{
    TextureExtent extent{ qint64(size.width()) * size.height(), 1 };
    if (mipmapped)
        while (size.width() > 1 || size.height() > 1) {
            size = { qMax(1, size.width() / 2), qMax(1, size.height() / 2) };
            extent.pixels += qint64(size.width()) * size.height();
            ++extent.levels;
        }
    return extent;
}
}
