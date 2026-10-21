// Maps uncompressed RHI payload formats to bytes; unknown backend depth uses its declared bound.
// Mip geometry is independent of the scalar storage representation.

#pragma once
#include "RhiApi.h"
#include <algorithm>
#include <iterator>
#include <utility>

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
    // UnknownFormat is QRhiRenderBuffer's undisclosed backend depth format; D32FS8
    // remains its conservative bound. Other unlisted formats are rejected.
    using Entry = std::pair<QRhiTexture::Format, int>;
    static constexpr Entry formats[] = {
        { QRhiTexture::UnknownFormat, pixelStorage::HalfRgbaOrDepthStencilBytes },
        { QRhiTexture::RGBA16F, pixelStorage::HalfRgbaOrDepthStencilBytes },
        { QRhiTexture::D32FS8, pixelStorage::HalfRgbaOrDepthStencilBytes },
        { QRhiTexture::RGBA32F, pixelStorage::FloatRgbaBytes },
        { QRhiTexture::D16, pixelStorage::HalfScalarBytes },
        { QRhiTexture::R16F, pixelStorage::HalfScalarBytes },
        { QRhiTexture::RGBA8, pixelStorage::RgbaOrFloatScalarBytes },
        { QRhiTexture::BGRA8, pixelStorage::RgbaOrFloatScalarBytes },
        { QRhiTexture::R32F, pixelStorage::RgbaOrFloatScalarBytes },
        { QRhiTexture::D32F, pixelStorage::RgbaOrFloatScalarBytes },
        { QRhiTexture::D24S8, pixelStorage::RgbaOrFloatScalarBytes },
        { QRhiTexture::D24, pixelStorage::RgbaOrFloatScalarBytes }
    };
    const auto entry =
            std::find_if(std::begin(formats), std::end(formats), [format](const Entry &candidate) {
                return candidate.first == format;
            });
    require(entry != std::end(formats), "Cannot estimate selected attachment format");
    return entry->second;
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
