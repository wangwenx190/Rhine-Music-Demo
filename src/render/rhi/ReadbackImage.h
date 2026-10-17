#pragma once
#include <QImage>

struct QRhiReadbackResult;

namespace yalahoo::rhi {
// Throws a checked data error; callers must contain it within the QRhi callback.
QImage readbackImage(const QRhiReadbackResult &result, bool yUp, bool uploaded = false,
                     QSize logicalSize = { });
}
