#pragma once
#include <QObject>
#include <QString>

namespace yalahoo {
class LibrarySession;
class PresentationController;
class FrameClock;
// One replaceable reveal; all navigation stays in PresentationController.
class TrackFocusController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackFocusController)
    Q_PROPERTY(quint64 revision READ revision NOTIFY changed FINAL)
    Q_PROPERTY(int row READ row NOTIFY changed FINAL)
    Q_PROPERTY(QString trackId READ trackId NOTIFY changed FINAL)
    Q_PROPERTY(double scrollY READ scrollY NOTIFY scrollChanged FINAL)
    Q_PROPERTY(double highlight READ highlight NOTIFY changed FINAL)
    Q_PROPERTY(bool revealing READ revealing NOTIFY changed FINAL)
    Q_PROPERTY(bool active READ active NOTIFY changed FINAL)
public:
    TrackFocusController(LibrarySession *library, PresentationController *presentation,
                         FrameClock *clock, QObject *parent = nullptr);
    Q_INVOKABLE bool request(const QString &albumId, const QString &trackId = { });
    Q_INVOKABLE bool locatePlayback();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void beginReveal(quint64 revision, double start, double target);
    Q_INVOKABLE void beginListReveal(quint64 revision, double startRelative, double origin,
                                     double low, double high, double rowTop, double rowHeight,
                                     double viewportHeight, double headerHeight);
    Q_INVOKABLE void refineListReveal(double origin, double rowTop, double rowHeight,
                                      double viewportHeight, double headerHeight, double high);
    Q_INVOKABLE double listScrollY(double origin) const;
    void setReducedMotion(bool reduced);
    void advance(double dt);
    quint64 revision() const
    {
        return m_revision;
    }
    int row() const
    {
        return m_row;
    }
    QString trackId() const
    {
        return m_trackId;
    }
    double scrollY() const
    {
        return m_scrollY;
    }
    double highlight() const
    {
        return m_highlight;
    }
    bool revealing() const
    {
        return m_scrolling || m_pulsing;
    }
    bool active() const
    {
        return !m_albumId.isEmpty();
    }
Q_SIGNALS:
    void navigationRequested();
    void changed();
    void scrollChanged();
    void revealRequested(quint64 revision, int row);
    void focusRequested(int row);
    void rejected(const QString &message);

private:
    void ready();
    void startHighlight();
    LibrarySession *m_library;
    PresentationController *m_presentation;
    QString m_albumId, m_trackId;
    quint64 m_revision = 0;
    int m_row = -1;
    bool m_reduced = false, m_requested = false, m_scrolling = false, m_pulsing = false;
    double m_scrollY = 0, m_start = 0, m_target = 0, m_duration = 0, m_elapsed = 0;
    double m_highlight = 0;
    double m_listOrigin = 0, m_listLow = 0, m_listHigh = 0;
    double m_listTarget = 0, m_listOffset = 0;
};
}
