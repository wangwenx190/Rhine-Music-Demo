// Private GUI-thread scene state; observation is implemented by separate consumers.
#pragma once
#include "SceneItem.h"
#include "SceneCovers.h"
#include "SceneAdapter.h"
#include "InputRouter.h"
#include "TitleReel.h"
#include <QMutex>
#include <QPointer>
namespace yalahoo::rhi {
struct SceneItem::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(SceneItem *owner) : q(owner)
    {
    }
    SceneItem *const q;

    void submitOccluded();
    double projectedCoverDemand(const QMatrix4x4 &vp, const QMatrix4x4 &model) const;
    void handleVisibility(bool visible);
    static void queuePrepared(const QPointer<SceneItem> &guard, quint64 token, quint64 submission,
                              bool uploaded);
    static void publishPrepared(const QPointer<SceneItem> &guard, quint64 token, quint64 submission,
                                bool uploaded);
    struct DiagnosticMailbox final
    {
        Q_DISABLE_COPY_MOVE(DiagnosticMailbox)
        DiagnosticMailbox() = default;
        QMutex mutex;
        DiagnosticSamples samples;
        DiagnosticSeries presented;
        std::optional<double> previousPresentation;
        std::optional<DiagnosticBatch> latest;
        DiagnosticSeries latestPresented;
        bool queued = false;
    };
    struct TelemetryMailbox;
    static void publishTelemetry(const std::shared_ptr<TelemetryMailbox> &mailbox,
                                 const QPointer<SceneItem> &guard, quint64 epoch);
    static void publishDiagnostics(const std::shared_ptr<DiagnosticMailbox> &mailbox,
                                   const QPointer<SceneItem> &guard, quint64 epoch);
    void bindWindow(QQuickWindow *window);
    static void queueDiagnostics(const std::shared_ptr<DiagnosticMailbox> &mailbox,
                                 const QPointer<SceneItem> &guard, const quint64 &epoch,
                                 DiagnosticSample sample);
    static void queueTelemetry(const std::shared_ptr<TelemetryMailbox> &mailbox,
                               const QPointer<SceneItem> &guard, const quint64 &epoch,
                               QJsonObject sample);

    void schedule();
    void configureDiagnostics();
    bool selectAt(QPointF point);
    void refreshTextResources();
    void bindTextReel(QPointer<TitleReel> &current, QMetaObject::Connection &changed,
                      QMetaObject::Connection &destroyed, TitleReel *reel);
    QPointer<TitleReel> m_titleReel, m_transportReel;
    QMetaObject::Connection m_titleConnection, m_transportConnection;
    QMetaObject::Connection m_titleDestroyedConnection, m_transportDestroyedConnection;
    QList<QSize> m_textSizes;
    quint64 m_textRevision = 0;
    SceneCovers m_sceneCovers;
    QMetaObject::Connection m_visibilityConnection, m_activeConnection;
    QMetaObject::Connection m_dprConnection, m_screenConnection;
    bool m_renderingActive = true;
    bool m_sceneOccluded = false, m_preparing = false;
    quint64 m_preparationToken = 0;
    QSize m_openingLayerSize, m_panelLayerSize;
    quint64 m_publishedCoverRevision = 0;
    QPointer<SceneAdapter> m_adapter;
    QPointer<InputRouter> m_inputRouter;
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
