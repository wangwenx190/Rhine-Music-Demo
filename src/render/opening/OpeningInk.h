// Renders the opening brand paths with Qt Canvas Painter. Immutable path snapshots cross the
// GUI/render boundary and hidden items stop visual work.

#pragma once
#include "OpeningFrame.h"
#include "CanvasSupport.h"
#include <QCanvasPath>
#include <QPainterPath>

using Qt::StringLiterals::operator""_L1;

namespace yalahoo {
namespace openingInkMetrics {
// Stroke widths are source SVG view-box units before logical viewport scaling.
inline constexpr auto DefaultStrokeWidth = 16;
}

struct OpeningPath final
{
    QPainterPath centerline;
    QCanvasPath full;
    double start = 0, end = 1, width = openingInkMetrics::DefaultStrokeWidth;
    bool accent = false, decoration = false, point = false;
};
CanvasInkStats &openingInkStats();
QVector<OpeningPath> loadOpeningPaths();
class OpeningInk : public QCanvasPainterItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OpeningInk)
    Q_PROPERTY(double time READ time WRITE setTime NOTIFY timeChanged FINAL)
    Q_PROPERTY(bool orbit READ orbit WRITE setOrbit NOTIFY orbitChanged FINAL)
    Q_PROPERTY(QColor ink READ ink WRITE setInk NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY colorsChanged FINAL)
public:
    explicit OpeningInk(QQuickItem *parent = nullptr);
    double time() const
    {
        return m_time;
    }
    void setTime(double value);
    bool orbit() const
    {
        return m_orbit;
    }
    void setOrbit(bool value);
    QColor ink() const
    {
        return m_ink;
    }
    void setInk(QColor value);
    QColor accent() const
    {
        return m_accent;
    }
    void setAccent(QColor value);
    const QVector<OpeningPath> &paths() const
    {
        return m_paths;
    }
    QCanvasPainterItemRenderer *createItemRenderer() const override;
Q_SIGNALS:
    void timeChanged();
    void orbitChanged();
    void colorsChanged();

private:
    void invalidate();
    QVector<OpeningPath> m_paths;
    double m_time = 0;
    bool m_orbit = false;
    QColor m_ink{ "#232722"_L1 }, m_accent{ "#b56834"_L1 };
};
}
