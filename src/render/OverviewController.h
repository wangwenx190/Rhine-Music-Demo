#pragma once
#include "OverviewLabelsModel.h"
#include "ScalarSpring.h"
#include "SceneFrame.h"
#include <QPointer>

namespace yalahoo {
class MotionController;
class SceneAdapter;
class SelectionController;
class PresentationController;
class OverviewController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(yalahoo::OverviewLabelsModel *labels READ labels CONSTANT)
    Q_PROPERTY(yalahoo::SurfaceTransition *chrome READ chrome CONSTANT)
    Q_PROPERTY(double navigationOpacity READ navigationOpacity NOTIFY changed)
    Q_PROPERTY(double navigationOffset READ navigationOffset NOTIFY changed)
public:
    explicit OverviewController(MotionController *motion, QObject *parent = nullptr);
    void bind(SceneAdapter *scene, SelectionController *selection,
              PresentationController *presentation);
    bool active() const;
    bool ready() const;
    double progress() const;
    const Kinematics &state() const;
    OverviewLabelsModel *labels();
    SurfaceTransition *chrome();
    double navigationOpacity() const;
    double navigationOffset() const;
    Q_INVOKABLE void setActive(bool active);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void collapse();
    Q_INVOKABLE void expand(int lane);
    Q_INVOKABLE bool enter(int lane);
    void albumClicked();
    void advance(double dt, bool reduced);
    void synchronize(double dt, bool reduced);
    void reset();
    QVector<OverviewColumn> projectColumns() const;
    static double columnExtent(double lane, double centerLane, double progress);
    static CameraPose framing(CameraPose base, double width, double height, double progress);
signals:
    void changed();
    void focusRequested(bool overview);

private:
    void notify();
    std::array<double, 5> m_notified{ -1, -1, -1, -1, -1 };
    MotionController *m_motion;
    FrameClock *m_clock;
    QPointer<SceneAdapter> m_scene;
    QPointer<SelectionController> m_selection;
    QPointer<PresentationController> m_presentation;
    OverviewLabelsModel m_labels;
    SurfaceTransition m_chrome{ 460, 240, false, false };
    ScalarSpring m_progress;
    bool m_active = false, m_chromeShown = false, m_revealPending = false;
    quint64 m_sceneRevision = 0;
    bool m_labelsDirty = true;
    double m_navigationTime = .36, m_navFrom = 1, m_navOpacity = 1;
    double m_navOffsetFrom = 0, m_navOffset = 0;
};
}
