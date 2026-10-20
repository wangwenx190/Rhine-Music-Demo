#include "RulerInk.h"
#include <QCanvasPainterItemRenderer>
#include <QCanvasLinearGradient>
#include <QCanvasPath>
#include <QElapsedTimer>

namespace {
// Cache the three authored tick shapes; per-frame state only selects and transforms them.
constexpr auto NarrowRestingHeight = 8.0;
constexpr auto RestingHeight = 10.0;
constexpr auto NarrowActiveHeight = 21.0;
constexpr auto ActiveHeight = 30.0;
constexpr auto NarrowHoverHeight = 16.0;
constexpr auto HoverHeight = 22.0;
}

namespace yalahoo {
namespace {
RulerInkStats stats;
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
        const auto *ruler = static_cast<RulerInk *>(item);
        drawing = ruler->controller() ? ruler->controller()->drawing() : RulerDrawing{ };
        if (cachedTick != drawing.tick) {
            cachedTick = drawing.tick;
            invalidatePaths = true;
            const bool narrow = cachedTick == 2;
            heights = { narrow ? NarrowRestingHeight : RestingHeight,
                        narrow ? NarrowActiveHeight : ActiveHeight,
                        narrow ? NarrowHoverHeight : HoverHeight };
            for (int i = 0; i < 3; ++i) {
                rects[i].clear();
                rects[i].rect(0, 0, drawing.tick, heights[i]);
            }
        }
        ink = ruler->ink();
        accent = ruler->accent();
        ++stats.syncs;
        stats.syncNs += timer.nsecsElapsed();
    }
    void paint(QCanvasPainter *p) override
    {
        QElapsedTimer timer;
        timer.start();
        if (invalidatePaths) {
            for (int group = 1; group <= 3; ++group)
                p->removePathGroup(group);
            invalidatePaths = false;
        }
        p->resetClipping();
        for (const auto &mark : drawing.marks) {
            if (mark.alpha <= 0 || mark.x + drawing.tick <= 0 || mark.x >= drawing.width)
                continue;
            const QColor color = mark.hovering && !mark.active ? accent : ink;
            p->save();
            p->setGlobalAlpha(float(mark.alpha));
            // The source mask multiplies alpha continuously across each tick's edge.
            if (drawing.overflow
                && (mark.x < drawing.gap || mark.x + drawing.tick > drawing.width - drawing.gap)) {
                QCanvasLinearGradient fade(0, 0, drawing.width, 0);
                QColor clear = color;
                clear.setAlpha(0);
                fade.addColorStop(0, clear);
                fade.addColorStop(float(drawing.gap / drawing.width), color);
                fade.addColorStop(float(1 - drawing.gap / drawing.width), color);
                fade.addColorStop(1, clear);
                p->setFillStyle(fade);
                p->fillRect(mark.x, (drawing.height - mark.height) / 2, drawing.tick, mark.height);
            } else {
                p->setFillStyle(color);
                p->translate(mark.x, (drawing.height - mark.height) / 2);
                const auto cached = std::find(heights.begin(), heights.end(), mark.height);
                if (cached != heights.end()) {
                    const auto index = std::distance(heights.begin(), cached);
                    p->fill(rects[index], quint32(index + 1));
                } else {
                    // Never stretch a tessellated AA fringe with the tick's animated height.
                    p->fillRect(0, 0, drawing.tick, mark.height);
                }
            }
            p->restore();
        }
        ++stats.paints;
        stats.paintNs += timer.nsecsElapsed();
    }

private:
    RulerDrawing drawing;
    QColor ink, accent;
    std::array<QCanvasPath, 3> rects;
    std::array<double, 3> heights;
    double cachedTick = 0;
    bool invalidatePaths = false;
};
}
RulerInkStats &rulerInkStats()
{
    return stats;
}
RulerInk::RulerInk(QQuickItem *parent) : QCanvasPainterItem(parent)
{
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
    setSampleCount(1);
}
RulerController *RulerInk::controller() const
{
    return m_controller;
}
void RulerInk::setController(RulerController *controller)
{
    if (m_controller == controller)
        return;
    disconnect(m_changed);
    m_controller = controller;
    if (controller)
        m_changed = connect(controller, &RulerController::changed, this, [this] {
            if (isVisible())
                update();
        });
    Q_EMIT controllerChanged();
    update();
}
QColor RulerInk::ink() const
{
    return m_ink;
}
void RulerInk::setInk(QColor color)
{
    if (m_ink == color)
        return;
    m_ink = color;
    Q_EMIT colorsChanged();
    update();
}
QColor RulerInk::accent() const
{
    return m_accent;
}
void RulerInk::setAccent(QColor color)
{
    if (m_accent == color)
        return;
    m_accent = color;
    Q_EMIT colorsChanged();
    update();
}
QCanvasPainterItemRenderer *RulerInk::createItemRenderer() const
{
    return new Renderer;
}
}
