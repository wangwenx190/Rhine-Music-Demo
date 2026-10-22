#include "Units.h"
#include "Logging.h"
#include "OpeningInk.h"
#include "BrandPaths.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QPainterPathStroker>
#include <numbers>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// View boxes and stable path IDs come from bundled brand geometry; ring dashes use source arc lengths.
constexpr auto ExpectedRevealEntries = 17;
constexpr auto LastBrandPathId = 19;
constexpr auto OrbitRingPathId = 101;
constexpr auto OrbitCrossPathId = 104;
constexpr auto BrandViewWidth = 816;
constexpr auto BrandViewHeight = 208;
constexpr auto OrbitViewWidth = 520;
constexpr auto OrbitLineWidthPixels = 1.2;
constexpr auto OuterOrbitRadius = 236;
constexpr auto InnerOrbitRadius = 207;
constexpr auto OrbitDotRadius = 4;
constexpr auto OrbitDashCount = 4;
constexpr auto CrossMinimum = 252;
constexpr auto CrossCenter = 260;
constexpr auto CrossMaximum = 268;
constexpr auto TopDotPathId = 102;
constexpr auto BottomDotPathId = 103;
constexpr auto CrossLineWidthPixels = 1.1;
constexpr auto DefaultPathWidth = 16;
constexpr auto PointPathLengthThreshold = .02;

CanvasInkStats stats;
class Renderer final : public CanvasRenderer
{
    Q_DISABLE_COPY_MOVE(Renderer)
public:
    Renderer() : CanvasRenderer(stats)
    {
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
            for (int i = 1; i <= LastBrandPathId; ++i)
                p->removePathGroup(i);
            for (int i = OrbitRingPathId; i <= OrbitCrossPathId; ++i)
                p->removePathGroup(i);
            const double fit =
                    std::min(size.width() / BrandViewWidth, size.height() / BrandViewHeight);
            brandTransform = QTransform::fromTranslate((size.width() - BrandViewWidth * fit) / 2,
                                                       (size.height() - BrandViewHeight * fit) / 2);
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
        const double scale =
                std::min(size.width() / BrandViewWidth, size.height() / BrandViewHeight);
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
        const double scale = size.width() / OrbitViewWidth;
        p->setStrokeStyle(ink);
        p->setLineWidth(OrbitLineWidthPixels * scale);
        p->save();
        p->translate(size.width() / 2, size.height() / 2);
        p->rotate(state.orbitTurn * std::numbers::pi / yalahoo::units::DegreesPerHalfTurn);
        QCanvasPath outer;
        if (state.orbitDraw > 0) {
            outer.arc(0, 0, OuterOrbitRadius * scale, 0, 2 * std::numbers::pi * state.orbitDraw);
            p->stroke(outer);
        }
        if (inner.isEmpty()) {
            // SVG dasharray repeats around circumference, starting at positive X.
            constexpr double circumference = 2 * std::numbers::pi * InnerOrbitRadius;
            double offset = 0;
            int dash = 0;
            const std::array<double, OrbitDashCount> lengths{ 142, 91, 380, 95 };
            while (offset < circumference) {
                const double end = std::min(circumference, offset + lengths[dash % OrbitDashCount]);
                if (dash % 2 == 0)
                    inner.arc(0, 0, InnerOrbitRadius * scale, offset / InnerOrbitRadius,
                              end / InnerOrbitRadius, QCanvasPainter::PathWinding::ClockWise,
                              QCanvasPainter::PathConnection::NotConnected);
                offset = end;
                ++dash;
            }
            top.circle(0, -OuterOrbitRadius * scale, OrbitDotRadius * scale);
            bottom.circle(0, OuterOrbitRadius * scale, OrbitDotRadius * scale);
            cross.moveTo(CrossMinimum * scale, CrossCenter * scale);
            cross.lineTo(CrossMaximum * scale, CrossCenter * scale);
            cross.moveTo(CrossCenter * scale, CrossMinimum * scale);
            cross.lineTo(CrossCenter * scale, CrossMaximum * scale);
        }
        p->stroke(inner, OrbitRingPathId);
        p->setFillStyle(ink);
        p->fill(top, TopDotPathId);
        p->setFillStyle(accent);
        p->fill(bottom, BottomDotPathId);
        p->restore();
        p->setLineWidth(CrossLineWidthPixels * scale);
        p->stroke(cross, OrbitCrossPathId);
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
CanvasInkStats &openingInkStats()
{
    return stats;
}
QVector<OpeningPath> loadOpeningPaths()
{
    QFile map(u":/assets/branding/reveal-map.json"_s), svg(u":/assets/branding/logo-light.svg"_s);
    if (!map.open(QIODevice::ReadOnly) || !svg.open(QIODevice::ReadOnly))
        qCFatal(yalahoo::logRender, "Missing opening brand assets");
    QHash<QString, QJsonObject> entries;
    for (const auto value :
         QJsonDocument::fromJson(map.readAll()).object().value(u"paths"_s).toArray()) {
        const auto entry = value.toObject();
        entries.insert(entry.value(u"id"_s).toString(), entry);
    }
    QVector<OpeningPath> paths;
    QXmlStreamReader xml(svg.readAll());
    QString stroke;
    double width = DefaultPathWidth;
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
            path.full.addPath(a.value("d").toString());
            const auto entry = entries.value(a.value("id").toString());
            path.decoration = entry.isEmpty();
            path.width =
                    a.hasAttribute("stroke-width") ? a.value("stroke-width").toDouble() : width;
            path.accent = (a.hasAttribute("stroke") ? a.value("stroke").toString() : stroke)
                    == "#b56834"_L1;
            if (!path.decoration) {
                try {
                    path.centerline = parseBrandCenterline(entry.value(u"d"_s).toString());
                } catch (const std::exception &error) {
                    qCFatal(yalahoo::logRender, "Invalid embedded opening centerline: %s",
                            error.what());
                }
                // The immutable centerline is trimmed on every reveal frame. Build
                // its arc-length cache once, before sharing it with renderers.
                path.centerline.setCachingEnabled(true);
                path.point = path.centerline.length() < PointPathLengthThreshold;
                path.start = entry.value(u"start"_s).toDouble();
                path.end = entry.value(u"end"_s).toDouble();
            }
            paths.append(std::move(path));
        }
    }
    if (xml.hasError() || entries.size() != ExpectedRevealEntries
        || paths.size() != LastBrandPathId)
        qCFatal(yalahoo::logRender, "Invalid opening brand mapping");
    return paths;
}
OpeningInk::OpeningInk(QQuickItem *parent) : QCanvasPainterItem(parent)
{
    static const auto paths = loadOpeningPaths();
    m_paths = paths;
    configureTransparentCanvas(*this);
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
    Q_EMIT timeChanged();
    invalidate();
}
void OpeningInk::setOrbit(bool value)
{
    if (m_orbit == value)
        return;
    m_orbit = value;
    Q_EMIT orbitChanged();
    invalidate();
}
void OpeningInk::setInk(QColor value)
{
    if (m_ink == value)
        return;
    m_ink = value;
    Q_EMIT colorsChanged();
    invalidate();
}
void OpeningInk::setAccent(QColor value)
{
    if (m_accent == value)
        return;
    m_accent = value;
    Q_EMIT colorsChanged();
    invalidate();
}
QCanvasPainterItemRenderer *OpeningInk::createItemRenderer() const
{
    return new Renderer;
}
}
