#pragma once
#include "TitleReel.h"
#include <QCanvasPainterItem>
#include <QtQml/qqmlregistration.h>
#include <atomic>
struct CanvasTitleStats
{
    std::atomic<int> renderers{ 0 };
    std::atomic<quint64> paints{ 0 }, syncs{ 0 };
    std::atomic<qint64> paintNs{ 0 }, syncNs{ 0 };
};
CanvasTitleStats &canvasTitleStats();
class CanvasTitleProbe : public QCanvasPainterItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(yalahoo::TitleReel *controller READ controller WRITE setController NOTIFY changed)
    Q_PROPERTY(QColor ink MEMBER ink NOTIFY changed)
    Q_PROPERTY(QColor muted MEMBER muted NOTIFY changed)
    Q_PROPERTY(bool offscreenRendering MEMBER offscreenRendering NOTIFY changed)
public:
    explicit CanvasTitleProbe(QQuickItem *parent = nullptr);
    yalahoo::TitleReel *controller() const;
    void setController(yalahoo::TitleReel *controller);
    QColor ink = Qt::black, muted = Qt::gray;
    bool offscreenRendering = false;
    QCanvasPainterItemRenderer *createItemRenderer() const override;
Q_SIGNALS:
    void changed();

private:
    void requestPaint();
    QMetaObject::Connection m_windowVisibility;
    QPointer<yalahoo::TitleReel> m_controller;
};
