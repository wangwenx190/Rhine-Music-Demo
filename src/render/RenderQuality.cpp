#include "RenderQuality.h"
#include "NumericValue.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
// Shadow power-of-two growth is bounded before conversion to the native integer dimension.
constexpr auto MaximumShadowSideShift = 30;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
float RenderQuality::aperture(double detail) const
{
    if (!std::isfinite(detail))
        return 0;
    const double coefficient = renderQuality::BrowseAperture
            + renderQuality::DetailApertureGain * std::clamp(detail, 0., 1.);
    const double value = coefficient * (m_depthOfField / renderQuality::PercentScale);
    return float(std::min(value, double(std::numeric_limits<float>::max())));
}
RenderQuality::RenderQuality(QObject *parent) : QObject(parent)
{
}
bool RenderQuality::configure(const QVariantMap &input)
{
    const auto value =
            input.value(u"depthOfField"_s, yalahoo::renderQuality::DefaultDepthOfFieldPercent);
    const auto amount = finiteNumber(value);
    if (!amount || !renderQuality::validDepthOfField(*amount))
        return false;
    const double next = *amount;
    if (next == m_depthOfField)
        return true;
    m_depthOfField = next;
    Q_EMIT changed();
    return true;
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
    while (side < demand && side < (qint64(1) << MaximumShadowSideShift))
        side *= 2;
    return int(side);
}
}
