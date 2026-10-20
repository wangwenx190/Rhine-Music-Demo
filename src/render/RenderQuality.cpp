#include "RenderQuality.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace Qt::StringLiterals;

namespace yalahoo {
RenderQuality::RenderQuality(QObject *parent) : QObject(parent)
{
}
void RenderQuality::configure(const QVariantMap &input)
{
    const auto value = input.value(u"depthOfField"_s, 100);
    bool valid = false;
    const double amount = value.toDouble(&valid);
    if (!valid || value.metaType().id() == QMetaType::QString
        || value.metaType().id() == QMetaType::Bool || !std::isfinite(amount))
        return;
    const double next = std::max(0.0, amount);
    if (next == m_depthOfField)
        return;
    m_depthOfField = next;
    Q_EMIT changed();
}
void RenderQuality::resize(double width, double height, double dpr)
{
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(dpr) || width <= 0
        || height <= 0 || dpr <= 0)
        return;
    const double pixelWidth = std::round(width * dpr);
    const double pixelHeight = std::round(height * dpr);
    if (!std::isfinite(pixelWidth) || !std::isfinite(pixelHeight)
        || pixelWidth > std::numeric_limits<int>::max()
        || pixelHeight > std::numeric_limits<int>::max())
        throw std::runtime_error("Native output dimensions exceed the supported integer range");
    const QSize next(std::max(1, int(pixelWidth)), std::max(1, int(pixelHeight)));
    if (next == m_size)
        return;
    m_size = next;
    Q_EMIT changed();
}
int RenderQuality::shadows() const
{
    // Allocate for current physical texel density, retaining compatible shadow targets
    // throughout resizes within the same power-of-two interval.
    const qint64 demand = qint64(std::max(m_size.width(), m_size.height())) * 2;
    qint64 side = 1;
    while (side < demand && side < (qint64(1) << 30))
        side *= 2;
    return int(side);
}
}
