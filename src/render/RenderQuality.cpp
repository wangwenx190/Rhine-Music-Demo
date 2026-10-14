#include "RenderQuality.h"
#include <algorithm>
#include <cmath>
namespace yalahoo {
QVariantMap RenderQuality::authoredPreset(const QString &name)
{
    QVariantMap v{ { "scale", 100 },        { "pixelRatio", 1.5 },   { "antialias", "off" },
                   { "shadows", 2048 },     { "aoSamples", 32 },     { "aoResolution", 1.0 },
                   { "depthOfField", 100 }, { "transmission", 1.0 }, { "anisotropy", 16 } };
    if (name == "performance") {
        v["scale"] = 80;
        v["pixelRatio"] = 1;
        v["shadows"] = 1024;
        v["aoSamples"] = 0;
        v["aoResolution"] = .5;
        v["depthOfField"] = 0;
        v["transmission"] = .5;
        v["anisotropy"] = 4;
    } else if (name == "high" || name == "ultra") {
        v["scale"] = name == "high" ? 125 : 150;
        v["pixelRatio"] = 2;
        v["antialias"] = "smaa";
        v["shadows"] = 4096;
        v["aoSamples"] = name == "high" ? 32 : 64;
    }
    return v;
}
RenderQuality::RenderQuality(QObject *parent)
    : QObject(parent), m_values(authoredPreset("original"))
{
}
QString RenderQuality::preset() const
{
    for (auto name : { "performance", "original", "high", "ultra" })
        if (m_values == authoredPreset(name))
            return name;
    return "custom";
}
void RenderQuality::setPreset(const QString &name)
{
    if (name == "performance" || name == "original" || name == "high" || name == "ultra")
        configure(authoredPreset(name));
}
void RenderQuality::configure(const QVariantMap &input, bool legacyHigh)
{
    auto result = authoredPreset("original");
    if (!legacyHigh) {
        result["pixelRatio"] = 1;
        result["aoSamples"] = 0;
        result["depthOfField"] = 0;
    }
    auto number = [&](const char *key) {
        const auto v = input.value(key);
        bool ok = false;
        const double n = v.toDouble(&ok);
        return v.metaType().id() != QMetaType::QString && v.metaType().id() != QMetaType::Bool && ok
                        && std::isfinite(n)
                ? n
                : result[key].toDouble();
    };
    for (auto key : { "scale", "depthOfField" }) {
        const double n = number(key);
        result[key] = std::clamp(std::floor(n / 5 + .5) * 5, key == QString("scale") ? 50.0 : 0.0,
                                 key == QString("scale") ? 200.0 : 150.0);
    }
    const QList<QPair<QString, QList<double>>> choices{
        { "pixelRatio", { 1, 1.5, 2, 3 } },      { "shadows", { 0, 1024, 2048, 4096 } },
        { "aoSamples", { 0, 16, 32, 64 } },      { "aoResolution", { .5, .75, 1 } },
        { "transmission", { .25, .5, .75, 1 } }, { "anisotropy", { 1, 2, 4, 8, 16 } }
    };
    for (const auto &c : choices) {
        const double n = number(c.first.toUtf8().constData());
        if (c.second.contains(n))
            result[c.first] = n;
    }
    if (input["antialias"] == "off" || input["antialias"] == "smaa")
        result["antialias"] = input["antialias"];
    if (result == m_values)
        return;
    m_values = result;
    measure();
    emit changed();
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
        emit changed();
}
void RenderQuality::measure()
{
    const double requested =
            (m_nativeDpr ? m_dpr : std::min(m_dpr, m_values["pixelRatio"].toDouble()))
            * m_stageScale * m_values["scale"].toDouble() / 100;
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
