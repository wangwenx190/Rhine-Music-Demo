#include "RowLightController.h"
#include "SelectionLighting.h"
#include <QVector4D>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
SceneVector linear(QLatin1StringView hex)
{
    const QColor c(hex);
    auto decode = [](double s) {
        return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
    };
    return { decode(c.redF()), decode(c.greenF()), decode(c.blueF()) };
}
void followVector(SceneVector &value, SceneVector &velocity, SceneVector target, double dt)
{
    const double decay = std::exp(-5 * dt);
    const auto delta = value - target, impulse = velocity + delta * 5;
    value = target + (delta + impulse * dt) * decay;
    velocity = (velocity - impulse * (5 * dt)) * decay;
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
    p.environment = night ? .16 : .25;
    p.key = night ? .30 : .45;
    p.hemisphere = night ? .21 : .32;
    p.spot = (night ? 130 : 180) * 64;
    p.area = night ? 24 : 34;
    p.scatter = night ? .72 : 1;
    p.printAmbient = night ? .12 : .5;
    p.exposure = night ? 1.08 : 1.05;
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
double SelectionLighting::themeProgress(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    double c = t;
    for (int n = 0; n < 6; ++n)
        c = std::clamp(c - (((1.6 * c - 1.8) * c + 1.2) * c - t) / ((4.8 * c - 3.6) * c + 1.2), 0.0,
                       1.0);
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
        m_elapsed = .65;
    }
    Q_EMIT nightChanged();
    Q_EMIT changed();
}
void SelectionLighting::setReducedMotion(bool reduced)
{
    m_reduced = reduced;
    if (reduced) {
        m_palette = m_to;
        m_elapsed = .65;
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
    if (int(s.mode) < 0 || int(s.mode) > 3)
        s.mode = m_settings.mode;
    s.strength = valid(s.strength, m_settings.strength, 0, 2);
    s.width = valid(s.width, m_settings.width, .6, 2.5);
    s.offset = valid(s.offset, m_settings.offset, -6, 6);
    s.fill = valid(s.fill, m_settings.fill, 0, 1);
    s.transitionDuration = valid(s.transitionDuration, m_settings.transitionDuration, .4, 2.4);
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
    if (!std::isfinite(dt) || dt < 0 || m_elapsed >= .65)
        return;
    m_elapsed = std::min(.65, m_elapsed + dt);
    const double t = themeProgress(m_elapsed / .65);
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
            : ribbon.settings.mode == RowLightingMode::Hybrid         ? .3
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
    return SceneVector{ p.x, p.y + 1.85 + 3.35 / 2 + .5, p.z + ribbon.settings.offset * .62 + .44 }
            .qt();
}
QVector3D SelectionLighting::areaTarget() const
{
    const auto &p = ribbon.column;
    return SceneVector{ p.x, p.y + 1.85 + 3.35 / 2 - .25, p.z + ribbon.settings.offset * .62 }.qt();
}
QColor SelectionLighting::backgroundColor() const
{
    auto encode = [](double l) {
        return l <= .0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - .055;
    };
    return QColor::fromRgbF(encode(m_palette.background.x), encode(m_palette.background.y),
                            encode(m_palette.background.z));
}
}
