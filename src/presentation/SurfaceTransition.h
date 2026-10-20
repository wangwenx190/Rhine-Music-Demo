#pragma once
#include <QObject>
#include <QEasingCurve>
#include <QObject>
#include <functional>
#include <optional>

namespace yalahoo {
class FrameClock;

// Frozen ui-transitions.ts: one replaceable animation, driven by the shared clock.
class SurfaceTransition final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SurfaceTransition)
    Q_PROPERTY(double opacity READ opacity NOTIFY changed FINAL)
    Q_PROPERTY(double offset READ offset NOTIFY changed FINAL)
    Q_PROPERTY(bool shown READ shown NOTIFY changed FINAL)
    Q_PROPERTY(bool running READ running NOTIFY changed FINAL)
public:
    SurfaceTransition(double enterMs, double exitMs, bool right, bool browse,
                      QObject *parent = nullptr);
    double opacity() const
    {
        return m_opacity;
    }
    double offset() const
    {
        return m_offset;
    }
    bool shown() const
    {
        return m_shown;
    }
    bool running() const
    {
        return m_running;
    }
    void setEnterCurve(const QEasingCurve &curve)
    {
        m_enterCurve = curve;
    }
    void show(bool reduced);
    void hide(bool reduced, std::function<void()> finished = { });
    void advance(double seconds);
    void finish();
    void reset(bool shown);
Q_SIGNALS:
    void changed();

private:
    void run(bool show, bool reduced, std::function<void()> finished);
    double m_enterMs, m_exitMs;
    double m_opacity = 0, m_offset = 0;
    double m_fromOpacity = 0, m_fromOffset = 0, m_toOffset = 0;
    double m_elapsed = 0, m_duration = 0;
    bool m_right, m_browse, m_shown = false, m_running = false, m_show = false;
    QEasingCurve m_ease;
    std::optional<QEasingCurve> m_enterCurve;
    quint64 m_revision = 0;
    std::function<void()> m_finished;
};
}
