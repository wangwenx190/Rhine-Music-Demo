// Keeps Canvas Painter transparency and renderer accounting consistent across ink items.
// Each component owns its counters for the lifetime of its renderers; no QObject state is shared.

#pragma once
#include <QCanvasPainterItem>
#include <QCanvasPainterItemRenderer>
#include <atomic>

namespace yalahoo {
struct CanvasInkStats final
{
    std::atomic<quint64> paints{ 0 }, syncs{ 0 }, paintNs{ 0 }, syncNs{ 0 };
    std::atomic<int> renderers{ 0 };
};
class CanvasRenderer : public QCanvasPainterItemRenderer
{
    Q_DISABLE_COPY_MOVE(CanvasRenderer)
public:
    ~CanvasRenderer() override
    {
        --m_stats.renderers;
    }

protected:
    explicit CanvasRenderer(CanvasInkStats &stats) : m_stats(stats)
    {
        setSharedPainter(false);
        ++m_stats.renderers;
    }

private:
    CanvasInkStats &m_stats;
};
inline void configureTransparentCanvas(QCanvasPainterItem &item)
{
    item.setFillColor(Qt::transparent);
    item.setAlphaBlending(true);
    item.setSampleCount(1);
}
}
