#pragma once
#include "TitleReel.h"
#include <QQuickItem>
#include <QColor>

namespace yalahoo {
class ReelInk : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(yalahoo::ReelSlot *slot READ slot WRITE setSlot NOTIFY slotChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit ReelInk(QQuickItem *parent = nullptr);
    ReelSlot *slot() const;
    void setSlot(ReelSlot *slot);
    QColor color() const;
    void setColor(const QColor &color);
    static int liveNodes();
    static quint64 geometryUpdates();
    static quint64 geometryReuses();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
signals:
    void slotChanged();
    void colorChanged();

private:
    QPointer<ReelSlot> m_slot;
    QColor m_color = Qt::black;
};
}
