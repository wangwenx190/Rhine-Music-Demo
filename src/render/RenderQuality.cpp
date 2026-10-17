#include "RenderQuality.h"
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
QVariantMap RenderQuality::authoredPreset(const QString &name)
{
    QVariantMap v{ { u"scale"_s, 100 },          { u"pixelRatio"_s, 1.5 },
                   { u"antialias"_s, u"off"_s }, { u"shadows"_s, 2048 },
                   { u"aoSamples"_s, 32 },       { u"aoResolution"_s, 1.0 },
                   { u"depthOfField"_s, 100 },   { u"transmission"_s, 1.0 },
                   { u"anisotropy"_s, 16 } };
    if (name == "performance"_L1) {
        v[u"scale"_s] = 80;
        v[u"pixelRatio"_s] = 1;
        v[u"shadows"_s] = 1024;
        v[u"aoSamples"_s] = 0;
        v[u"aoResolution"_s] = .5;
        v[u"depthOfField"_s] = 0;
        v[u"transmission"_s] = .5;
        v[u"anisotropy"_s] = 4;
    } else if (name == "high"_L1 || name == "ultra"_L1) {
        v[u"scale"_s] = name == "high"_L1 ? 125 : 150;
        v[u"pixelRatio"_s] = 2;
        v[u"antialias"_s] = u"smaa"_s;
        v[u"shadows"_s] = 4096;
        v[u"aoSamples"_s] = name == "high"_L1 ? 32 : 64;
    }
    return v;
}
RenderQuality::RenderQuality(QObject *parent)
    : QObject(parent), m_values(authoredPreset(u"original"_s))
{
}
QString RenderQuality::preset() const
{
    for (const auto &name : { u"performance"_s, u"original"_s, u"high"_s, u"ultra"_s })
        if (m_values == authoredPreset(name))
            return name;
    return u"custom"_s;
}
void RenderQuality::setPreset(const QString &name)
{
    if (name == "performance"_L1 || name == "original"_L1 || name == "high"_L1
        || name == "ultra"_L1)
        configure(authoredPreset(name));
}
void RenderQuality::configure(const QVariantMap &input)
{
    auto result = authoredPreset(u"original"_s);
    auto number = [&](const QString &key) {
        const auto v = input.value(key);
        bool ok = false;
        const double n = v.toDouble(&ok);
        return v.metaType().id() != QMetaType::QString && v.metaType().id() != QMetaType::Bool && ok
                        && std::isfinite(n)
                ? n
                : result[key].toDouble();
    };
    for (auto key : { u"scale"_s, u"depthOfField"_s }) {
        const double n = number(key);
        result[key] = std::clamp(std::floor(n / 5 + .5) * 5, key == u"scale"_s ? 50.0 : 0.0,
                                 key == u"scale"_s ? 200.0 : 150.0);
    }
    const QList<QPair<QString, QList<double>>> choices{
        { u"pixelRatio"_s, { 1, 1.5, 2, 3 } },      { u"shadows"_s, { 0, 1024, 2048, 4096 } },
        { u"aoSamples"_s, { 0, 16, 32, 64 } },      { u"aoResolution"_s, { .5, .75, 1 } },
        { u"transmission"_s, { .25, .5, .75, 1 } }, { u"anisotropy"_s, { 1, 2, 4, 8, 16 } }
    };
    for (const auto &c : choices) {
        const double n = number(c.first);
        if (c.second.contains(n))
            result[c.first] = n;
    }
    if (input[u"antialias"_s] == u"off"_s || input[u"antialias"_s] == u"smaa"_s)
        result[u"antialias"_s] = input[u"antialias"_s];
    if (result == m_values)
        return;
    m_values = result;
    measure();
    Q_EMIT changed();
}
void RenderQuality::resize(double w, double h, double stage, double dpr, int maximum,
                           bool nativeDpr)
{
    if (!std::isfinite(w) || !std::isfinite(h) || !std::isfinite(stage) || !std::isfinite(dpr)
        || w <= 0 || h <= 0 || stage <= 0 || dpr <= 0 || maximum <= 0)
        return;
    m_width = w;
    m_height = h;
    m_stageScale = stage;
    m_dpr = dpr;
    m_maxTexture = maximum;
    m_nativeDpr = nativeDpr;
    const auto old = m_size;
    const auto limited = m_limited;
    measure();
    if (old != m_size || limited != m_limited)
        Q_EMIT changed();
}
void RenderQuality::measure()
{
    const double requested =
            (m_nativeDpr ? m_dpr : std::min(m_dpr, m_values[u"pixelRatio"_s].toDouble()))
            * m_stageScale * m_values[u"scale"_s].toDouble() / 100;
    const double ratio =
            std::min({ requested, std::sqrt(8294400 / std::max(1.0, m_width * m_height)),
                       m_maxTexture / std::max({ 1.0, m_width, m_height }) });
    m_size = { std::max(1, int(std::floor(m_width * ratio))),
               std::max(1, int(std::floor(m_height * ratio))) };
    m_limited = ratio < requested - .0001;
}
QSize RenderQuality::aoSize() const
{
    return { std::max(1, int(std::floor(m_size.width() * aoResolution()))),
             std::max(1, int(std::floor(m_size.height() * aoResolution()))) };
}
}
