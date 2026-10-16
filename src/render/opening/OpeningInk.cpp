#include "OpeningInk.h"
#include "BrandPaths.h"
#include <QCanvasPainterItemRenderer>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QPainterPathStroker>
#include <numbers>

namespace yalahoo {
namespace {
OpeningInkStats stats;
QString svgPath(const QPainterPath &p)
{
    QString result;
    auto point = [&](int i) {
        const auto e = p.elementAt(i);
        return QString::number(e.x, 'g', 17) + " " + QString::number(e.y, 'g', 17) + " ";
    };
    for (int i = 0; i < p.elementCount(); ++i) {
        const auto e = p.elementAt(i);
        if (e.isMoveTo())
            result += "M" + point(i);
        else if (e.isLineTo())
            result += "L" + point(i);
        else if (e.isCurveTo()) {
            result += "C" + point(i) + point(i + 1) + point(i + 2);
            i += 2;
        }
    }
    return result;
}
class Renderer final : public QCanvasPainterItemRenderer
{
public:
    Renderer()
    {
        setSharedPainter(false);
        ++stats.renderers;
    }
    ~Renderer() override
    {
        --stats.renderers;
    }

protected:
    void synchronizeData(QCanvasPainterItem *item) override
    {
        QElapsedTimer timer;
        timer.start();
        const auto *source = static_cast<OpeningInk *>(item);
        if (paths.isEmpty())
            paths = source->paths();
        state = openingFrame(source->time());
        orbit = source->orbit();
        ink = source->ink();
        accent = source->accent();
        if (size != source->size()) {
            size = source->size();
            rebuildPaths = true;
        }
        visible = source->isVisible();
        ++stats.syncs;
        stats.syncNs += timer.nsecsElapsed();
    }
    void paint(QCanvasPainter *p) override
    {
        QElapsedTimer timer;
        timer.start();
        if (!visible)
            return;
        p->resetClipping();
        if (rebuildPaths) {
            for (int i = 1; i <= 19; ++i)
                p->removePathGroup(i);
            for (int i = 101; i <= 104; ++i)
                p->removePathGroup(i);
            const double fit = std::min(size.width() / 816, size.height() / 208);
            brandTransform = QTransform::fromTranslate((size.width() - 816 * fit) / 2,
                                                       (size.height() - 208 * fit) / 2);
            brandTransform.scale(fit, fit);
            logicalPaths.clear();
            for (const auto &path : paths) {
                QCanvasPath logical;
                if (path.point) {
                    QPainterPathStroker outline;
                    outline.setWidth(path.width);
                    outline.setCapStyle(Qt::RoundCap);
                    logical.addPath(brandTransform.map(outline.createStroke(path.centerline)));
                } else
                    logical.addPath(path.full, brandTransform);
                logicalPaths.append(std::move(logical));
            }
            inner.clear();
            top.clear();
            bottom.clear();
            cross.clear();
            rebuildPaths = false;
        }
        if (orbit)
            drawOrbit(p);
        else
            drawMark(p);
        ++stats.paints;
        stats.paintNs += timer.nsecsElapsed();
    }

private:
    void drawMark(QCanvasPainter *p)
    {
        const double scale = std::min(size.width() / 816, size.height() / 208);
        // Bake the SVG-to-logical transform into cached geometry. The module's cached
        // stroke path also applies the state scale to stroke width before the vertex
        // transform; identity-scale drawing preserves the authored width and AA fringe.
        p->setLineCap(QCanvasPainter::LineCap::Round);
        p->setLineJoin(QCanvasPainter::LineJoin::Round);
        for (int i = 0; i < paths.size(); ++i) {
            const auto &path = paths[i];
            const double progress = path.decoration
                    ? double(state.contour == 1)
                    : std::clamp((state.contour - path.start) / (path.end - path.start), 0.0, 1.0);
            if (progress <= 0)
                continue;
            p->setGlobalAlpha(path.accent ? state.symbols : 1);
            p->setStrokeStyle(path.accent ? accent : ink);
            p->setLineWidth(path.width * scale);
            if (path.point) {
                p->setFillStyle(path.accent ? accent : ink);
                if (progress == 1)
                    p->fill(logicalPaths[i], i + 1);
                else {
                    QPainterPathStroker outline;
                    outline.setWidth(path.width);
                    outline.setCapStyle(Qt::RoundCap);
                    QCanvasPath partial;
                    partial.addPath(brandTransform.map(
                            outline.createStroke(path.centerline.trimmed(0, progress))));
                    p->fill(partial);
                }
            } else if (progress == 1)
                p->stroke(logicalPaths[i], i + 1);
            else {
                QCanvasPath partial;
                partial.addPath(brandTransform.map(path.centerline.trimmed(0, progress)));
                p->stroke(partial);
            }
        }
    }
    void drawOrbit(QCanvasPainter *p)
    {
        const double scale = size.width() / 520;
        p->setStrokeStyle(ink);
        p->setLineWidth(1.2 * scale);
        p->save();
        p->translate(size.width() / 2, size.height() / 2);
        p->rotate(state.orbitTurn * std::numbers::pi / 180);
        QCanvasPath outer;
        if (state.orbitDraw > 0) {
            outer.arc(0, 0, 236 * scale, 0, 2 * std::numbers::pi * state.orbitDraw);
            p->stroke(outer);
        }
        if (inner.isEmpty()) {
            // SVG dasharray repeats around circumference, starting at positive X.
            constexpr double circumference = 2 * std::numbers::pi * 207;
            double offset = 0;
            int dash = 0;
            const std::array<double, 4> lengths{ 142, 91, 380, 95 };
            while (offset < circumference) {
                const double end = std::min(circumference, offset + lengths[dash % 4]);
                if (dash % 2 == 0)
                    inner.arc(0, 0, 207 * scale, offset / 207, end / 207,
                              QCanvasPainter::PathWinding::ClockWise,
                              QCanvasPainter::PathConnection::NotConnected);
                offset = end;
                ++dash;
            }
            top.circle(0, -236 * scale, 4 * scale);
            bottom.circle(0, 236 * scale, 4 * scale);
            cross.moveTo(252 * scale, 260 * scale);
            cross.lineTo(268 * scale, 260 * scale);
            cross.moveTo(260 * scale, 252 * scale);
            cross.lineTo(260 * scale, 268 * scale);
        }
        p->stroke(inner, 101);
        p->setFillStyle(ink);
        p->fill(top, 102);
        p->setFillStyle(accent);
        p->fill(bottom, 103);
        p->restore();
        p->setLineWidth(1.1 * scale);
        p->stroke(cross, 104);
    }
    QVector<OpeningPath> paths;
    QVector<QCanvasPath> logicalPaths;
    QTransform brandTransform;
    bool rebuildPaths = true;
    QCanvasPath inner, top, bottom, cross;
    OpeningFrame state;
    QSizeF size;
    QColor ink, accent;
    bool orbit = false, visible = false;
};
}
OpeningInkStats &openingInkStats()
{
    return stats;
}
QVector<OpeningPath> loadOpeningPaths()
{
    QFile map(":/assets/branding/reveal-map.json"), svg(":/assets/branding/logo-light.svg");
    if (!map.open(QIODevice::ReadOnly) || !svg.open(QIODevice::ReadOnly))
        qFatal("Missing opening brand assets");
    QHash<QString, QJsonObject> entries;
    for (const auto value :
         QJsonDocument::fromJson(map.readAll()).object().value("paths").toArray()) {
        const auto entry = value.toObject();
        entries.insert(entry.value("id").toString(), entry);
    }
    QVector<OpeningPath> paths;
    QXmlStreamReader xml(svg.readAll());
    QString stroke;
    double width = 16;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        const auto a = xml.attributes();
        if (xml.name() == u"g") {
            stroke = a.value("stroke").toString();
            width = a.value("stroke-width").toDouble();
        } else if (xml.name() == u"path") {
            OpeningPath path;
            path.fullSvg = a.value("d").toString();
            path.full.addPath(path.fullSvg);
            const auto entry = entries.value(a.value("id").toString());
            path.decoration = entry.isEmpty();
            path.width =
                    a.hasAttribute("stroke-width") ? a.value("stroke-width").toDouble() : width;
            path.accent =
                    (a.hasAttribute("stroke") ? a.value("stroke").toString() : stroke) == "#b56834";
            if (!path.decoration) {
                path.centerline = parseBrandCenterline(entry.value("d").toString());
                path.point = path.centerline.length() < .02;
                path.start = entry.value("start").toDouble();
                path.end = entry.value("end").toDouble();
            }
            paths.append(std::move(path));
        }
    }
    if (xml.hasError() || entries.size() != 17 || paths.size() != 19)
        qFatal("Invalid opening brand mapping");
    return paths;
}
OpeningInk::OpeningInk(QQuickItem *parent) : QCanvasPainterItem(parent)
{
    static const auto paths = loadOpeningPaths();
    m_paths = paths;
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
    setSampleCount(1);
    connect(this, &QQuickItem::visibleChanged, this, &OpeningInk::invalidate);
}
void OpeningInk::invalidate()
{
    if (isVisible())
        update();
}
void OpeningInk::setTime(double value)
{
    if (!std::isfinite(value) || value == m_time)
        return;
    m_time = value;
    emit timeChanged();
    invalidate();
}
void OpeningInk::setOrbit(bool value)
{
    if (m_orbit == value)
        return;
    m_orbit = value;
    emit orbitChanged();
    invalidate();
}
void OpeningInk::setInk(QColor value)
{
    if (m_ink == value)
        return;
    m_ink = value;
    emit colorsChanged();
    invalidate();
}
void OpeningInk::setAccent(QColor value)
{
    if (m_accent == value)
        return;
    m_accent = value;
    emit colorsChanged();
    invalidate();
}
QVariantList OpeningInk::comparisonPaths() const
{
    const auto state = openingFrame(m_time);
    QVariantList result;
    for (const auto &path : m_paths) {
        const double progress = path.decoration
                ? double(state.contour == 1)
                : std::clamp((state.contour - path.start) / (path.end - path.start), 0.0, 1.0);
        result.append(QVariantMap{ { "d",
                                     progress == 1 ? path.fullSvg
                                             : progress > 0
                                             ? svgPath(path.centerline.trimmed(0, progress))
                                             : QString() },
                                   { "width", path.width },
                                   { "color", path.accent ? m_accent : m_ink },
                                   { "alpha", path.accent ? state.symbols : 1 } });
    }
    return result;
}
QCanvasPainterItemRenderer *OpeningInk::createItemRenderer() const
{
    return new Renderer;
}
}
