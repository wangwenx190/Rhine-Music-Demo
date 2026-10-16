#pragma once
#include "RhiItem.h"
#include "SceneAdapter.h"
#include "InputRouter.h"
#include "AtlasAllocator.h"
#include "TitleReel.h"
#include <QPointer>

namespace yalahoo::rhi {
class SceneItem : public RhiItem
{
    Q_OBJECT
    Q_PROPERTY(bool sceneOccluded READ sceneOccluded WRITE setSceneOccluded NOTIFY openingChanged)
    Q_PROPERTY(bool preparing READ preparing WRITE setPreparing NOTIFY openingChanged)
    Q_PROPERTY(QSize panelLayerSize READ panelLayerSize WRITE setPanelLayerSize NOTIFY openingChanged)
    Q_PROPERTY(QSize openingLayerSize READ openingLayerSize WRITE setOpeningLayerSize NOTIFY openingChanged)
    Q_PROPERTY(yalahoo::InputRouter *inputRouter READ inputRouter WRITE setInputRouter NOTIFY inputRouterChanged)
    Q_PROPERTY(yalahoo::SceneAdapter* adapter READ adapter WRITE setAdapter NOTIFY adapterChanged)
    Q_PROPERTY(bool inspectionEnabled READ inspectionEnabled WRITE setInspectionEnabled NOTIFY inspectionEnabledChanged)
    Q_PROPERTY(bool geometryOnly READ geometryOnly WRITE setGeometryOnly NOTIFY geometryOnlyChanged)
    Q_PROPERTY(bool telemetryEnabled READ telemetryEnabled WRITE setTelemetryEnabled NOTIFY telemetryEnabledChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(yalahoo::TitleReel *titleReel READ titleReel WRITE setTitleReel NOTIFY textReelsChanged)
    Q_PROPERTY(yalahoo::TitleReel *transportReel READ transportReel WRITE setTransportReel NOTIFY textReelsChanged)
public:
    explicit SceneItem(QQuickItem *parent = nullptr);
    bool sceneOccluded() const
    {
        return m_sceneOccluded;
    }
    void setSceneOccluded(bool value);
    bool preparing() const
    {
        return m_preparing;
    }
    void setPreparing(bool value);
    Q_INVOKABLE void restartPreparation();
    QSize panelLayerSize() const
    {
        return m_panelLayerSize;
    }
    void setPanelLayerSize(QSize value);
    QSize openingLayerSize() const
    {
        return m_openingLayerSize;
    }
    void setOpeningLayerSize(QSize value);
    SceneAdapter *adapter() const
    {
        return m_adapter;
    }
    void setAdapter(SceneAdapter *adapter);
    InputRouter *inputRouter() const;
    void setInputRouter(InputRouter *router);
    bool inspectionEnabled() const
    {
        return m_inspectionEnabled;
    }
    void setInspectionEnabled(bool enabled);
    bool geometryOnly() const
    {
        return m_geometryOnly;
    }
    void setGeometryOnly(bool enabled);
    bool telemetryEnabled() const
    {
        return m_telemetryEnabled;
    }
    void setTelemetryEnabled(bool enabled);
    QString errorText() const
    {
        return m_errorText;
    }
    Q_INVOKABLE QVector3D qtProjection(QVector3D world) const;
    QJsonObject coverStatistics() const;
    TitleReel *titleReel() const;
    void setTitleReel(TitleReel *reel);
    TitleReel *transportReel() const;
    void setTransportReel(TitleReel *reel);
signals:
    void openingChanged();
    void scenePrepared();
    void adapterChanged();
    void inputRouterChanged();
    void geometryOnlyChanged();
    void inspectionEnabledChanged();
    void telemetryEnabledChanged();
    void frameSubmitted(QJsonObject sample);
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
    void schedule();
    bool selectAt(QPointF point);
    void preparePool(Input &input);
    void resetPool();
    void refreshPool();
    void refreshTextResources();
    QPointer<TitleReel> m_titleReel, m_transportReel;
    QMetaObject::Connection m_titleConnection, m_transportConnection;
    QMetaObject::Connection m_titleDestroyedConnection, m_transportDestroyedConnection;
    QList<QSize> m_textSizes;
    quint64 m_textRevision = 0;
    struct OwnedCover
    {
        quint64 objectId = 0, lease = 0;
        std::shared_ptr<SelectedCoverLease> pixels;
        CoverSnapshot tiledPixels;
        QVector<AtlasUpload> uploads;
    };
    AtlasAllocator m_atlas{ 8192, 436, 22 };
    QHash<QString, AtlasLease> m_poolLeases;
    QHash<QString, QPair<QString, QString>> m_poolSources;
    QSet<QString> m_readyCovers;
    std::array<OwnedCover, 4> m_ownedCovers;
    quint64 m_ownedSerial = 0;
    quint64 m_activeObject = 0;
    QPointer<SelectionController> m_poolSelection;
    QMetaObject::Connection m_poolResetConnection, m_visibilityConnection, m_activeConnection;
    QMetaObject::Connection m_dprConnection, m_screenConnection;
    bool m_poolMode = false;
    bool m_sceneOccluded = false, m_preparing = false;
    quint64 m_preparationToken = 0;
    QSize m_openingLayerSize, m_panelLayerSize;
    quint64 m_coverRevision = 0, m_publishedCoverRevision = 0;
    QPointer<SceneAdapter> m_adapter;
    QPointer<InputRouter> m_inputRouter;
    QImage m_cover;
    CoverCache m_covers;
    SelectedCoverPool m_coverPool;
    std::shared_ptr<SelectedCoverLease> m_selectedCover;
    QString m_errorText;
    bool m_geometryOnly = false, m_inspectionEnabled = true;
    bool m_telemetryEnabled = false;
    std::optional<QPointF> m_mouseStart;
    bool m_mouseMoved = false, m_restorePending = false;
    std::optional<QPointF> m_touchStart;
    bool m_touchInspecting = false;
    quint64 m_touchTimestamp = 0;
};
}
