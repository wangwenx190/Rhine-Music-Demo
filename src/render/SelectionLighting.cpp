#include "SceneGeometry.h"
#include "RowLightController.h"
#include "SelectionLighting.h"
#include <QVector4D>
#include <algorithm>

namespace {
// Palette intensities preserve source light units; sRGB encoding/decoding uses the standard piecewise transfer.
constexpr auto SrgbDecodeKnee = .04045;
constexpr auto SrgbLinearSlope = 12.92;
constexpr auto SrgbOffset = .055;
constexpr auto SrgbScale = 1.055;
constexpr auto SrgbExponent = 2.4;
constexpr auto AnchorFollowRate = 5;
constexpr auto NightEnvironment = .16;
constexpr auto DayEnvironment = .25;
constexpr auto AreaTargetDrop = .25;
constexpr auto NightKey = .30;
constexpr auto DayKey = .45;
constexpr auto NightHemisphere = .21;
constexpr auto DayHemisphere = .32;
constexpr auto NightSpotSourceIntensity = 130;
constexpr auto DaySpotSourceIntensity = 180;
constexpr auto SpotUnitScale = 64;
constexpr auto NightAreaIntensity = 24;
constexpr auto DayAreaIntensity = 34;
constexpr auto NightScatter = .72;
constexpr auto NightPrintAmbient = .12;
constexpr auto DayPrintAmbient = .5;
constexpr auto NightExposure = 1.08;
constexpr auto DayExposure = 1.05;
constexpr auto ThemeTransitionSeconds = .65;
constexpr auto MinimumRibbonWidth = .6;
constexpr auto MaximumRibbonWidth = 2.5;
constexpr auto MaximumRibbonOffset = 6;
constexpr auto ThemeBezierIterations = 6;
constexpr auto MinimumRibbonSeconds = .4;
constexpr auto MaximumRibbonSeconds = 2.4;
constexpr auto AreaHeightOffset = .5;
constexpr auto HybridAreaGain = .3;
constexpr auto AreaLightForwardOffset = .44;
constexpr auto SrgbEncodeKnee = .0031308;
constexpr auto ThemeBezierCubic = 1.6;
constexpr auto ThemeBezierQuadratic = 1.8;
constexpr auto ThemeBezierLinear = 1.2;
constexpr auto ThemeBezierDerivativeQuadratic = 4.8;
constexpr auto ThemeBezierDerivativeLinear = 3.6;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
SceneVector linear(QLatin1StringView hex)
{
    const QColor c(hex);
    auto decode = [](double s) {
        return s <= SrgbDecodeKnee ? s / SrgbLinearSlope
                                   : std::pow((s + SrgbOffset) / SrgbScale, SrgbExponent);
    };
    return { decode(c.redF()), decode(c.greenF()), decode(c.blueF()) };
}
void followVector(SceneVector &value, SceneVector &velocity, SceneVector target, double dt)
{
    const double decay = std::exp(-AnchorFollowRate * dt);
    const auto delta = value - target, impulse = velocity + delta * AnchorFollowRate;
    value = target + (delta + impulse * dt) * decay;
    velocity = (velocity - impulse * (AnchorFollowRate * dt)) * decay;
}
}
SceneVector LightingPalette::floorColor(QStringView theme)
{
    return linear(theme == u"night"          ? "#0b1828"_L1
                          : theme == u"dusk" ? "#a6b8c0"_L1
                                             : "#d8c9b9"_L1);
}
LightingPalette LightingPalette::theme(bool night)
{
    LightingPalette p;
    p.warmth = night ? 0 : 1;
    p.environment = night ? NightEnvironment : DayEnvironment;
    p.key = night ? NightKey : DayKey;
    p.hemisphere = night ? NightHemisphere : DayHemisphere;
    p.spot = (night ? NightSpotSourceIntensity : DaySpotSourceIntensity) * SpotUnitScale;
    p.area = night ? NightAreaIntensity : DayAreaIntensity;
    p.scatter = night ? NightScatter : 1;
    p.printAmbient = night ? NightPrintAmbient : DayPrintAmbient;
    p.exposure = night ? NightExposure : DayExposure;
    p.keyColor = linear(night ? "#e5f0ff"_L1 : "#fff7ed"_L1);
    p.skyColor = linear(night ? "#e2eeff"_L1 : "#fffaf5"_L1);
    p.groundColor = linear(night ? "#56708c"_L1 : "#b4a18c"_L1);
    p.spotColor = linear(night ? "#dbe9ff"_L1 : "#ffe3b2"_L1);
    p.scatterColor = linear(night ? "#cee5ff"_L1 : "#ffdba3"_L1);
    p.background = linear(night ? "#07111f"_L1 : "#eae5e1"_L1);
    p.floor = floorColor(night ? u"night" : u"day");
    return p;
}
SelectionLighting::SelectionLighting(QObject *parent)
    : QObject(parent), m_palette(LightingPalette::theme(false)), m_from(m_palette), m_to(m_palette)
{
}
// Invert the authored cubic Bezier x coordinate with bounded Newton iterations.
// The polynomial and derivative coefficients come from the source control points.
double SelectionLighting::themeProgress(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    double c = t;
    for (int n = 0; n < ThemeBezierIterations; ++n)
        c = std::clamp(
                c
                        - (((ThemeBezierCubic * c - ThemeBezierQuadratic) * c + ThemeBezierLinear)
                                   * c
                           - t)
                                / ((ThemeBezierDerivativeQuadratic * c
                                    - ThemeBezierDerivativeLinear)
                                           * c
                                   + ThemeBezierLinear),
                0.0, 1.0);
    return t == 1 ? 1 : c * c * (3 - 2 * c);
}
void SelectionLighting::setNight(bool night)
{
    if (night == m_night)
        return;
    m_night = night;
    m_from = m_palette;
    m_to = LightingPalette::theme(night);
    m_elapsed = 0;
    if (m_reduced) {
        m_palette = m_to;
        m_elapsed = ThemeTransitionSeconds;
    }
    Q_EMIT nightChanged();
    Q_EMIT changed();
}
void SelectionLighting::setReducedMotion(bool reduced)
{
    m_reduced = reduced;
    if (reduced) {
        m_palette = m_to;
        m_elapsed = ThemeTransitionSeconds;
        Q_EMIT changed();
    }
}
QVariantMap SelectionLighting::settings() const
{
    const QStringList modes{ u"baseline"_s, u"area"_s, u"guided"_s, u"hybrid"_s };
    return { { u"mode"_s, modes[int(m_settings.mode)] },
             { u"strength"_s, m_settings.strength },
             { u"width"_s, m_settings.width },
             { u"offset"_s, m_settings.offset },
             { u"fill"_s, m_settings.fill },
             { u"transitionDuration"_s, m_settings.transitionDuration } };
}
void SelectionLighting::setSettings(RowLightingSettings s)
{
    auto valid = [](double n, double previous, double lo, double hi) {
        return std::isfinite(n) ? std::clamp(n, lo, hi) : previous;
    };
    if (int(s.mode) < 0 || int(s.mode) > int(RowLightingMode::Hybrid))
        s.mode = m_settings.mode;
    s.strength = valid(s.strength, m_settings.strength, 0, 2);
    s.width = valid(s.width, m_settings.width, MinimumRibbonWidth, MaximumRibbonWidth);
    s.offset = valid(s.offset, m_settings.offset, -MaximumRibbonOffset, MaximumRibbonOffset);
    s.fill = valid(s.fill, m_settings.fill, 0, 1);
    s.transitionDuration = valid(s.transitionDuration, m_settings.transitionDuration,
                                 MinimumRibbonSeconds, MaximumRibbonSeconds);
    const auto before = settings();
    m_settings = s;
    ribbon.settings = s;
    if (m_comparing)
        ribbon.settings.mode = RowLightingMode::Baseline;
    if (settings() != before) {
        Q_EMIT settingsChanged();
        Q_EMIT changed();
    }
}
bool SelectionLighting::configure(const QVariantMap &patch)
{
    auto next = m_settings;
    const QStringList modes{ u"baseline"_s, u"area"_s, u"guided"_s, u"hybrid"_s };
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) {
        if (it.key() == "mode"_L1) {
            const auto index = modes.indexOf(it.value().toString());
            if (index < 0)
                return false;
            next.mode = RowLightingMode(index);
            continue;
        }
        double *target = it.key() == "strength"_L1    ? &next.strength
                : it.key() == "width"_L1              ? &next.width
                : it.key() == "offset"_L1             ? &next.offset
                : it.key() == "fill"_L1               ? &next.fill
                : it.key() == "transitionDuration"_L1 ? &next.transitionDuration
                                                      : nullptr;
        const auto &value = it.value();
        if (!target || value.metaType().id() == QMetaType::QString
            || value.metaType().id() == QMetaType::Bool || !value.canConvert<double>()
            || !std::isfinite(value.toDouble()))
            return false;
        *target = value.toDouble();
    }
    setSettings(next);
    return true;
}
void SelectionLighting::resetSettings()
{
    setComparing(false);
    setSettings({ });
}
void SelectionLighting::setComparing(bool value)
{
    if (m_comparing == value)
        return;
    m_comparing = value;
    ribbon.settings = m_settings;
    if (value)
        ribbon.settings.mode = RowLightingMode::Baseline;
    Q_EMIT comparingChanged();
    Q_EMIT changed();
}
void SelectionLighting::advanceTheme(double dt)
{
    if (!std::isfinite(dt) || dt < 0 || m_elapsed >= ThemeTransitionSeconds)
        return;
    m_elapsed = std::min(ThemeTransitionSeconds, m_elapsed + dt);
    const double t = themeProgress(m_elapsed / ThemeTransitionSeconds);
#define NUMBER(name) m_palette.name = m_from.name + (m_to.name - m_from.name) * t
#define COLOR(name) m_palette.name = m_from.name * (1 - t) + m_to.name * t
    NUMBER(warmth);
    NUMBER(environment);
    NUMBER(key);
    NUMBER(hemisphere);
    NUMBER(fill);
    NUMBER(spot);
    NUMBER(area);
    NUMBER(scatter);
    NUMBER(printAmbient);
    NUMBER(exposure);
    COLOR(keyColor);
    COLOR(skyColor);
    COLOR(groundColor);
    COLOR(spotColor);
    COLOR(scatterColor);
    COLOR(background);
    COLOR(floor);
#undef NUMBER
#undef COLOR
}
void SelectionLighting::update(const RowLightingInput &i, SceneVector aim, SceneVector offset,
                               double detail, double dt)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    advanceTheme(dt);
    m_visible = i.visible;
    m_detail = std::clamp(detail, 0.0, 1.0);
    m_detail = m_detail * m_detail * (3 - 2 * m_detail);
    if (!i.visible) {
        m_initialized = false;
        ribbon.reset();
        Q_EMIT changed();
        return;
    }
    if (!m_initialized || i.reduced || i.cinematic) {
        m_anchor = aim;
        m_column = i.origin;
        m_anchorVelocity = m_columnVelocity = { };
    } else {
        followVector(m_anchor, m_anchorVelocity, aim, dt);
        followVector(m_column, m_columnVelocity, i.origin, dt);
    }
    m_offset = offset;
    m_initialized = true;
    ribbon.update(i, dt);
    Q_EMIT changed();
}
double SelectionLighting::areaIntensity() const
{
    const double gain = ribbon.settings.mode == RowLightingMode::Area ? 1
            : ribbon.settings.mode == RowLightingMode::Hybrid         ? HybridAreaGain
                                                                      : 0;
    return m_visible ? m_palette.area * ribbon.settings.strength * gain * (1 - m_detail) : 0;
}
QVector4D SelectionLighting::rowSettings() const
{
    const auto &s = ribbon.settings;
    return { float(int(s.mode)), float(s.strength), float(s.width), float(s.offset) };
}
QVector3D SelectionLighting::areaPosition() const
{
    const auto &p = ribbon.column;
    return SceneVector{
        p.x, p.y + sceneGeometry::HingeHeight + sceneGeometry::CaseHeight / 2 + AreaHeightOffset,
        p.z + ribbon.settings.offset * sceneGeometry::RowSpacing + AreaLightForwardOffset
    }
            .qt();
}
QVector3D SelectionLighting::areaTarget() const
{
    const auto &p = ribbon.column;
    return SceneVector{ p.x,
                        p.y + sceneGeometry::HingeHeight + sceneGeometry::CaseHeight / 2
                                - AreaTargetDrop,
                        p.z + ribbon.settings.offset * sceneGeometry::RowSpacing }
            .qt();
}
QColor SelectionLighting::backgroundColor() const
{
    auto encode = [](double l) {
        return l <= SrgbEncodeKnee ? SrgbLinearSlope * l
                                   : SrgbScale * std::pow(l, 1 / SrgbExponent) - SrgbOffset;
    };
    return QColor::fromRgbF(encode(m_palette.background.x), encode(m_palette.background.y),
                            encode(m_palette.background.z));
}
}
