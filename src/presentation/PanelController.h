// Coordinates modal panel intent and the shared surface transition. Focus and visibility are
// released only when the exit transition has completed.

#pragma once
#include "SurfaceTransition.h"
namespace yalahoo {
namespace panelMotion {
// Modal panel transition timing from the source presentation.
inline constexpr auto EnterMilliseconds = 300;
inline constexpr auto ExitMilliseconds = 200;
}

class PanelController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PanelController)
    Q_PROPERTY(QString panel READ panel NOTIFY changed FINAL)
    Q_PROPERTY(yalahoo::SurfaceTransition *surface READ surface CONSTANT FINAL)
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
    Q_INVOKABLE void selectResult(const QString &albumId, const QString &trackId);
    Q_INVOKABLE void reset();
    void setReducedMotion(bool reduced);
Q_SIGNALS:
    void changed();
    void closed();
    void resultRequested(const QString &albumId, const QString &trackId);

private:
    void beginClose();
    SurfaceTransition m_surface{ panelMotion::EnterMilliseconds, panelMotion::ExitMilliseconds,
                                 false, false, this };
    QString m_panel;
    QString m_pendingAlbum, m_pendingTrack;
    quint64 m_revision = 0;
    bool m_reduced = false;
};
}
