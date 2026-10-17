#pragma once
#include <QCanvasPainterItem>
#include <QtQml/qqmlregistration.h>
#include <atomic>

struct CanvasProbeStats
{
    std::atomic<quint64> paints{ 0 }, synchronizations{ 0 }, renderers{ 0 };
    std::atomic<qint64> paintNanoseconds{ 0 }, syncNanoseconds{ 0 }, imageBytes{ 0 },
            imageCount{ 0 };
};
CanvasProbeStats &canvasProbeStats();

class CanvasProbe : public QCanvasPainterItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool night READ night WRITE setNight NOTIFY nightChanged)
public:
    explicit CanvasProbe(QQuickItem *parent = nullptr);
    bool night() const;
    void setNight(bool night);
    QCanvasPainterItemRenderer *createItemRenderer() const override;
Q_SIGNALS:
    void nightChanged();

private:
    bool m_night = false;
};
