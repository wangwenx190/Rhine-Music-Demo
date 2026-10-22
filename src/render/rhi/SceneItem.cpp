#include "Units.h"
#include "RenderData.h"
#include "SelectionLighting.h"
#include "RenderQuality.h"
#include "SceneAdapter.h"
#include "SelectionController.h"
#include <QCoreApplication>
#include "SceneItem.h"
#include <QLineF>
#include <QQuickRenderTarget>
#include "MonotonicTime.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QMutex>
#include <QMutexLocker>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTouchEvent>
#include <QHoverEvent>
#include <memory>
#include <utility>
#include <limits>
#include <cmath>

namespace {
// Projected paper demand uses physical DPR; owned covers reserve two rows beyond the fixed base atlas.
constexpr auto MaximumFeedbackSamples = 128;
constexpr auto ClickSlopPixels = 6;
constexpr auto PaperTileExtent = 64;
constexpr auto PaperInteriorExtent = 62;
constexpr auto CaptureSamples = 4;
constexpr auto PaperCornerCount = 4;
constexpr auto BrowseAperture = .0003;
constexpr auto DetailApertureGain = .0005;
constexpr auto PercentScale = 100;
}

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
struct SceneItem::DiagnosticMailbox final
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
struct SceneItem::TelemetryMailbox final
{
    Q_DISABLE_COPY_MOVE(TelemetryMailbox)
    TelemetryMailbox() = default;
    QMutex mutex;
    QVector<QJsonObject> samples;
    bool queued = false;
    quint64 coalesced = 0;
};
QString SceneItem::diagnosticText() const
{
    if (!m_diagnosticsEnabled)
        return QCoreApplication::translate("Yalahoo", "Diagnostics disabled");
    const auto &sample = m_diagnosticValues;
    if (!sample.value(u"submissions"_s).toULongLong())
        return QCoreApplication::translate("Yalahoo", "Waiting for valid rendering samples...");
    const auto percentile = [&sample](const QString &series, const QString &key) {
        const auto values = sample.value(series).toMap();
        return values.value(u"count"_s).toUInt()
                ? QString::number(values.value(key).toDouble(), 'f', 2) + " ms"_L1
                : QCoreApplication::translate("Yalahoo", "Unavailable");
    };
    return QStringList{
        QCoreApplication::translate("Yalahoo", "RHI submission interval p50 / p95 / p99"),
        percentile(u"submissionIntervals"_s, u"p50"_s) + " / "_L1
                + percentile(u"submissionIntervals"_s, u"p95"_s) + " / "_L1
                + percentile(u"submissionIntervals"_s, u"p99"_s),
        QCoreApplication::translate("Yalahoo", "CPU submission p95 ")
                + percentile(u"cpu"_s, u"p95"_s)
                + QCoreApplication::translate("Yalahoo", " \u00b7 Sync p95 ")
                + percentile(u"sync"_s, u"p95"_s),
        QCoreApplication::translate("Yalahoo", "Completed GPU frame p95 ")
                + percentile(u"gpu"_s, u"p95"_s),
        QCoreApplication::translate("Yalahoo", "Qt presentation event p95 ")
                + percentile(u"qtFrameIntervals"_s, u"p95"_s),
        QCoreApplication::translate("Yalahoo", "Batch uploads: atlas / selected / uniform"),
        sample.value(u"atlasUploadBytes"_s).toString() + " / "_L1
                + sample.value(u"selectedUploadBytes"_s).toString() + " / "_L1
                + sample.value(u"uniformUploadBytes"_s).toString() + " bytes"_L1,
        QCoreApplication::translate("Yalahoo", "Total decodes started ")
                + sample.value(u"decodeStarts"_s).toString()
                + QCoreApplication::translate("Yalahoo", " \u00b7 Active ")
                + sample.value(u"decodeActive"_s).toString()
                + QCoreApplication::translate("Yalahoo", " \u00b7 Waiting ")
                + sample.value(u"decodePending"_s).toString(),
        QCoreApplication::translate("Yalahoo", "Submission throughput at current scheduling ")
                + QString::number(sample.value(u"submissionRate"_s).toDouble(), 'f', 1)
                + QCoreApplication::translate("Yalahoo", " / second"),
        QCoreApplication::translate("Yalahoo", "Submission samples ")
                + sample.value(u"submissions"_s).toString() + " / "_L1
                + QString::number(sample.value(u"elapsedMs"_s).toDouble(), 'f', 0) + " ms"_L1,
        QCoreApplication::translate("Yalahoo", "Requested VSYNC interval ")
                + sample.value(u"requestedSwapInterval"_s).toString()
                + QCoreApplication::translate(
                        "Yalahoo", " \u00b7 Actual VSYNC and screen presentation time unavailable"),
        QCoreApplication::translate(
                "Yalahoo",
                "A fixed window retains the latest 256 samples. RHI intervals reflect current scheduling, not a complete product throughput benchmark. CPU excludes GPU completion; GPU scope is provided by the QRhi backend.")
    }
            .join('\n'_L1);
}
TitleReel *SceneItem::titleReel() const
{
    return m_titleReel;
}
void SceneItem::setTitleReel(TitleReel *reel)
{
    bindTextReel(m_titleReel, m_titleConnection, m_titleDestroyedConnection, reel);
}
TitleReel *SceneItem::transportReel() const
{
    return m_transportReel;
}
void SceneItem::setTransportReel(TitleReel *reel)
{
    bindTextReel(m_transportReel, m_transportConnection, m_transportDestroyedConnection, reel);
}
void SceneItem::bindTextReel(QPointer<TitleReel> &current, QMetaObject::Connection &changed,
                             QMetaObject::Connection &destroyed, TitleReel *reel)
{
    if (current == reel)
        return;
    disconnect(changed);
    disconnect(destroyed);
    current = reel;
    if (reel) {
        changed =
                connect(reel, &TitleReel::resourcesChanged, this, &SceneItem::refreshTextResources);
        destroyed = connect(reel, &QObject::destroyed, this, &SceneItem::refreshTextResources);
    }
    refreshTextResources();
    Q_EMIT textReelsChanged();
}
void SceneItem::refreshTextResources()
{
    const auto sizes = (m_titleReel ? m_titleReel->resourceSizes() : QList<QSize>())
            + (m_transportReel ? m_transportReel->resourceSizes() : QList<QSize>());
    if (sizes == m_textSizes)
        return;
    m_textSizes = sizes;
    ++m_textRevision;
    schedule();
}
void SceneItem::handleVisibility(bool visible)
{
    configureDiagnostics();
    if (!visible)
        mouseUngrabEvent();
    schedule();
    if (visible && !m_restorePending) {
        m_restorePending = true;
        // A native surface can be replaced while the basic loop retains
        // its scene graph. Submit the current immutable frame on restore,
        // even when reduced motion leaves all scene values unchanged.
        QMetaObject::invokeMethod(
                this,
                [this] {
                    m_restorePending = false;
                    if (this->window() && this->window()->isVisible()) {
                        ensurePolished();
                        update();
                    }
                },
                Qt::QueuedConnection);
    }
}
void SceneItem::queuePrepared(const QPointer<SceneItem> &guard, quint64 token, quint64 submission,
                              bool uploaded)
{
    if (!guard)
        return;
    QMetaObject::invokeMethod(
            guard,
            [guard, token, submission, uploaded] {
                publishPrepared(guard, token, submission, uploaded);
            },
            Qt::QueuedConnection);
}
void SceneItem::publishPrepared(const QPointer<SceneItem> &guard, quint64 token, quint64 submission,
                                bool uploaded)
{
    if (!guard || !guard->m_renderingActive || !guard->m_preparing
        || token != guard->m_preparationToken || submission != guard->submission() || !uploaded)
        return;
    if (!guard->m_sceneCovers.prepared())
        return;
    Q_EMIT guard->scenePrepared();
}
void SceneItem::bindWindow(QQuickWindow *window)
{
    disconnect(m_windowStateConnection);
    disconnect(m_visibilityConnection);
    disconnect(m_activeConnection);
    disconnect(m_dprConnection);
    disconnect(m_screenConnection);
    mouseUngrabEvent();
    if (window) {
        m_screenConnection = connect(window, &QWindow::screenChanged, this, &SceneItem::schedule);
        m_dprConnection =
                connect(window, &QQuickWindow::devicePixelRatioChanged, this, &SceneItem::schedule);
        m_visibilityConnection =
                connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
                    handleVisibility(visible);
                });
        m_windowStateConnection = connect(window, &QWindow::visibilityChanged, this,
                                          &SceneItem::configureDiagnostics);
        m_activeConnection = connect(window, &QWindow::activeChanged, this, [this, window] {
            if (!window->isActive())
                mouseUngrabEvent();
        });
    }
    configureDiagnostics();
    schedule();
}
SceneItem::SceneItem(QQuickItem *parent) : RhiItem(parent)
{
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            &SceneItem::configureDiagnostics);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
    setFlag(QQuickItem::ItemIsFocusScope, true);
    setFocus(true);
    connect(&m_sceneCovers, &SceneCovers::changed, this, &SceneItem::schedule);
    connect(this, &QQuickItem::widthChanged, this, &SceneItem::schedule);
    connect(this, &RhiItem::hdrHeadroomChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::heightChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::visibleChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::visibleChanged, this, &SceneItem::configureDiagnostics);
    connect(this, &RhiItem::renderFailed, this, [this](const QString &message) {
        m_errorText = message;
        Q_EMIT errorTextChanged();
    });
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
        return bindWindow(window);
    });
}
void SceneItem::setAdapter(SceneAdapter *adapter)
{
    if (m_adapter == adapter)
        return;
    if (m_adapter) {
        if (m_adapter->selection())
            m_adapter->selection()->cancelDrag();
        disconnect(m_adapter, nullptr, this, nullptr);
        disconnect(m_adapter->lighting(), nullptr, this, nullptr);
        disconnect(m_adapter->quality(), nullptr, this, nullptr);
    }
    m_adapter = adapter;
    if (adapter) {
        connect(adapter, &SceneAdapter::frameChanged, this, [this, adapter] {
            m_sceneCovers.observeFrame(adapter->frame());
            schedule();
        });
        connect(adapter, &QObject::destroyed, this, [this] {
            m_sceneCovers.setSelection(nullptr);
        });
        connect(adapter->lighting(), &SelectionLighting::changed, this, &SceneItem::schedule);
        connect(adapter->quality(), &RenderQuality::changed, this, &SceneItem::schedule);
        connect(adapter, &SceneAdapter::selectionControllerChanged, this, &SceneItem::schedule);
    }
    m_sceneCovers.setSelection(adapter ? adapter->selection() : nullptr);
    if (adapter)
        m_sceneCovers.observeFrame(adapter->frame());
    schedule();
    Q_EMIT adapterChanged();
}
void SceneItem::setInspectionEnabled(bool enabled)
{
    if (m_inspectionEnabled == enabled)
        return;
    m_inspectionEnabled = enabled;
    if (!enabled)
        mouseUngrabEvent();
    Q_EMIT inspectionEnabledChanged();
}
void SceneItem::setGeometryOnly(bool enabled)
{
    if (m_geometryOnly == enabled)
        return;
    m_geometryOnly = enabled;
    schedule();
    Q_EMIT geometryOnlyChanged();
}
void SceneItem::setDiagnosticsEnabled(bool enabled)
{
    if (m_diagnosticsEnabled == enabled)
        return;
    m_diagnosticsEnabled = enabled;
    configureDiagnostics();
    Q_EMIT diagnosticsChanged();
}
void SceneItem::publishDiagnostics(const std::shared_ptr<DiagnosticMailbox> &mailbox,
                                   const QPointer<SceneItem> &guard, quint64 epoch)
{
    DiagnosticBatch batch;
    DiagnosticSeries presented;
    {
        const QMutexLocker lock(&mailbox->mutex);
        batch = *mailbox->latest;
        presented = mailbox->latestPresented;
        mailbox->latest.reset();
        mailbox->queued = false;
    }
    if (!guard || epoch != guard->m_diagnosticsEpoch)
        return;
    const auto cache = guard->m_sceneCovers.cacheStatistics();
    guard->m_diagnosticValues = {
        { u"cpu"_s, batch.cpu.distribution() },
        { u"sync"_s, batch.sync.distribution() },
        { u"gpu"_s, batch.gpu.distribution() },
        { u"submissionIntervals"_s, batch.intervals.distribution() },
        { u"qtFrameIntervals"_s, presented.distribution() },
        { u"submissions"_s, batch.submissions },
        { u"elapsedMs"_s, batch.elapsedMs },
        { u"submissionRate"_s,
          batch.intervals.seen() * yalahoo::units::MillisecondsPerSecond / batch.elapsedMs },
        { u"atlasUploadBytes"_s, batch.atlasBytes },
        { u"selectedUploadBytes"_s, batch.selectedBytes },
        { u"uniformUploadBytes"_s, batch.uniformBytes },
        { u"decodeStarts"_s, cache.value(u"decodeStarts"_s).toVariant() },
        { u"decodeActive"_s, cache.value(u"active"_s).toVariant() },
        { u"decodePending"_s, cache.value(u"pending"_s).toVariant() },
        { u"requestedSwapInterval"_s, guard->window()->requestedFormat().swapInterval() }
    };
    Q_EMIT guard->diagnosticValuesChanged();
}
void SceneItem::queueDiagnostics(const std::shared_ptr<DiagnosticMailbox> &mailbox,
                                 const QPointer<SceneItem> &guard, const quint64 &epoch,
                                 DiagnosticSample sample)
{
    const QMutexLocker lock(&mailbox->mutex);
    auto batch = mailbox->samples.add(sample);
    if (!batch)
        return;
    mailbox->latest = *batch;
    mailbox->latestPresented = mailbox->presented;
    mailbox->presented = { };
    if (mailbox->queued)
        return;
    mailbox->queued = true;
    if (guard)
        QMetaObject::invokeMethod(
                guard,
                [mailbox, guard, epoch] {
                    publishDiagnostics(mailbox, guard, epoch);
                },
                Qt::QueuedConnection);
}
void SceneItem::configureDiagnostics()
{
    ++m_callbackRevision;
    const auto epoch = ++m_diagnosticsEpoch;
    diagnosed = { };
    disconnect(m_presentedConnection);
    m_diagnosticValues.clear();
    Q_EMIT diagnosticValuesChanged();
    // Explicit Review targets can submit while the native window stays hidden.
    if (!m_renderingActive || !m_diagnosticsEnabled || !isVisible() || !window()
        || QGuiApplication::applicationState() == Qt::ApplicationHidden
        || QGuiApplication::applicationState() == Qt::ApplicationSuspended
        || (!window()->isVisible() && window()->renderTarget().isNull())
        || window()->visibility() == QWindow::Minimized) {
        schedule();
        return;
    }

    auto mailbox = std::make_shared<DiagnosticMailbox>();
    m_presentedConnection = connect(
            window(), &QQuickWindow::frameSwapped, this,
            [mailbox] {
                const auto now = yalahoo::monotonicMilliseconds();
                const QMutexLocker lock(&mailbox->mutex);
                if (mailbox->previousPresentation)
                    mailbox->presented.add(now - *mailbox->previousPresentation);
                mailbox->previousPresentation = now;
            },
            Qt::DirectConnection);
    QPointer<SceneItem> guard(this);
    diagnosed = [mailbox, guard, epoch](DiagnosticSample sample) {
        return queueDiagnostics(mailbox, guard, epoch, sample);
    };
    schedule();
}
void SceneItem::setTelemetryEnabled(bool enabled)
{
    if (m_telemetryEnabled == enabled)
        return;
    m_telemetryEnabled = enabled;
    configureTelemetry();
    schedule();
    Q_EMIT telemetryEnabledChanged();
}
void SceneItem::publishTelemetry(const std::shared_ptr<TelemetryMailbox> &mailbox,
                                 const QPointer<SceneItem> &guard, quint64 epoch)
{
    QVector<QJsonObject> batch;
    quint64 dropped = 0;
    {
        const QMutexLocker lock(&mailbox->mutex);
        batch.swap(mailbox->samples);
        dropped = mailbox->coalesced;
        mailbox->queued = false;
    }
    if (guard && epoch == guard->m_telemetryEpoch && guard->m_renderingActive
        && guard->telemetryEnabled())
        for (auto &sample : batch) {
            if (!guard || epoch != guard->m_telemetryEpoch || !guard->m_renderingActive
                || !guard->telemetryEnabled())
                break;
            sample[u"coalescedSamples"_s] = qint64(dropped);
            Q_EMIT guard->frameSubmitted(std::move(sample));
        }
}
void SceneItem::queueTelemetry(const std::shared_ptr<TelemetryMailbox> &mailbox,
                               const QPointer<SceneItem> &guard, const quint64 &epoch,
                               QJsonObject sample)
{
    const QMutexLocker lock(&mailbox->mutex);
    if (mailbox->samples.size() == MaximumFeedbackSamples) {
        mailbox->samples.removeFirst();
        ++mailbox->coalesced;
    }
    mailbox->samples.append(std::move(sample));
    if (mailbox->queued)
        return;
    mailbox->queued = true;
    if (guard)
        QMetaObject::invokeMethod(
                guard,
                [mailbox, guard, epoch] {
                    publishTelemetry(mailbox, guard, epoch);
                },
                Qt::QueuedConnection);
}
void SceneItem::configureTelemetry()
{
    ++m_callbackRevision;
    const auto epoch = ++m_telemetryEpoch;
    profiled = { };
    if (m_telemetryEnabled && m_renderingActive) {

        auto mailbox = std::make_shared<TelemetryMailbox>();
        QPointer<SceneItem> guard(this);
        profiled = [mailbox, guard, epoch](QJsonObject sample) {
            return queueTelemetry(mailbox, guard, epoch, std::move(sample));
        };
    }
}
QJsonObject SceneItem::coverStatistics() const
{
    return m_sceneCovers.statistics();
}
void SceneItem::setRenderingActive(bool active)
{
    if (m_renderingActive == active)
        return;
    m_renderingActive = active;
    m_sceneCovers.setSuspended(!active);
    configureTelemetry();
    configureDiagnostics();
    if (active) {
        schedule();
        update();
    } else {
        mouseUngrabEvent();
    }
    Q_EMIT renderingActiveChanged();
}
void SceneItem::setSceneOccluded(bool value)
{
    if (m_sceneOccluded == value)
        return;
    m_sceneOccluded = value;
    schedule();
    Q_EMIT openingChanged();
}
void SceneItem::restartPreparation()
{
    setPreparing(false);
    setPreparing(true);
}
void SceneItem::setPreparing(bool value)
{
    if (m_preparing == value)
        return;
    m_preparing = value;
    ++m_preparationToken;
    prepared = { };
    if (value) {
        QPointer<SceneItem> guard(this);
        prepared = [guard](quint64 token, quint64 submission, bool uploaded) {
            queuePrepared(guard, token, submission, uploaded);
        };
    }
    schedule();
    Q_EMIT openingChanged();
}
void SceneItem::setPanelLayerSize(QSize value)
{
    if (m_panelLayerSize == value)
        return;
    m_panelLayerSize = value;
    schedule();
    Q_EMIT openingChanged();
}
void SceneItem::setOpeningLayerSize(QSize value)
{
    if (m_openingLayerSize == value)
        return;
    m_openingLayerSize = value;
    schedule();
    Q_EMIT openingChanged();
}
void SceneItem::schedule()
{
    // Qt coalesces polish: camera, lighting and quality notifications publish once per frame.
    if (m_renderingActive)
        polish();
}
bool SceneItem::event(QEvent *event)
{
    if (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate) {
        mouseUngrabEvent();
        m_touchStart.reset();
        m_touchInspecting = false;
    }
    if (event->type() == QEvent::DevicePixelRatioChange)
        schedule();
    return RhiItem::event(event);
}
bool SceneItem::selectAt(QPointF point)
{
    if (!m_adapter || !m_adapter->selection() || (m_inputRouter && !m_inputRouter->enabled()))
        return false;
    const auto hit = m_adapter->hitTest(point.x(), point.y());
    if (hit.isEmpty())
        return false;
    if (m_inputRouter)
        m_inputRouter->reset();
    if (!window() || !window()->flags().testFlag(Qt::WindowDoesNotAcceptFocus))
        forceActiveFocus(Qt::MouseFocusReason);
    m_adapter->recordInput();
    const bool selected = m_adapter->selection()->selectCell(
            { hit.value(u"lane"_s).toDouble(), hit.value(u"row"_s).toDouble() }, true);
    if (selected) {
        m_adapter->selection()->overviewAlbumClicked();
        m_adapter->recordNavigationCommit(m_adapter->selection()->revision());
    }
    return selected;
}
void SceneItem::mousePressEvent(QMouseEvent *event)
{
    auto *selection = m_adapter ? m_adapter->selection() : nullptr;
    m_mouseStart.reset();
    if (!selection || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    if (m_inspectionEnabled && selection->beginDrag(event->position())) {
        m_adapter->recordInput();
        event->accept();
    } else if (!m_inputRouter || m_inputRouter->enabled()) {
        m_mouseStart = event->position();
        m_mouseMoved = false;
        event->accept();
    } else
        event->ignore();
}
void SceneItem::mouseMoveEvent(QMouseEvent *event)
{
    auto *selection = m_adapter ? m_adapter->selection() : nullptr;
    if (selection && selection->dragging()) {
        event->setAccepted(selection->moveDrag(event->position()));
        return;
    }
    if (m_mouseStart) {
        const auto delta = event->position() - *m_mouseStart;
        m_mouseMoved |= std::hypot(delta.x(), delta.y()) > ClickSlopPixels;
        event->accept();
    } else
        event->ignore();
}
void SceneItem::mouseReleaseEvent(QMouseEvent *event)
{
    auto *selection = m_adapter ? m_adapter->selection() : nullptr;
    if (selection && selection->dragging()) {
        selection->endDrag(event->position());
        event->accept();
    } else if (m_mouseStart) {
        const auto delta = event->position() - *m_mouseStart;
        const bool click = !m_mouseMoved && std::hypot(delta.x(), delta.y()) <= ClickSlopPixels;
        m_mouseStart.reset();
        event->setAccepted(click && selectAt(event->position()));
    } else
        event->ignore();
}
void SceneItem::mouseUngrabEvent()
{
    m_mouseStart.reset();
    m_touchStart.reset();
    m_touchInspecting = false;
    if (m_adapter && m_adapter->selection())
        m_adapter->selection()->cancelDrag();
}
void SceneItem::hoverMoveEvent(QHoverEvent *event)
{
    if (m_adapter && width() > 0 && height() > 0)
        m_adapter->setPointer(event->position().x() / width() - .5,
                              event->position().y() / height() - .5);
    event->accept();
}
void SceneItem::hoverLeaveEvent(QHoverEvent *event)
{
    if (m_adapter)
        m_adapter->setPointer(0, 0);
    event->accept();
}
InputRouter *SceneItem::inputRouter() const
{
    return m_inputRouter;
}
void SceneItem::setInputRouter(InputRouter *router)
{
    if (m_inputRouter == router)
        return;
    m_inputRouter = router;
    Q_EMIT inputRouterChanged();
}
void SceneItem::wheelEvent(QWheelEvent *event)
{
    event->setAccepted(m_inputRouter && m_inputRouter->wheel(event, window()));
}
void SceneItem::keyPressEvent(QKeyEvent *event)
{
    event->setAccepted(m_inputRouter && m_inputRouter->key(event, window()));
}
void SceneItem::touchEvent(QTouchEvent *event)
{
    auto *selection = m_adapter ? m_adapter->selection() : nullptr;
    if (!selection || event->type() == QEvent::TouchCancel || event->points().size() != 1) {
        m_touchStart.reset();
        m_touchInspecting = false;
        if (selection)
            selection->cancelDrag();
        event->accept();
        return;
    }
    const auto &point = event->points().first();
    if (event->type() == QEvent::TouchBegin) {
        m_touchInspecting =
                m_inspectionEnabled && selection->beginDrag(point.position(), point.id());
        if (m_touchInspecting) {
            m_adapter->recordInput();
            event->accept();
            return;
        }
        if (m_inputRouter && !m_inputRouter->enabled()) {
            event->ignore();
            return;
        }
        m_touchStart = point.position();
        m_touchTimestamp = event->timestamp();
    } else if (m_touchInspecting) {
        if (event->type() == QEvent::TouchEnd) {
            selection->endDrag(point.position(), point.id());
            m_touchInspecting = false;
        } else
            selection->moveDrag(point.position(), point.id());
    } else if (event->type() == QEvent::TouchEnd && m_touchStart) {
        const auto delta = point.position() - *m_touchStart;
        m_touchStart.reset();
        const bool swiped = m_inputRouter
                && m_inputRouter->swipe(delta, double(event->timestamp() - m_touchTimestamp));
        if (!swiped && std::hypot(delta.x(), delta.y()) <= ClickSlopPixels)
            selectAt(point.position());
    }
    event->accept();
}
// Prepare a coherent scene/cover snapshot on the GUI thread after layout. Hidden or opaque
// opening states return before pool preparation; only the renderer owns native submission resources.
void SceneItem::submitOccluded()
{

    if (!input().sceneOccluded) {
        auto next = input();
        next.sceneOccluded = true;
        next.preparationToken = 0;
        if (next.cover.isNull())
            next.cover = m_sceneCovers.image();
        auto target = QSize(fixedColorBufferWidth(), fixedColorBufferHeight());
        if (target.isEmpty())
            target = m_adapter->quality()->targetSize();
        submit(std::move(next), target.expandedTo(QSize(1, 1)));
    }
    return;
}
double SceneItem::projectedCoverDemand(const QMatrix4x4 &vp, const QMatrix4x4 &model) const
{
    double demand = renderData::CoverTilePixels;
    const auto projected = vp * model;

    std::array<QPointF, PaperCornerCount> points;
    int index = 0;
    for (float y : { yalahoo::rhi::renderData::PaperBottom, yalahoo::rhi::renderData::PaperTop })
        for (float x : { -yalahoo::rhi::renderData::PaperLeftMagnitude,
                         yalahoo::rhi::renderData::PaperRight }) {
            const auto clip = projected * QVector4D(x, y, yalahoo::rhi::renderData::PaperFront, 1);
            if (clip.w() <= 0)
                return renderData::CoverTilePixels;
            points[index++] = QPointF(clip.x() / clip.w() * width() * .5,
                                      clip.y() / clip.w() * height() * .5);
        }
    struct PaperEdge final
    {
        int start, end;
    };
    static constexpr PaperEdge edges[]{ { 0, 1 }, { 0, 2 }, { 1, 3 }, { 2, 3 } };
    const double dpr = window()->effectiveDevicePixelRatio();
    for (const auto edge : edges)
        demand = std::max(demand,
                          QLineF(points[edge.start], points[edge.end]).length() * dpr
                                  * PaperTileExtent / PaperInteriorExtent);

    return demand;
}
void SceneItem::updatePolish()
try {
    if (!m_renderingActive || !m_adapter || !window() || !isVisible() || width() <= 0
        || height() <= 0)
        return;
    if (m_sceneOccluded) {
        submitOccluded();
        return;
    }
    auto &adapter = *m_adapter;
    m_sceneCovers.setSelection(adapter.selection());
    // Cover demand follows the projected paper at native DPR, including its source inset.
    // A power-of-two cache tier avoids decoding again for every animation pixel.
    double demand = renderData::CoverTilePixels;
    const auto &frame = adapter.frame();
    const auto vp = frame.projection * frame.view;
    const auto inspect = [&](const QMatrix4x4 &model) {
        demand = std::max(demand, projectedCoverDemand(vp, model));
    };
    if (frame.instances.isEmpty())
        inspect(frame.caseTransform);
    else
        for (const auto &instance : frame.instances)
            if (instance.owned)
                inspect(instance.transform);
    m_sceneCovers.setDemand(demand);
    auto &quality = *adapter.quality();
    const double dpr = window()->effectiveDevicePixelRatio();
    quality.resize(width(), height(), dpr);
    const auto &l = *adapter.lighting();
    const double depthOfField = quality.depthOfField();
    Input in;
    in.preparationToken = m_preparing ? m_preparationToken : 0;
    in.openingLayerSize = QSize(
            int(std::ceil(m_openingLayerSize.width() * window()->effectiveDevicePixelRatio())),
            int(std::ceil(m_openingLayerSize.height() * window()->effectiveDevicePixelRatio())));
    // Review capture tokens survive a coalesced camera/cover polish before synchronization.
    in.request = input().request;
#ifdef YALAHOO_RHI_AUDIT
    in.opticalProbe = input().opticalProbe;
#endif
    in.captureAtlas = input().captureAtlas;
    in.scene = adapter.frame();
    in.panelLayerSize = m_panelLayerSize * dpr;
    in.navigationDpr = m_inputRouter ? window()->effectiveDevicePixelRatio() : 0;
    in.textLayerSizes = m_textSizes;
    in.textResourceRevision = m_textRevision;
    m_sceneCovers.applyImage(in);
    in.geometryDiagnostic = m_geometryOnly;
    in.fogEnabled = !m_geometryOnly;
    in.productFloor = !m_geometryOnly;
    in.floorColor = l.palette().floor.qt();
    in.shells = !m_geometryOnly;
    in.showPaper = !m_geometryOnly;
    in.captureSamples = CaptureSamples;
    in.hdrHeadroom = hdrHeadroom();
    in.hdrActive = hdrActive();
    if (in.hdrActive)
        in.displayLayerSize = window()->size() * window()->effectiveDevicePixelRatio();
    in.profile = m_telemetryEnabled;
    in.shadows = !m_geometryOnly && quality.shadows() > 0;
    in.shadowSize = m_geometryOnly ? 1 : qMax(1, quality.shadows());
    in.postprocess = !m_geometryOnly;
    in.aoSamples = m_geometryOnly ? 0 : quality.aoSamples();
    in.bokeh = !m_geometryOnly && depthOfField > 0;
    in.smaa = !m_geometryOnly;
#ifdef YALAHOO_RHI_AUDIT
    if (property("_yalahooReviewOpticsOnly").toBool()) {
        in.postprocess = false;
        in.aoSamples = 0;
        in.bokeh = false;
        in.smaa = false;
    }
#endif
    in.focus = float(adapter.focalDepth(adapter.casePosition()));
    in.aperture = float((BrowseAperture + DetailApertureGain * adapter.detail()) * depthOfField
                        / PercentScale);
    in.clarity = adapter.frame().individualMaterials ? 0 : float(adapter.detail());
    in.setLighting(l);
    const auto target = quality.targetSize();
    const auto &old = input();
    const bool unchanged = m_callbackRevision == m_publishedCallbackRevision && !old.sceneOccluded
            && in.preparationToken == old.preparationToken
            && in.openingLayerSize == old.openingLayerSize
            && in.panelLayerSize == old.panelLayerSize && in.scene.revision == old.scene.revision
            && in.scene.navigationRevision == old.scene.navigationRevision
            && in.scene.navigationCommittedNs == old.scene.navigationCommittedNs
            && in.navigationDpr == old.navigationDpr
            && in.textResourceRevision == old.textResourceRevision
            && m_sceneCovers.revision() == m_publishedCoverRevision
            && in.scene.inputRevision == old.scene.inputRevision
            && in.cover.cacheKey() == old.cover.cacheKey() && in.filterCovers == old.filterCovers
            && target == QSize(fixedColorBufferWidth(), fixedColorBufferHeight())
            && in.geometryDiagnostic == old.geometryDiagnostic && in.shells == old.shells
            && in.fogEnabled == old.fogEnabled && in.profile == old.profile
            && in.productFloor == old.productFloor && in.floorColor == old.floorColor
            && in.shadows == old.shadows && in.shadowSize == old.shadowSize
            && in.captureSamples == old.captureSamples && in.aoSamples == old.aoSamples
            && in.hdrHeadroom == old.hdrHeadroom && in.hdrActive == old.hdrActive
            && in.displayLayerSize == old.displayLayerSize && in.postprocess == old.postprocess
            && in.bokeh == old.bokeh && in.smaa == old.smaa && in.focus == old.focus
            && in.aperture == old.aperture && in.clarity == old.clarity
            && in.background == old.background && in.keyColor == old.keyColor
            && in.skyColor == old.skyColor && in.groundColor == old.groundColor
            && in.lightIntensities == old.lightIntensities && in.spotPosition == old.spotPosition
            && in.spotTarget == old.spotTarget && in.spotColor == old.spotColor
            && in.spotIntensity == old.spotIntensity
            && in.environmentIntensity == old.environmentIntensity
            && in.rowSettings == old.rowSettings && in.rowColumn == old.rowColumn
            && in.rowDetail == old.rowDetail && in.rowFill == old.rowFill
            && in.lightColumn == old.lightColumn && in.printAmbient == old.printAmbient
            && in.scatterColor == old.scatterColor && in.scatterStrength == old.scatterStrength
            && in.areaPosition == old.areaPosition && in.areaTarget == old.areaTarget
            && in.areaIntensity == old.areaIntensity && in.warmth == old.warmth
            && in.exposure == old.exposure;
    if (unchanged)
        return;
    if (m_sceneCovers.poolMode())
        m_sceneCovers.prepare(in, input());
    else {
        in.entities = { in.scene.caseTransform };
        in.entityIds = { 1 };
    }
    m_publishedCallbackRevision = m_callbackRevision;
    m_publishedCoverRevision = m_sceneCovers.revision();
    submit(std::move(in), target);
} catch (const std::runtime_error &error) {
    m_errorText = QString::fromUtf8(error.what());
    setRenderingActive(false);
    Q_EMIT errorTextChanged();
}
}
