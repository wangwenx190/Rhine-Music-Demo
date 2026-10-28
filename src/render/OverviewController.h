// Coordinates overview camera framing, navigation chrome and column entry. Label projection
// follows scene revisions and navigation remains independent of playback.

#pragma once
#include "SurfaceTransition.h"
#include <memory>
#include "OverviewLabelsModel.h"
#include "ScalarSpring.h"
#include "SceneFrame.h"
#include <QPointer>

namespace yalahoo {
namespace overviewMotion {
// Chrome and camera tracks share a frame source while preserving independent source timing.
inline constexpr auto NotificationFields = 5;
inline constexpr auto ChromeEnterMilliseconds = 460;
inline constexpr auto ChromeExitMilliseconds = 240;
inline constexpr auto NavigationSlideSeconds = .36;
}

class MotionController;
class SceneAdapter;
class SelectionController;
class PresentationController;
class OverviewController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OverviewController)
    Q_PROPERTY(bool active READ active NOTIFY changed FINAL)
    Q_PROPERTY(bool ready READ ready NOTIFY changed FINAL)
    Q_PROPERTY(double progress READ progress NOTIFY changed FINAL)
    Q_PROPERTY(yalahoo::OverviewLabelsModel *labels READ labels CONSTANT FINAL)
    Q_PROPERTY(yalahoo::SurfaceTransition *chrome READ chrome CONSTANT FINAL)
    Q_PROPERTY(double navigationOpacity READ navigationOpacity NOTIFY changed FINAL)
    Q_PROPERTY(double navigationOffset READ navigationOffset NOTIFY changed FINAL)
public:
    explicit OverviewController(MotionController *motion, QObject *parent = nullptr);
    ~OverviewController() override;
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
Q_SIGNALS:
    void changed();
    void focusRequested(bool overview);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
