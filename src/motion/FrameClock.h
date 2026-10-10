#pragma once
#include <QObject>

namespace yalahoo {
class FrameClock : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool applicationVisible READ applicationVisible WRITE setApplicationVisible NOTIFY applicationVisibleChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(double visualTime READ visualTime NOTIFY advanced)
    Q_PROPERTY(double openingTime READ openingTime NOTIFY advanced)
    Q_PROPERTY(bool openingRunning READ openingRunning NOTIFY openingChanged)
    Q_PROPERTY(quint64 frameCount READ frameCount NOTIFY advanced)
public:
    explicit FrameClock(QObject *parent = nullptr) : QObject(parent) {}
    bool active() const { return m_active; }
    bool applicationVisible() const { return m_applicationVisible; }
    double speed() const { return m_speed; }
    double visualTime() const { return m_visualTime; }
    double openingTime() const { return m_openingTime; }
    bool openingRunning() const { return m_openingRunning; }
    quint64 frameCount() const { return m_frameCount; }
    void setActive(bool active);
    void setApplicationVisible(bool visible);
    void setSpeed(double speed);
    Q_INVOKABLE void advance(double frameSeconds);
    Q_INVOKABLE void startOpening();
    Q_INVOKABLE void finishOpening();
signals:
    void frame(double dt, double motionDt);
    void advanced();
    void activeChanged();
    void applicationVisibleChanged();
    void speedChanged();
    void openingChanged();
    void openingFinished();
private:
    bool m_active = false;
    bool m_applicationVisible = true;
    bool m_discardNext = true;
    bool m_openingRunning = false;
    double m_speed = 1;
    double m_visualTime = 0;
    double m_openingTime = 0;
    quint64 m_frameCount = 0;
};
}
