#pragma once
#include "RulerController.h"
#include <QCanvasPainterItem>
#include <atomic>

namespace yalahoo {
struct RulerInkStats final
{
    std::atomic<quint64> paints{ 0 }, syncs{ 0 }, paintNs{ 0 }, syncNs{ 0 };
    std::atomic<int> renderers{ 0 };
};
RulerInkStats &rulerInkStats();
class RulerInk : public QCanvasPainterItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RulerInk)
    Q_PROPERTY(yalahoo::RulerController *controller READ controller WRITE setController NOTIFY controllerChanged FINAL)
    Q_PROPERTY(QColor ink READ ink WRITE setInk NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY colorsChanged FINAL)
public:
    explicit RulerInk(QQuickItem *parent = nullptr);
    RulerController *controller() const;
    void setController(RulerController *controller);
    QColor ink() const;
    void setInk(QColor color);
    QColor accent() const;
    void setAccent(QColor color);
    QCanvasPainterItemRenderer *createItemRenderer() const override;
Q_SIGNALS:
    void controllerChanged();
    void colorsChanged();

private:
    QPointer<RulerController> m_controller;
    QMetaObject::Connection m_changed;
    QColor m_ink, m_accent;
};
}
