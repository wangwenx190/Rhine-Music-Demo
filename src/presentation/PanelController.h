#pragma once
#include "SurfaceTransition.h"
namespace yalahoo {
class PanelController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString panel READ panel NOTIFY changed)
    Q_PROPERTY(yalahoo::SurfaceTransition *surface READ surface CONSTANT)
public:
    explicit PanelController(FrameClock *clock, QObject *parent = nullptr);
    QString panel() const
    {
        return m_panel;
    }
    SurfaceTransition *surface()
    {
        return &m_surface;
    }
    Q_INVOKABLE void open(const QString &panel);
    Q_INVOKABLE void close();
    void setReducedMotion(bool reduced);
signals:
    void changed();
    void closed();

private:
    SurfaceTransition m_surface{ 300, 200, false, false, this };
    QString m_panel;
    bool m_reduced = false;
};
}
