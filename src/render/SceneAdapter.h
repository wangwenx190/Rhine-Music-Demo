// Builds backend-neutral camera, geometry and material frame snapshots. Source-space math is
// isolated from Qt projection and native backend conventions.

#pragma once
#include "SceneFrame.h"
#include "RowLightController.h"
#include <QObject>
#include <QVariantMap>
#include <memory>
#include <optional>
namespace yalahoo {
class FrameClock;
class LayoutMetrics;
class SelectionLighting;
class RenderQuality;
class SelectionController;
}
Q_MOC_INCLUDE("LayoutMetrics.h")
Q_MOC_INCLUDE("SelectionLighting.h")
Q_MOC_INCLUDE("RenderQuality.h")
Q_MOC_INCLUDE("SelectionController.h")

namespace yalahoo {
class SceneAdapter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SceneAdapter)
    Q_PROPERTY(yalahoo::SelectionLighting* lighting READ lighting CONSTANT FINAL)
    Q_PROPERTY(yalahoo::RenderQuality* quality READ quality CONSTANT FINAL)
    Q_PROPERTY(QVariantMap renderBackendContract READ renderBackendContract CONSTANT FINAL)
    Q_PROPERTY(yalahoo::LayoutMetrics* layout READ layout CONSTANT FINAL)
    Q_PROPERTY(QVector3D cameraPosition READ cameraPosition NOTIFY frameChanged FINAL)
    Q_PROPERTY(QVector3D cameraAim READ cameraAim NOTIFY frameChanged FINAL)
    Q_PROPERTY(QQuaternion cameraRotation READ cameraRotation NOTIFY frameChanged FINAL)
    Q_PROPERTY(QVector3D casePosition READ casePosition NOTIFY frameChanged FINAL)
    Q_PROPERTY(double fieldOfView READ fieldOfView NOTIFY frameChanged FINAL)
    Q_PROPERTY(double clipNear READ clipNear CONSTANT FINAL)
    Q_PROPERTY(double clipFar READ clipFar CONSTANT FINAL)
    Q_PROPERTY(quint64 frameRevision READ frameRevision NOTIFY frameChanged FINAL)
    Q_PROPERTY(double detail READ detail WRITE setDetail NOTIFY detailChanged FINAL)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged FINAL)
    Q_PROPERTY(yalahoo::SelectionController* selection READ selection NOTIFY selectionControllerChanged FINAL)
public:
    explicit SceneAdapter(FrameClock *clock = nullptr, QObject *parent = nullptr);
    ~SceneAdapter() override;
    void setSelection(SelectionController *selection);
    SelectionController *selection() const;
    SelectionLighting *lighting();
    RenderQuality *quality();
    QVariantMap renderBackendContract() const;
    LayoutMetrics *layout();
    const SceneFrame &frame() const;
    QVector3D cameraPosition() const;
    QVector3D cameraAim() const;
    QQuaternion cameraRotation() const;
    QVector3D casePosition() const;
    double fieldOfView() const;
    double clipNear() const;
    double clipFar() const;
    quint64 frameRevision() const;
    double detail() const;
    bool reducedMotion() const;
    void setDetail(double detail);
    void requestDetail(bool detail);
    void setReducedMotion(bool reduced);
    // Called with the already speed-scaled motionDt from the single FrameClock.
    void advance(double motionDt);
    void observe(CameraPose pose, double motionDt);
    void setOpeningTime(std::optional<double> time, double dt = 0);
    bool settled() const;
    CameraPose target() const;
    void setNavigation(double laneSpeed, double rowSpeed, bool realColumns = false);
    void setPointer(double x, double y);
    void snapToTarget();
    void setLightingInput(RowLightingInput input, QQuaternion caseRotation = { });
    void updateLighting(double motionDt);
    std::optional<QVector3D> projectWorldPoint(QVector3D world) const;
    Q_INVOKABLE double focalDepth(QVector3D origin) const;
    Q_INVOKABLE QVariantMap hitTest(double x, double y) const;
    Q_INVOKABLE void recordInput();
    void recordNavigationCommit(quint64 revision);
Q_SIGNALS:
    void frameChanged();
    void detailChanged();
    void reducedMotionChanged();
    void selectionControllerChanged();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
