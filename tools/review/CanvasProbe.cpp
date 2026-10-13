#include "CanvasProbe.h"
#include "AssetCatalog.h"
#include <QCanvasPainterItemRenderer>
#include <QCanvasLinearGradient>
#include <QCanvasPath>
#include <QElapsedTimer>

namespace {
CanvasProbeStats stats;
class ProbeRenderer final : public QCanvasPainterItemRenderer
{
public:
    ProbeRenderer()
    {
        setSharedPainter(false);
        ++stats.renderers;
        curve.addPath(u"M16 60 C40 12 76 108 112 60");
        font = yalahoo::assetFont(400);
        font.setPixelSize(20);
    }
    ~ProbeRenderer() override
    {
        --stats.renderers;
    }

protected:
    void synchronizeData(QCanvasPainterItem *item) override
    {
        QElapsedTimer timer;
        timer.start();
        night = static_cast<CanvasProbe *>(item)->night();
        ++stats.synchronizations;
        stats.syncNanoseconds += timer.nsecsElapsed();
    }
    void paint(QCanvasPainter *p) override
    {
        QElapsedTimer timer;
        timer.start();
        const QColor ink(night ? "#e9eef2" : "#232722");
        p->resetClipping();
        p->setStrokeStyle(ink);
        p->setLineWidth(3);
        p->setLineCap(QCanvasPainter::LineCap::Round);
        p->stroke(curve, 1);
        QCanvasLinearGradient gradient(136, 20, 232, 80);
        gradient.setStartColor(QColor("#b56834"));
        gradient.setEndColor(QColor(217, 173, 125, 100));
        p->setFillStyle(gradient);
        p->fillRect(136, 20, 96, 60);
        p->save();
        p->setClipRect(256, 20, 80, 60);
        p->setFillStyle(QColor(ink.red(), ink.green(), ink.blue(), 128));
        p->fillRect(244, 10, 110, 100);
        p->restore();
        p->resetClipping();
        p->setFillStyle(ink);
        p->setFont(font);
        p->setTextBaseline(QCanvasPainter::TextBaseline::Alphabetic);
        p->fillText(QStringLiteral("音乐收藏  gyp  é  🎵"), 16, 128);
        stats.imageBytes = p->activeImageMemoryUsage();
        stats.imageCount = p->activeImageCount();
        ++stats.paints;
        stats.paintNanoseconds += timer.nsecsElapsed();
    }

private:
    bool night = false;
    QFont font;
    QCanvasPath curve;
};
}
CanvasProbeStats &canvasProbeStats()
{
    return stats;
}
CanvasProbe::CanvasProbe(QQuickItem *parent) : QCanvasPainterItem(parent)
{
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
    setSampleCount(1);
}
bool CanvasProbe::night() const
{
    return m_night;
}
void CanvasProbe::setNight(bool night)
{
    if (m_night == night)
        return;
    m_night = night;
    emit nightChanged();
    update();
}
QCanvasPainterItemRenderer *CanvasProbe::createItemRenderer() const
{
    return new ProbeRenderer;
}
