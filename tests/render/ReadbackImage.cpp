#include "Units.h"
#include "ReadbackImage.h"
#include "PixelStorage.h"
#include <QFloat16>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace yalahoo::rhi {
namespace {
enum PixelChannel {
    RedChannel,
    GreenChannel,
    BlueChannel,
    AlphaChannel,
    RgbaChannels
};
}

QImage readbackImage(const QRhiReadbackResult &result, bool yUp, bool uploaded, QSize logicalSize)
{
    require(result.pixelSize.width() > 0 && result.pixelSize.height() > 0,
            "Empty RHI readback dimensions");
    // Only image/readback formats are valid here; inventory also accepts depth descriptors.
    require(result.format == QRhiTexture::R16F || result.format == QRhiTexture::R32F
                    || result.format == QRhiTexture::RGBA8 || result.format == QRhiTexture::BGRA8
                    || result.format == QRhiTexture::RGBA16F
                    || result.format == QRhiTexture::RGBA32F,
            "Unsupported RHI readback format");
    const int pixelBytes = bytesPerPixel(result.format);
    const qint64 pixels = qint64(result.pixelSize.width()) * result.pixelSize.height();
    require(pixels <= std::numeric_limits<qsizetype>::max() / pixelBytes
                    && result.data.size() == pixels * pixelBytes,
            "Invalid RHI readback byte count");
    const auto size = logicalSize.isValid() ? logicalSize : result.pixelSize;
    require(size.width() > 0 && size.height() > 0 && size.width() <= result.pixelSize.width()
                    && size.height() <= result.pixelSize.height(),
            "Invalid logical RHI readback dimensions");
    if (result.format == QRhiTexture::RGBA8 || result.format == QRhiTexture::BGRA8) {
        // Uploaded PNG bytes preserve their original row order on every backend.
        auto image =
                QImage(reinterpret_cast<const uchar *>(result.data.constData()),
                       result.pixelSize.width(), result.pixelSize.height(), QImage::Format_RGBA8888)
                        .copy(QRect(QPoint(), size));
        require(!image.isNull(), "Cannot allocate RHI readback image");
        if (result.format == QRhiTexture::BGRA8)
            image = image.rgbSwapped();
        return yUp && !uploaded ? image.flipped(Qt::Vertical) : image;
    }
    QImage image(size, QImage::Format_RGBA8888);
    require(!image.isNull(), "Cannot allocate RHI readback image");
    const bool scalar = result.format == QRhiTexture::R16F || result.format == QRhiTexture::R32F;
    const int channels = scalar ? 1 : RgbaChannels;
    const int componentBytes = pixelBytes / channels;
    for (int y = 0; y < image.height(); ++y) {
        const int sourceY = yUp ? image.height() - 1 - y : y;
        auto *destination = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const auto *source = result.data.constData()
                    + (qsizetype(sourceY) * result.pixelSize.width() + x) * pixelBytes;
            for (int channel = 0; channel < channels; ++channel) {
                float value;
                if (componentBytes == pixelStorage::HalfScalarBytes) {
                    qfloat16 half;
                    std::memcpy(&half, source + channel * componentBytes, sizeof(half));
                    value = float(half);
                } else {
                    std::memcpy(&value, source + channel * componentBytes, sizeof(value));
                }
                require(std::isfinite(value), "Nonfinite RHI readback channel");
                // Clamp before integer conversion: finite HDR values can exceed integer range.
                const auto byte = uchar(std::lround(std::clamp(value, 0.f, 1.f)
                                                    * yalahoo::units::MaximumChannelByte));
                if (scalar) {
                    destination[x * RgbaChannels] = byte;
                    destination[x * RgbaChannels + GreenChannel] = byte;
                    destination[x * RgbaChannels + BlueChannel] = byte;
                    destination[x * RgbaChannels + AlphaChannel] =
                            yalahoo::units::MaximumChannelByte;
                } else {
                    destination[x * RgbaChannels + channel] = byte;
                }
            }
        }
    }
    return image;
}
}
