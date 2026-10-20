#include "CanvasTitleProbe.h"
#include <QCanvasPainterItemRenderer>
#include <QCanvasLinearGradient>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QQuickWindow>
namespace {
CanvasTitleStats stats;
struct Slot final
{
    QList<yalahoo::ReelSlot::Face> faces;
    double x = 0, y = 0, height = 0, roll = 0, size = 0;
    bool translation = false;
};
class Renderer final : public QCanvasPainterItemRenderer
{
    Q_DISABLE_COPY_MOVE(Renderer)
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
        auto *probe = static_cast<CanvasTitleProbe *>(item);
        m_visible = probe->isVisible()
                && (probe->offscreenRendering || !probe->window() || probe->window()->isVisible());
        ink = probe->ink;
        muted = probe->muted;
        m_slots.clear();
        if (!m_visible)
            return;
        if (auto *controller = probe->controller())
            for (int i = 0; i < controller->slotCount(); ++i) {
                const auto *slot = controller->slotAt(i);
                if (slot->shown())
                    m_slots.append({ slot->faces, slot->positionX(), slot->positionY(),
                                     slot->viewportHeight(), slot->rollPosition(),
                                     slot->pixelSize(), slot->translation() });
            }
        ++stats.syncs;
        stats.syncNs += timer.nsecsElapsed();
    }
    void paint(QCanvasPainter *p) override
    {
        QElapsedTimer timer;
        timer.start();
        p->reset();
        if (!m_visible)
            return;
        p->setTextBaseline(QCanvasPainter::TextBaseline::Alphabetic);
        for (const auto &slot : m_slots) {
            const QColor color = slot.translation ? muted : ink;
            for (const auto &face : slot.faces) {
                const auto &glyph = face.glyph;
                if (glyph.text.isEmpty())
                    continue;
                p->save();
                p->setClipRect(slot.x - slot.size * .15, slot.y, glyph.width + slot.size * .3 + .3,
                               slot.height);
                p->translate(slot.x, slot.y + face.offset - slot.roll);
                p->scale(glyph.scale, glyph.scale);
                p->setFont(glyph.font);
                const double top = (slot.roll - face.offset) / glyph.scale;
                const double bottom = top + slot.height / glyph.scale;
                const double fade = slot.size * .02 / glyph.scale;
                QCanvasLinearGradient gradient(0, top, 0, bottom);
                QColor transparent = color;
                transparent.setAlpha(0);
                gradient.setStartColor(transparent);
                gradient.addColorStop(fade / (bottom - top), color);
                gradient.addColorStop(1 - fade / (bottom - top), color);
                gradient.setEndColor(transparent);
                p->setFillStyle(gradient);
                const QFontMetricsF metrics(glyph.font);
                const double ascent = metrics.ascent() * glyph.scale;
                const double descent = metrics.descent() * glyph.scale;
                const double baseline =
                        (glyph.height - std::round(ascent) - std::round(descent)) / 2
                        + std::round(ascent);
                p->fillText(glyph.text, 0, baseline / glyph.scale);
                p->restore();
                p->resetClipping();
            }
        }
        ++stats.paints;
        stats.paintNs += timer.nsecsElapsed();
    }

private:
    QList<Slot> m_slots;
    QColor ink, muted;
    bool m_visible = false;
};
}
CanvasTitleStats &canvasTitleStats()
{
    return stats;
}
CanvasTitleProbe::CanvasTitleProbe(QQuickItem *parent) : QCanvasPainterItem(parent)
{
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
    setSampleCount(1);
    connect(this, &CanvasTitleProbe::changed, this, &CanvasTitleProbe::requestPaint);
    connect(this, &QQuickItem::visibleChanged, this, &CanvasTitleProbe::requestPaint);
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
        disconnect(m_windowVisibility);
        if (window)
            m_windowVisibility = connect(window, &QWindow::visibleChanged, this,
                                         &CanvasTitleProbe::requestPaint);
        requestPaint();
    });
}
void CanvasTitleProbe::requestPaint()
{
    if (isVisible())
        update();
}
yalahoo::TitleReel *CanvasTitleProbe::controller() const
{
    return m_controller;
}
void CanvasTitleProbe::setController(yalahoo::TitleReel *controller)
{
    if (m_controller == controller)
        return;
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    if (controller)
        connect(controller, &yalahoo::TitleReel::changed, this, &CanvasTitleProbe::requestPaint);
    Q_EMIT changed();
}
QCanvasPainterItemRenderer *CanvasTitleProbe::createItemRenderer() const
{
    return new Renderer;
}
