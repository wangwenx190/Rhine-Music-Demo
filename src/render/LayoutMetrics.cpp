#include "LayoutMetrics.h"
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
QString LayoutMetrics::kind() const
{
    return m_cinematic   ? u"cinematic"_s
            : portrait() ? u"portrait"_s
            : compact()  ? u"compact"_s
                         : u"desktop"_s;
}
double LayoutMetrics::layoutScale() const
{
    return m_cinematic  ? std::min(m_width / 1920, m_height / 1080)
            : compact() ? 1
                        : m_height / 1080;
}
double LayoutMetrics::referenceWidth() const
{
    return m_cinematic ? 1920 : m_width / layoutScale();
}
double LayoutMetrics::referenceHeight() const
{
    return m_cinematic ? 1080 : m_height / layoutScale();
}
double LayoutMetrics::informationX() const
{
    return portrait() ? std::clamp(.04 * m_width, 23.0, 42.0) - 12 : .68633333 * m_width;
}
double LayoutMetrics::informationRight() const
{
    return portrait() ? std::clamp(.04 * m_width, 23.0, 42.0) : std::max(24.0, .022 * m_width);
}
void LayoutMetrics::setViewport(double width, double height, bool coarse, bool cinematic)
{
    if (!std::isfinite(width) || !std::isfinite(height))
        return;
    width = std::max(1.0, width);
    height = std::max(1.0, height);
    if (m_width == width && m_height == height && m_coarse == coarse && m_cinematic == cinematic)
        return;
    m_width = width;
    m_height = height;
    m_coarse = coarse;
    m_cinematic = cinematic;
    Q_EMIT changed();
}
}
