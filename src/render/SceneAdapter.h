#pragma once
#include "FrameClock.h"
#include "LayoutMetrics.h"
#include "SceneFrame.h"
#include "ScalarSpring.h"
#include "SelectionLighting.h"
#include "RenderQuality.h"
#include "RenderBackend.h"
#include "SelectionController.h"
#include <QVariantMap>
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
    void setSelection(SelectionController *selection);
    SelectionController *selection() const;
    SelectionLighting *lighting()
    {
        return &m_lighting;
    }
    RenderQuality *quality()
    {
        return &m_quality;
    }
    QVariantMap renderBackendContract() const
    {
        return RenderBackendContract::fields();
    }
    LayoutMetrics *layout()
    {
        return &m_layout;
    }
    const SceneFrame &frame() const
    {
        return m_frame;
    }
    QVector3D cameraPosition() const
    {
        return m_frame.camera.position.qt();
    }
    QVector3D cameraAim() const
    {
        return m_frame.camera.aim.qt();
    }
    QQuaternion cameraRotation() const
    {
        return m_frame.cameraRotation;
    }
    QVector3D casePosition() const
    {
        return m_frame.casePosition.qt();
    }
    double fieldOfView() const
    {
        return m_frame.fieldOfView;
    }
    double clipNear() const
    {
        return m_frame.nearPlane;
    }
    double clipFar() const
    {
        return m_frame.farPlane;
    }
    quint64 frameRevision() const
    {
        return m_frame.revision;
    }
    double detail() const
    {
        return m_detail;
    }
    bool reducedMotion() const
    {
        return m_reduced;
    }
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
    Q_INVOKABLE QVector3D projectWorldPoint(QVector3D world) const;
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
    void publish();
    QPointer<SelectionController> m_selection;
    quint64 m_selectionRevision = 0;
    LayoutMetrics m_layout;
    SelectionLighting m_lighting;
    RenderQuality m_quality;
    RowLightingInput m_lightingInput;
    QQuaternion m_caseRotation;
    bool m_explicitLightingInput = false;
    SceneFrame m_frame;
    QMatrix4x4 m_viewProjection;
    std::array<ScalarSpring, 3> m_position, m_aim;
    ScalarSpring m_span, m_yaw, m_elevation;
    double m_detail = 0, m_laneSpeed = 0, m_rowSpeed = 0, m_pointerX = 0, m_pointerY = 0;
    bool m_reduced = false, m_managedDetail = false;
    std::optional<double> m_openingTime;
};
}
