#pragma once
#include "OpeningFrame.h"
#include <QCanvasPainterItem>
#include <QCanvasPath>
#include <QPainterPath>
#include <atomic>

using Qt::StringLiterals::operator""_L1;

namespace yalahoo {
struct OpeningPath
{
    QPainterPath centerline;
    QCanvasPath full;
    QString fullSvg;
    double start = 0, end = 1, width = 16;
    bool accent = false, decoration = false, point = false;
};
struct OpeningInkStats
{
    std::atomic<quint64> paints{ 0 }, syncs{ 0 }, paintNs{ 0 }, syncNs{ 0 };
    std::atomic<int> renderers{ 0 };
};
OpeningInkStats &openingInkStats();
QVector<OpeningPath> loadOpeningPaths();
class OpeningInk : public QCanvasPainterItem
{
    Q_OBJECT
    Q_PROPERTY(double time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(bool orbit READ orbit WRITE setOrbit NOTIFY orbitChanged)
    Q_PROPERTY(QColor ink READ ink WRITE setInk NOTIFY colorsChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY colorsChanged)
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
    Q_INVOKABLE QVariantList comparisonPaths() const;
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
