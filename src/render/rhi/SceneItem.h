// Connects production scene snapshots, cover leases and RHI rendering. GUI synchronization
// publishes coherent revisions without render-thread access to live models.

#pragma once
#include "RhiItem.h"
#include "SceneAdapter.h"
#include "InputRouter.h"
#include "AtlasAllocator.h"
#include "TitleReel.h"
#include <QPointer>

namespace yalahoo::rhi {
namespace sceneAtlas {
// The fixed archive plus owned detail tiles determine the initial atlas allocation.
inline constexpr auto MaximumAtlasSide = 8192;
inline constexpr auto CaseCapacity = 436;
inline constexpr auto BaseColumns = 24;
inline constexpr auto OwnedCoverCount = 4;
inline constexpr auto TilePixels = 256;
inline constexpr auto BaseWidthPixels = 6144;
inline constexpr auto InitialHeightPixels = 6656;
}

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
    bool renderingActive() const
    {
        return m_renderingActive;
    }
    void setRenderingActive(bool active);
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
    bool diagnosticsEnabled() const
    {
        return m_diagnosticsEnabled;
    }
    void setDiagnosticsEnabled(bool enabled);
    QVariantMap diagnosticValues() const
    {
        return m_diagnosticValues;
    }
    QString diagnosticText() const;
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
    void schedule();
    void configureDiagnostics();
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
    struct OwnedCover final
    {
        quint64 objectId = 0, lease = 0;
        std::shared_ptr<SelectedCoverLease> pixels;
        CoverSnapshot tiledPixels;
        QRect tiledRegion;
        QVector<AtlasUpload> uploads;
        bool highResolutionFailed = false;
    };
    AtlasAllocator m_atlas{ sceneAtlas::MaximumAtlasSide, sceneAtlas::CaseCapacity,
                            sceneAtlas::BaseColumns };
    QHash<QString, AtlasLease> m_poolLeases;
    QHash<QString, QPair<QString, QString>> m_poolSources;
    // Ready identities map to whether the tile contains real artwork rather than a placeholder.
    QHash<QString, bool> m_readyCovers;
    std::array<OwnedCover, sceneAtlas::OwnedCoverCount> m_ownedCovers;
    quint64 m_ownedSerial = 0;
    int m_detailResolution = sceneAtlas::TilePixels;
    int m_ownedSide = sceneAtlas::TilePixels;
    QSize m_atlasSize{ sceneAtlas::BaseWidthPixels, sceneAtlas::InitialHeightPixels };
    quint64 m_activeObject = 0;
    QPointer<SelectionController> m_poolSelection;
    QMetaObject::Connection m_poolResetConnection, m_visibilityConnection, m_activeConnection;
    QMetaObject::Connection m_dprConnection, m_screenConnection;
    bool m_poolMode = false, m_renderingActive = true;
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
    bool m_telemetryEnabled = false, m_diagnosticsEnabled = false;
    void configureTelemetry();
    quint64 m_diagnosticsEpoch = 0, m_telemetryEpoch = 0;
    quint64 m_callbackRevision = 0, m_publishedCallbackRevision = 0;
    QVariantMap m_diagnosticValues;
    QMetaObject::Connection m_presentedConnection, m_windowStateConnection;
    std::optional<QPointF> m_mouseStart;
    bool m_mouseMoved = false, m_restorePending = false;
    std::optional<QPointF> m_touchStart;
    bool m_touchInspecting = false;
    quint64 m_touchTimestamp = 0;
};
}
