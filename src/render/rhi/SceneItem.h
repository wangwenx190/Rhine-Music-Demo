// Connects production scene snapshots, cover leases and RHI rendering. GUI synchronization
// publishes coherent revisions without render-thread access to live models.

#pragma once
#include "RhiItem.h"
#include <memory>

namespace yalahoo {
class SceneAdapter;
class InputRouter;
class TitleReel;
}
Q_MOC_INCLUDE("SceneAdapter.h")
Q_MOC_INCLUDE("InputRouter.h")
Q_MOC_INCLUDE("TitleReel.h")

namespace yalahoo::rhi {
class SceneItem : public RhiItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SceneItem)
    Q_PROPERTY(bool renderingActive READ renderingActive WRITE setRenderingActive NOTIFY renderingActiveChanged FINAL)
    Q_PROPERTY(bool sceneOccluded READ sceneOccluded WRITE setSceneOccluded NOTIFY openingChanged FINAL)
    Q_PROPERTY(bool preparing READ preparing WRITE setPreparing NOTIFY openingChanged FINAL)
    Q_PROPERTY(QSize panelLayerSize READ panelLayerSize WRITE setPanelLayerSize NOTIFY openingChanged FINAL)
    Q_PROPERTY(QSize openingLayerSize READ openingLayerSize WRITE setOpeningLayerSize NOTIFY openingChanged FINAL)
    Q_PROPERTY(yalahoo::InputRouter *inputRouter READ inputRouter WRITE setInputRouter NOTIFY inputRouterChanged FINAL)
    Q_PROPERTY(yalahoo::SceneAdapter* adapter READ adapter WRITE setAdapter NOTIFY adapterChanged FINAL)
    Q_PROPERTY(bool inspectionEnabled READ inspectionEnabled WRITE setInspectionEnabled NOTIFY inspectionEnabledChanged FINAL)
    Q_PROPERTY(bool geometryOnly READ geometryOnly WRITE setGeometryOnly NOTIFY geometryOnlyChanged FINAL)
    Q_PROPERTY(bool telemetryEnabled READ telemetryEnabled WRITE setTelemetryEnabled NOTIFY telemetryEnabledChanged FINAL)
    Q_PROPERTY(bool diagnosticsEnabled READ diagnosticsEnabled WRITE setDiagnosticsEnabled NOTIFY diagnosticsChanged FINAL)
    Q_PROPERTY(QString diagnosticText READ diagnosticText NOTIFY diagnosticValuesChanged FINAL)
    Q_PROPERTY(QVariantMap diagnosticValues READ diagnosticValues NOTIFY diagnosticValuesChanged FINAL)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged FINAL)
    Q_PROPERTY(yalahoo::TitleReel *titleReel READ titleReel WRITE setTitleReel NOTIFY textReelsChanged FINAL)
    Q_PROPERTY(yalahoo::TitleReel *transportReel READ transportReel WRITE setTransportReel NOTIFY textReelsChanged FINAL)
public:
    explicit SceneItem(QQuickItem *parent = nullptr);
    ~SceneItem() override;
    bool renderingActive() const;
    void setRenderingActive(bool active);
    bool sceneOccluded() const;
    void setSceneOccluded(bool value);
    bool preparing() const;
    void setPreparing(bool value);
    Q_INVOKABLE void restartPreparation();
    QSize panelLayerSize() const;
    void setPanelLayerSize(QSize value);
    QSize openingLayerSize() const;
    void setOpeningLayerSize(QSize value);
    SceneAdapter *adapter() const;
    void setAdapter(SceneAdapter *adapter);
    InputRouter *inputRouter() const;
    void setInputRouter(InputRouter *router);
    bool inspectionEnabled() const;
    void setInspectionEnabled(bool enabled);
    bool geometryOnly() const;
    void setGeometryOnly(bool enabled);
    bool telemetryEnabled() const;
    void setTelemetryEnabled(bool enabled);
    bool diagnosticsEnabled() const;
    void setDiagnosticsEnabled(bool enabled);
    QVariantMap diagnosticValues() const;
    QString diagnosticText() const;
    QString errorText() const;
    QJsonObject coverStatistics() const;
    TitleReel *titleReel() const;
    void setTitleReel(TitleReel *reel);
    TitleReel *transportReel() const;
    void setTransportReel(TitleReel *reel);
Q_SIGNALS:
    void renderingActiveChanged();
    void openingChanged();
    void scenePrepared();
    void adapterChanged();
    void inputRouterChanged();
    void geometryOnlyChanged();
    void inspectionEnabledChanged();
    void telemetryEnabledChanged();
    void frameSubmitted(QJsonObject sample);
    void diagnosticsChanged();
    void diagnosticValuesChanged();
    void errorTextChanged();
    void textReelsChanged();

protected:
    void updatePolish() override;
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void touchEvent(QTouchEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
