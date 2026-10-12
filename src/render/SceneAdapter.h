#pragma once
#include "FrameClock.h"
#include "LayoutMetrics.h"
#include "SceneFrame.h"
#include "ScalarSpring.h"
#include "SelectionLighting.h"
#include "RenderQuality.h"
#include "RenderBackend.h"
#include <QVariantMap>
namespace yalahoo {
class SceneAdapter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(yalahoo::SelectionLighting* lighting READ lighting CONSTANT)
    Q_PROPERTY(yalahoo::RenderQuality* quality READ quality CONSTANT)
    Q_PROPERTY(QVariantMap renderBackendContract READ renderBackendContract CONSTANT)
    Q_PROPERTY(yalahoo::LayoutMetrics* layout READ layout CONSTANT)
    Q_PROPERTY(QVector3D cameraPosition READ cameraPosition NOTIFY frameChanged)
    Q_PROPERTY(QVector3D cameraAim READ cameraAim NOTIFY frameChanged)
    Q_PROPERTY(QQuaternion cameraRotation READ cameraRotation NOTIFY frameChanged)
    Q_PROPERTY(QVector3D casePosition READ casePosition NOTIFY frameChanged)
    Q_PROPERTY(double fieldOfView READ fieldOfView NOTIFY frameChanged)
    Q_PROPERTY(double clipNear READ clipNear CONSTANT)
    Q_PROPERTY(double clipFar READ clipFar CONSTANT)
    Q_PROPERTY(quint64 frameRevision READ frameRevision NOTIFY frameChanged)
    Q_PROPERTY(double detail READ detail WRITE setDetail NOTIFY detailChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
public:
    explicit SceneAdapter(FrameClock *clock = nullptr, QObject *parent = nullptr);
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
    void setReducedMotion(bool reduced);
    // Called with the already speed-scaled motionDt from the single FrameClock.
    void advance(double motionDt);
    void observe(CameraPose pose, double motionDt);
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
signals:
    void frameChanged();
    void detailChanged();
    void reducedMotionChanged();

private:
    void publish();
    LayoutMetrics m_layout;
    SelectionLighting m_lighting;
    RenderQuality m_quality;
    RowLightingInput m_lightingInput;
    QQuaternion m_caseRotation;
    bool m_explicitLightingInput = false;
    SceneFrame m_frame;
    std::array<ScalarSpring, 3> m_position, m_aim;
    ScalarSpring m_span, m_yaw, m_elevation;
    double m_detail = 0, m_laneSpeed = 0, m_rowSpeed = 0, m_pointerX = 0, m_pointerY = 0;
    bool m_reduced = false;
};
}
