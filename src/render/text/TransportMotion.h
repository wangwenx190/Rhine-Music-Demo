// Animates transport-title state from the common frame clock. Reversible scalar tracks retain
// displayed derivatives and stop work while hidden.

#pragma once
#include "FrameClock.h"
#include <QPointer>
#include <QVariantMap>
#include <QQmlParserStatus>

namespace yalahoo {
struct TransportTrack final
{
    double start = 0, target = 0, elapsed = 1, duration = 1;
    double reversingStart = 0, shortening = 1;
    bool ease = false;
    double value() const;
    void move(double to, double seconds, bool instant);
    void advance(double dt);

private:
    double progress() const;
    mutable double m_cachedElapsed = -1, m_cachedDuration = -1, m_cachedProgress = 0;
    mutable bool m_cachedEase = false;
};
class TransportMotion : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransportMotion)
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(yalahoo::FrameClock *clock READ clock WRITE setClock NOTIFY inputChanged FINAL)
    Q_PROPERTY(double targetWidth READ targetWidth WRITE setTargetWidth NOTIFY inputChanged FINAL)
    Q_PROPERTY(bool shown READ shown WRITE setShown NOTIFY inputChanged FINAL)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY inputChanged FINAL)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY inputChanged FINAL)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY inputChanged FINAL)
    Q_PROPERTY(double width READ width NOTIFY changed FINAL)
    Q_PROPERTY(double opacity READ opacity NOTIFY changed FINAL)
    Q_PROPERTY(double offset READ offset NOTIFY changed FINAL)
    Q_PROPERTY(double pauseOpacity READ pauseOpacity NOTIFY changed FINAL)
    Q_PROPERTY(bool moving READ moving NOTIFY changed FINAL)
public:
    explicit TransportMotion(QObject *parent = nullptr);
    void classBegin() override;
    void componentComplete() override;
    FrameClock *clock() const;
    void setClock(FrameClock *clock);
    double targetWidth() const;
    void setTargetWidth(double width);
    bool shown() const;
    void setShown(bool shown);
    bool playing() const;
    void setPlaying(bool playing);
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    bool active() const;
    void setActive(bool active);
    double width() const;
    double opacity() const;
    double offset() const;
    double pauseOpacity() const;
    bool moving() const;
    void advance(double dt);
    Q_INVOKABLE void finish();
Q_SIGNALS:
    void inputChanged();
    void changed();

private:
    void retarget();
    void move(bool instant);
    QPointer<FrameClock> m_clock;
    QMetaObject::Connection m_connection;
    TransportTrack m_width, m_alpha, m_offset, m_icon;
    double m_targetWidth = 0;
    bool m_shown = false, m_playing = false, m_reduced = false, m_active = true, m_complete = false;
};
}
