// Animates ruler ordinals and input feedback using scalar state. The shared frame clock
// preserves deterministic retargeting and reduced-motion endpoints.

#pragma once
#include "FrameClock.h"
#include <QPointer>
#include <QVariantList>
#include <QQmlParserStatus>
#include <array>

namespace yalahoo {
namespace ordinalMotion {
// The source trajectory has 48 intervals and at most ten decimal wheels.
inline constexpr auto TrackSampleCount = 49;
inline constexpr auto TransitionSeconds = .460;
inline constexpr auto MaximumDecimalDigits = 10;
}

// Frozen rolling-number's 49-point critically damped track, sampled by the shared clock.
struct OrdinalTrack final
{
    std::array<double, ordinalMotion::TrackSampleCount> points{ };
    double value = 0, velocity = 0, target = 0, elapsed = ordinalMotion::TransitionSeconds;
    void retarget(double to, bool instant);
    void advance(double dt);
};
class OrdinalMotion : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OrdinalMotion)
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(yalahoo::FrameClock *clock READ clock WRITE setClock NOTIFY clockChanged FINAL)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged FINAL)
    Q_PROPERTY(int digits READ digits WRITE setDigits NOTIFY digitsChanged FINAL)
    Q_PROPERTY(int direction READ direction WRITE setDirection NOTIFY directionChanged FINAL)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged FINAL)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(QVariantList faces READ faces NOTIFY changed FINAL)
public:
    explicit OrdinalMotion(QObject *parent = nullptr);
    void classBegin() override;
    void componentComplete() override;
    FrameClock *clock() const;
    void setClock(FrameClock *clock);
    int value() const;
    void setValue(int value);
    int digits() const;
    void setDigits(int digits);
    int direction() const;
    void setDirection(int direction);
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    bool active() const;
    void setActive(bool active);
    QVariantList faces() const;
    void advance(double dt);
Q_SIGNALS:
    void clockChanged();
    void valueChanged();
    void digitsChanged();
    void directionChanged();
    void reducedMotionChanged();
    void activeChanged();
    void changed();

private:
    void retarget(bool instant);
    void publish();
    QPointer<FrameClock> m_clock;
    QMetaObject::Connection m_connection;
    std::array<OrdinalTrack, ordinalMotion::MaximumDecimalDigits> m_tracks;
    QVariantList m_faces;
    int m_value = 1, m_previous = 1, m_digits = 2, m_direction = 0, m_count = 2;
    bool m_reduced = false, m_active = true, m_initialized = false;
};
}
