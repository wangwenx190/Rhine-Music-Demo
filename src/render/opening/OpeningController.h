#pragma once
#include "OpeningFrame.h"
#include "SceneAdapter.h"
#include "MotionController.h"

namespace yalahoo {
class OpeningController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool preparing READ preparing NOTIFY changed)
    Q_PROPERTY(bool sceneOccluded READ sceneOccluded NOTIFY changed)
    Q_PROPERTY(bool film READ film NOTIFY changed)
    Q_PROPERTY(double time READ time NOTIFY changed)
    Q_PROPERTY(double revealTime READ revealTime NOTIFY changed)
    Q_PROPERTY(QVariantMap drawing READ drawing NOTIFY changed)
public:
    OpeningController(MotionController *motion, SceneAdapter *scene, SelectionController *selection,
                      QObject *parent = nullptr);
    bool active() const
    {
        return m_phase != Phase::Idle && m_phase != Phase::Done;
    }
    bool preparing() const
    {
        return m_phase == Phase::Preparing;
    }
    bool film() const
    {
        return m_phase == Phase::Film;
    }
    bool sceneOccluded() const
    {
        return film() && time() < 6.32;
    }
    double time() const
    {
        return m_time;
    }
    double revealTime() const
    {
        return m_phase == Phase::Reveal                            ? m_reveal
                : m_phase == Phase::Done || m_phase == Phase::Idle ? .72
                                                                   : 0;
    }
    QVariantMap drawing() const
    {
        return openingFrame(m_time).values();
    }
    Q_INVOKABLE void replay();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void scenePrepared();
    Q_INVOKABLE double chrome(double elapsed, double delay = 0) const
    {
        return openingReveal(elapsed, delay);
    }
    quint64 revision() const
    {
        return m_revision;
    }
signals:
    void changed();
    void started();
    void handedOff();
    void completed();

private:
    enum class Phase {
        Idle,
        Preparing,
        Film,
        Reveal,
        Done
    };
    void advance(double dt);
    void sample(double dt);
    void finishFilm(bool skipped);
    void complete();
    MotionController *m_motion;
    SceneAdapter *m_scene;
    SelectionController *m_selection;
    Phase m_phase = Phase::Idle;
    double m_time = 0, m_reveal = 0;
    bool m_skipPending = false;
    quint64 m_revision = 0;
};
}
