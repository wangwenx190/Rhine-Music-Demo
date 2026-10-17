#include "ReadbackImage.h"
#include "RhiApi.h"
#include <QFloat16>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace yalahoo::rhi {
QImage readbackImage(const QRhiReadbackResult &result, bool yUp, bool uploaded, QSize logicalSize)
{
    require(result.pixelSize.width() > 0 && result.pixelSize.height() > 0,
            "Empty RHI readback dimensions");
    int bytesPerPixel = 0;
    switch (result.format) {
    case QRhiTexture::R16F:
        bytesPerPixel = 2;
        break;
    case QRhiTexture::R32F:
    case QRhiTexture::RGBA8:
    case QRhiTexture::BGRA8:
        bytesPerPixel = 4;
        break;
    case QRhiTexture::RGBA16F:
        bytesPerPixel = 8;
        break;
    case QRhiTexture::RGBA32F:
        bytesPerPixel = 16;
        break;
    default:
        require(false, "Unsupported RHI readback format");
    }
    const qint64 pixels = qint64(result.pixelSize.width()) * result.pixelSize.height();
    require(pixels <= std::numeric_limits<qsizetype>::max() / bytesPerPixel
                    && result.data.size() == pixels * bytesPerPixel,
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
    const int channels = scalar ? 1 : 4;
    const int componentBytes = bytesPerPixel / channels;
    for (int y = 0; y < image.height(); ++y) {
        const int sourceY = yUp ? image.height() - 1 - y : y;
        auto *destination = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const auto *source = result.data.constData()
                    + (qsizetype(sourceY) * result.pixelSize.width() + x) * bytesPerPixel;
            for (int channel = 0; channel < channels; ++channel) {
                float value;
                if (componentBytes == 2) {
                    qfloat16 half;
                    std::memcpy(&half, source + channel * componentBytes, sizeof(half));
                    value = float(half);
                } else {
                    std::memcpy(&value, source + channel * componentBytes, sizeof(value));
                }
                require(std::isfinite(value), "Nonfinite RHI readback channel");
                // Clamp before integer conversion: finite HDR values can exceed integer range.
                const auto byte = uchar(std::lround(std::clamp(value, 0.f, 1.f) * 255));
                if (scalar) {
                    destination[x * 4] = byte;
                    destination[x * 4 + 1] = byte;
                    destination[x * 4 + 2] = byte;
                    destination[x * 4 + 3] = 255;
                } else {
                    destination[x * 4 + channel] = byte;
                }
            }
        }
    }
    return image;
}
}
