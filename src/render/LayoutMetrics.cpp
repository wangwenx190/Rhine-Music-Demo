#include "CameraFraming.h"
#include "LayoutMetrics.h"
#include <algorithm>
#include <cmath>

namespace {
// Logical layout values come from the final production CSS override, not screenshot measurements.
constexpr auto ReferenceWidthPixels = 1920;
constexpr auto PortraitInsetRatio = .04;
constexpr auto MinimumPortraitInsetPixels = 23.0;
constexpr auto MaximumPortraitInsetPixels = 42.0;
constexpr auto PortraitTextOverhangPixels = 12;
constexpr auto LandscapeInformationXRatio = .68633333;
constexpr auto MinimumLandscapeRightPixels = 24.0;
constexpr auto LandscapeRightRatio = .022;
}

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
    return m_cinematic  ? std::min(m_width / ReferenceWidthPixels,
                                   m_height / yalahoo::cameraFraming::ReferenceHeightPixels)
            : compact() ? 1
                        : m_height / yalahoo::cameraFraming::ReferenceHeightPixels;
}
double LayoutMetrics::referenceWidth() const
{
    return m_cinematic ? ReferenceWidthPixels : m_width / layoutScale();
}
double LayoutMetrics::referenceHeight() const
{
    return m_cinematic ? yalahoo::cameraFraming::ReferenceHeightPixels : m_height / layoutScale();
}
double LayoutMetrics::informationX() const
{
    return portrait() ? std::clamp(PortraitInsetRatio * m_width, MinimumPortraitInsetPixels,
                                   MaximumPortraitInsetPixels)
                    - PortraitTextOverhangPixels
                      : LandscapeInformationXRatio * m_width;
}
double LayoutMetrics::informationRight() const
{
    return portrait() ? std::clamp(PortraitInsetRatio * m_width, MinimumPortraitInsetPixels,
                                   MaximumPortraitInsetPixels)
                      : std::max(MinimumLandscapeRightPixels, LandscapeRightRatio * m_width);
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
