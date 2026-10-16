#include "SceneItem.h"
#include <QQuickRenderTarget>
#include <chrono>
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
#include <limits>
#include <cmath>

namespace yalahoo::rhi {
TitleReel *SceneItem::titleReel() const
{
    return m_titleReel;
}
void SceneItem::setTitleReel(TitleReel *reel)
{
    if (m_titleReel == reel)
        return;
    disconnect(m_titleConnection);
    disconnect(m_titleDestroyedConnection);
    m_titleReel = reel;
    if (reel) {
        m_titleConnection =
                connect(reel, &TitleReel::resourcesChanged, this, &SceneItem::refreshTextResources);
        m_titleDestroyedConnection =
                connect(reel, &QObject::destroyed, this, &SceneItem::refreshTextResources);
    }
    refreshTextResources();
    emit textReelsChanged();
}
TitleReel *SceneItem::transportReel() const
{
    return m_transportReel;
}
void SceneItem::setTransportReel(TitleReel *reel)
{
    if (m_transportReel == reel)
        return;
    disconnect(m_transportConnection);
    disconnect(m_transportDestroyedConnection);
    m_transportReel = reel;
    if (reel) {
        m_transportConnection =
                connect(reel, &TitleReel::resourcesChanged, this, &SceneItem::refreshTextResources);
        m_transportDestroyedConnection =
                connect(reel, &QObject::destroyed, this, &SceneItem::refreshTextResources);
    }
    refreshTextResources();
    emit textReelsChanged();
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
SceneItem::SceneItem(QQuickItem *parent)
    : RhiItem(parent), m_cover(256, 256, QImage::Format_RGBA8888_Premultiplied)
{
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            &SceneItem::configureDiagnostics);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
    setFlag(QQuickItem::ItemIsFocusScope, true);
    setFocus(true);
    m_cover.fill(Qt::transparent);
    m_selectedCover = m_coverPool.acquire(QStringLiteral("demo-square"));
    connect(&m_covers, &CoverCache::ready, this, [this](CoverSnapshot pixels) {
        if (m_poolMode) {
            if (!m_adapter)
                return;
            const auto &instances = m_adapter->frame().instances;
            const auto activeObject = instances.isEmpty() ? 0 : instances.last().objectId;
            if (pixels->resolution == 256) {
                const auto lease = m_poolLeases.constFind(pixels->identity);
                if (lease != m_poolLeases.cend() && m_atlas.publish(*lease, pixels))
                    m_readyCovers.insert(pixels->identity);
            }
            for (auto &owned : m_ownedCovers)
                if (owned.objectId == activeObject && owned.pixels
                    && owned.pixels->identity == pixels->identity)
                    m_coverPool.upgrade(owned.pixels, pixels);
            ++m_coverRevision;
            schedule();
            return;
        }
        if (m_coverPool.upgrade(m_selectedCover, pixels)) {
            m_cover = pixels->image;
            schedule();
        }
    });
    connect(this, &QQuickItem::widthChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::heightChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::visibleChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::visibleChanged, this, &SceneItem::configureDiagnostics);
    connect(this, &RhiItem::renderFailed, this, [this](const QString &message) {
        m_errorText = message;
        emit errorTextChanged();
    });
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
        disconnect(m_windowStateConnection);
        disconnect(m_visibilityConnection);
        disconnect(m_activeConnection);
        disconnect(m_dprConnection);
        disconnect(m_screenConnection);
        mouseUngrabEvent();
        if (window) {
            m_screenConnection =
                    connect(window, &QWindow::screenChanged, this, &SceneItem::schedule);
            m_dprConnection = connect(window, &QQuickWindow::devicePixelRatioChanged, this,
                                      &SceneItem::schedule);
            m_visibilityConnection =
                    connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
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
        connect(adapter, &SceneAdapter::frameChanged, this, &SceneItem::schedule);
        connect(adapter->lighting(), &SelectionLighting::changed, this, &SceneItem::schedule);
        connect(adapter->quality(), &RenderQuality::changed, this, &SceneItem::schedule);
        connect(adapter, &SceneAdapter::selectionControllerChanged, this, &SceneItem::schedule);
    }
    schedule();
    emit adapterChanged();
}
void SceneItem::resetPool()
{
    m_covers.reset();
    m_coverPool.reset();
    m_selectedCover.reset();
    m_poolLeases.clear();
    m_poolSources.clear();
    m_readyCovers.clear();
    m_atlas.reset();
    m_ownedCovers = { };
    m_activeObject = 0;
    ++m_coverRevision;
    schedule();
}
void SceneItem::refreshPool()
{
    if (!m_poolSelection)
        return;
    // Only the bounded visible pool is reconciled; content-addressed native paths
    // carry revisions, while missing artwork also depends on its fallback title.
    for (auto it = m_poolSources.begin(); it != m_poolSources.end();) {
        auto next = m_poolSelection->coverSource(it.key());
        if (!next.first.isEmpty())
            next.second.clear();
        if (it.value() == next) {
            ++it;
            continue;
        }
        const auto id = it.key();
        m_covers.invalidate(id);
        m_atlas.release(m_poolLeases.take(id));
        m_readyCovers.remove(id);
        for (auto &owned : m_ownedCovers)
            if (owned.pixels && owned.pixels->identity == id)
                owned = { };
        it = m_poolSources.erase(it);
    }
    ++m_coverRevision;
    schedule();
}
void SceneItem::preparePool(Input &in)
{
    auto *selection = m_adapter->selection();
    m_activeObject = in.scene.instances.isEmpty() ? 0 : in.scene.instances.last().objectId;
    in.entities.clear();
    in.entityIds.clear();
    QSet<QString> wanted;
    QSet<quint64> ownedIds;
    const int count = int(in.scene.instances.size());
    in.entities.resize(count);
    in.entityIds.resize(count);
    in.coverTiles.resize(count);
    in.coverLeases.resize(count);
    in.fallbackCoverTiles.resize(count);
    in.fallbackCoverLeases.resize(count);
    for (const auto &instance : in.scene.instances) {
        wanted.insert(instance.albumId);
        if (instance.owned)
            ownedIds.insert(instance.objectId);
    }
    for (auto it = m_poolLeases.begin(); it != m_poolLeases.end();) {
        if (!wanted.contains(it.key())) {
            m_covers.cancel(it.key());
            m_atlas.release(it.value());
            m_readyCovers.remove(it.key());
            m_poolSources.remove(it.key());
            it = m_poolLeases.erase(it);
        } else
            ++it;
    }
    for (auto &owned : m_ownedCovers) {
        if (!owned.objectId || ownedIds.contains(owned.objectId))
            continue;
        if (in.scene.instances.isEmpty()) {
            owned = { };
            continue;
        }
        const auto &active = in.scene.instances.last();
        if (active.owned && owned.pixels && owned.pixels->identity == active.albumId
            && std::none_of(m_ownedCovers.begin(), m_ownedCovers.end(),
                            [&](const OwnedCover &other) {
                                return other.objectId == active.objectId;
                            }))
            owned.objectId = active.objectId;
        else
            owned = { };
    }
    // Source descriptors are indexed at library/sort boundaries, outside the frame path.
    for (int offset = 0; offset < count; ++offset) {
        // Admit the selected cover first, without changing the instance/sort ordering.
        const int index = offset == 0 ? count - 1 : offset - 1;
        const auto &instance = in.scene.instances[index];
        const auto &id = instance.albumId;
        const auto source = selection->coverSource(id);
        if (!m_poolLeases.contains(id)) {
            m_poolLeases.insert(id, m_atlas.acquire(id, m_covers.generation()));
            auto key = source;
            if (!key.first.isEmpty())
                key.second.clear();
            m_poolSources.insert(id, key);
        }
        const auto lease = m_poolLeases.value(id);
        auto tile = m_atlas.tile(lease);
        auto coverLease = lease.lease;
        in.fallbackCoverTiles[index] = tile;
        in.fallbackCoverLeases[index] = coverLease;
        if (!m_readyCovers.contains(id)) {
            if (auto pixels = m_covers.lookup(id, source.first, 256, source.second)) {
                if (m_atlas.publish(lease, pixels))
                    m_readyCovers.insert(id);
            } else if (m_covers.hasPendingCapacity())
                m_covers.request(id, source.first, 256, source.second);
        }
        if (instance.owned) {
            auto it = std::find_if(m_ownedCovers.begin(), m_ownedCovers.end(),
                                   [&](const OwnedCover &c) {
                                       return c.objectId == instance.objectId;
                                   });
            if (it == m_ownedCovers.end()) {
                it = std::find_if(m_ownedCovers.begin(), m_ownedCovers.end(),
                                  [](const OwnedCover &c) {
                                      return c.objectId == 0;
                                  });
                if (it != m_ownedCovers.end()) {
                    it->objectId = instance.objectId;
                    it->lease = ++m_ownedSerial;
                    it->pixels = m_coverPool.acquire(
                            id, m_covers.lookup(id, source.first, 256, source.second),
                            m_covers.generation());
                }
            }
            if (it != m_ownedCovers.end() && it->pixels) {
                if (instance.objectId == m_activeObject
                    && (!it->pixels->pixels || it->pixels->pixels->resolution != 1024)) {
                    if (auto pixels = m_covers.lookup(id, source.first, 1024, source.second))
                        m_coverPool.upgrade(it->pixels, pixels);
                    else if (m_covers.hasPendingCapacity())
                        m_covers.request(id, source.first, 1024, source.second);
                }
                const auto pixels = it->pixels->pixels;
                if (pixels && pixels->resolution == 1024) {
                    const int block = int(it - m_ownedCovers.begin());
                    tile = { block * 1024, 5632, 1024, 1024 };
                    coverLease = it->lease;
                    if (it->tiledPixels != pixels) {
                        it->uploads.clear();
                        for (int y = 0; y < 1024; y += 256)
                            for (int x = 0; x < 1024; x += 256) {
                                const int slot = ((tile.y() + y) / 256) * 22 + (tile.x() + x) / 256;
                                it->uploads.append({ m_atlas.generation(), it->lease,
                                                     ++m_ownedSerial, slot,
                                                     pixels->image.copy(x, y, 256, 256), true });
                            }
                        it->tiledPixels = pixels;
                    }
                    in.atlasUploads += it->uploads;
                }
            }
        }
        in.entities[index] = instance.transform;
        in.entityIds[index] = instance.objectId;
        in.coverTiles[index] = tile;
        in.coverLeases[index] = coverLease;
    }
    in.atlasSize = { 5632, 6656 };
    in.atlasGeneration = m_atlas.generation();
    in.poolFrame = true;
    // Immutable replay handles render-context recreation and superseded GUI submissions;
    // the render queue rejects known revisions before conversion or upload.
    in.atlasUploads += m_atlas.replay(0);
    in.immutableCover = true;
}
void SceneItem::setInspectionEnabled(bool enabled)
{
    if (m_inspectionEnabled == enabled)
        return;
    m_inspectionEnabled = enabled;
    if (!enabled)
        mouseUngrabEvent();
    emit inspectionEnabledChanged();
}
void SceneItem::setGeometryOnly(bool enabled)
{
    if (m_geometryOnly == enabled)
        return;
    m_geometryOnly = enabled;
    schedule();
    emit geometryOnlyChanged();
}
void SceneItem::setDiagnosticsEnabled(bool enabled)
{
    if (m_diagnosticsEnabled == enabled)
        return;
    m_diagnosticsEnabled = enabled;
    configureDiagnostics();
    emit diagnosticsChanged();
}
void SceneItem::configureDiagnostics()
{
    ++m_callbackRevision;
    const auto epoch = ++m_diagnosticsEpoch;
    diagnosed = { };
    disconnect(m_presentedConnection);
    m_diagnosticValues.clear();
    emit diagnosticValuesChanged();
    // Explicit Review targets can submit while the native window stays hidden.
    if (!m_renderingActive || !m_diagnosticsEnabled || !isVisible() || !window()
        || QGuiApplication::applicationState() == Qt::ApplicationHidden
        || QGuiApplication::applicationState() == Qt::ApplicationSuspended
        || (!window()->isVisible() && window()->renderTarget().isNull())
        || window()->visibility() == QWindow::Minimized) {
        schedule();
        return;
    }
    struct Mailbox
    {
        QMutex mutex;
        DiagnosticSamples samples;
        DiagnosticSeries presented;
        std::optional<double> previousPresentation;
        std::optional<DiagnosticBatch> latest;
        DiagnosticSeries latestPresented;
        bool queued = false;
    };
    auto mailbox = std::make_shared<Mailbox>();
    m_presentedConnection = connect(
            window(), &QQuickWindow::frameSwapped, this,
            [mailbox] {
                const auto now = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now().time_since_epoch())
                                         .count();
                const QMutexLocker lock(&mailbox->mutex);
                if (mailbox->previousPresentation)
                    mailbox->presented.add(now - *mailbox->previousPresentation);
                mailbox->previousPresentation = now;
            },
            Qt::DirectConnection);
    QPointer<SceneItem> guard(this);
    diagnosed = [mailbox, guard, epoch](DiagnosticSample sample) {
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
                        const auto cache = guard->m_covers.statistics();
                        guard->m_diagnosticValues = {
                            { "cpu", batch.cpu.distribution() },
                            { "sync", batch.sync.distribution() },
                            { "gpu", batch.gpu.distribution() },
                            { "submissionIntervals", batch.intervals.distribution() },
                            { "qtFrameIntervals", presented.distribution() },
                            { "submissions", batch.submissions },
                            { "elapsedMs", batch.elapsedMs },
                            { "submissionRate", batch.intervals.seen() * 1000.0 / batch.elapsedMs },
                            { "atlasUploadBytes", batch.atlasBytes },
                            { "selectedUploadBytes", batch.selectedBytes },
                            { "uniformUploadBytes", batch.uniformBytes },
                            { "decodeStarts", cache.value("decodeStarts").toVariant() },
                            { "decodeActive", cache.value("active").toVariant() },
                            { "decodePending", cache.value("pending").toVariant() },
                            { "requestedSwapInterval",
                              guard->window()->requestedFormat().swapInterval() }
                        };
                        emit guard->diagnosticValuesChanged();
                    },
                    Qt::QueuedConnection);
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
    emit telemetryEnabledChanged();
}
void SceneItem::configureTelemetry()
{
    ++m_callbackRevision;
    const auto epoch = ++m_telemetryEpoch;
    profiled = { };
    if (m_telemetryEnabled && m_renderingActive) {
        struct Mailbox
        {
            QMutex mutex;
            QVector<QJsonObject> samples;
            bool queued = false;
            quint64 coalesced = 0;
        };
        auto mailbox = std::make_shared<Mailbox>();
        QPointer<SceneItem> guard(this);
        profiled = [mailbox, guard, epoch](QJsonObject sample) {
            const QMutexLocker lock(&mailbox->mutex);
            if (mailbox->samples.size() == 128) {
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
                            QVector<QJsonObject> batch;
                            quint64 dropped = 0;
                            {
                                const QMutexLocker lock(&mailbox->mutex);
                                batch.swap(mailbox->samples);
                                dropped = mailbox->coalesced;
                                mailbox->queued = false;
                            }
                            if (guard && epoch == guard->m_telemetryEpoch
                                && guard->m_renderingActive && guard->telemetryEnabled())
                                for (auto &sample : batch) {
                                    sample["coalescedSamples"] = qint64(dropped);
                                    emit guard->frameSubmitted(std::move(sample));
                                }
                        },
                        Qt::QueuedConnection);
        };
    }
}
QVector3D SceneItem::qtProjection(QVector3D world) const
{
    const auto &scene = input().scene;
    const auto clip = scene.projection * scene.view * QVector4D(world, 1);
    if (clip.w() <= 0)
        return { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                 -1 };
    const auto ndc = clip.toVector3DAffine();
    return { float((ndc.x() + 1) * .5 * scene.width), float((1 - ndc.y()) * .5 * scene.height),
             ndc.z() };
}
QJsonObject SceneItem::coverStatistics() const
{
    int detailed = 0, owned = 0;
    qint64 ownedBytes = 0, tiledBytes = 0;
    bool selectedDetailed = false;
    QJsonArray owners;
    for (const auto &cover : m_ownedCovers) {
        if (!cover.pixels)
            continue;
        ++owned;
        owners.append(QJsonObject{
                { "objectId", qint64(cover.objectId) },
                { "albumId", cover.pixels->identity },
                { "lease", qint64(cover.lease) },
                { "resolution", cover.pixels->pixels ? cover.pixels->pixels->resolution : 0 } });
        if (cover.pixels->pixels) {
            ownedBytes += cover.pixels->pixels->image.sizeInBytes();
            detailed += cover.pixels->pixels->resolution == 1024;
            if (cover.objectId == m_activeObject)
                selectedDetailed = cover.pixels->pixels->resolution == 1024;
        }
        for (const auto &upload : cover.uploads)
            tiledBytes += upload.pixels.sizeInBytes();
    }
    return { { "owners", owners },
             { "cache", m_covers.statistics() },
             { "identities", m_poolLeases.size() },
             { "ready", m_readyCovers.size() },
             { "owned", owned },
             { "detailed", detailed },
             { "selectedDetailed", selectedDetailed },
             { "atlasRetainedBytes", m_atlas.retainedBytes() },
             { "ownedBytes", ownedBytes },
             { "tiledBytes", tiledBytes },
             { "atlasSize", QJsonArray{ 5632, 6656 } } };
}
void SceneItem::setRenderingActive(bool active)
{
    if (m_renderingActive == active)
        return;
    m_renderingActive = active;
    m_covers.setSuspended(!active);
    configureTelemetry();
    configureDiagnostics();
    if (active) {
        schedule();
        update();
    } else {
        mouseUngrabEvent();
    }
    emit renderingActiveChanged();
}
void SceneItem::setSceneOccluded(bool value)
{
    if (m_sceneOccluded == value)
        return;
    m_sceneOccluded = value;
    schedule();
    emit openingChanged();
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
            if (!guard)
                return;
            QMetaObject::invokeMethod(
                    guard,
                    [guard, token, submission, uploaded] {
                        if (!guard || !guard->m_renderingActive || !guard->m_preparing
                            || token != guard->m_preparationToken
                            || submission != guard->submission() || !uploaded)
                            return;
                        const auto covers = guard->coverStatistics();
                        if (covers.value("ready").toInt() != covers.value("identities").toInt()
                            || (covers.value("owned").toInt() > 0
                                && !covers.value("selectedDetailed").toBool()))
                            return;
                        emit guard->scenePrepared();
                    },
                    Qt::QueuedConnection);
        };
    }
    schedule();
    emit openingChanged();
}
void SceneItem::setPanelLayerSize(QSize value)
{
    if (m_panelLayerSize == value)
        return;
    m_panelLayerSize = value;
    schedule();
    emit openingChanged();
}
void SceneItem::setOpeningLayerSize(QSize value)
{
    if (m_openingLayerSize == value)
        return;
    m_openingLayerSize = value;
    schedule();
    emit openingChanged();
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
            { hit.value("lane").toDouble(), hit.value("row").toDouble() }, true);
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
        m_mouseMoved |= std::hypot(delta.x(), delta.y()) > 6;
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
        const bool click = !m_mouseMoved && std::hypot(delta.x(), delta.y()) <= 6;
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
    emit inputRouterChanged();
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
        if (!swiped && std::hypot(delta.x(), delta.y()) <= 6)
            selectAt(point.position());
    }
    event->accept();
}
void SceneItem::updatePolish()
{
    if (!m_renderingActive || !m_adapter || !window() || !isVisible() || width() <= 0
        || height() <= 0)
        return;
    if (m_sceneOccluded) {
        if (!input().sceneOccluded) {
            auto next = input();
            next.sceneOccluded = true;
            next.preparationToken = 0;
            if (next.cover.isNull())
                next.cover = m_cover;
            auto target = QSize(fixedColorBufferWidth(), fixedColorBufferHeight());
            if (target.isEmpty())
                target = m_adapter->quality()->targetSize();
            submit(std::move(next), target.expandedTo(QSize(1, 1)));
        }
        return;
    }
    auto &adapter = *m_adapter;
    if (m_poolSelection != adapter.selection()) {
        disconnect(m_poolResetConnection);
        m_poolSelection = adapter.selection();
        m_poolMode = m_poolSelection;
        resetPool();
        if (m_poolSelection)
            m_poolResetConnection = connect(m_poolSelection, &SelectionController::libraryChanged,
                                            this, &SceneItem::refreshPool);
        else {
            m_selectedCover = m_coverPool.acquire(QStringLiteral("demo-square"));
        }
    }
    if (!m_poolMode && m_selectedCover
        && (!m_selectedCover->pixels || m_selectedCover->pixels->resolution != 1024)) {
        const auto identity = QStringLiteral("demo-square");
        const auto path = QStringLiteral(":/assets/demo-covers/square.png");
        if (auto pixels = m_covers.lookup(identity, path, 1024)) {
            if (m_coverPool.upgrade(m_selectedCover, pixels))
                m_cover = pixels->image;
        } else {
            m_covers.request(identity, path, 256);
            m_covers.request(identity, path, 1024);
        }
    }
    auto &quality = *adapter.quality();
    // The item already fills the displayed viewport (music.css: transform: none).
    // Applying the reference-stage scale again undersamples compact windows.
    quality.resize(width(), height(), 1, window()->effectiveDevicePixelRatio(),
                   std::numeric_limits<int>::max(), m_geometryOnly);
    const auto &l = *adapter.lighting();
    const auto settings = quality.values();
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
    in.panelLayerSize = m_panelLayerSize;
    in.navigationDpr = m_inputRouter ? window()->effectiveDevicePixelRatio() : 0;
    in.textLayerSizes = m_textSizes;
    in.textResourceRevision = m_textRevision;
    in.cover = m_cover;
    in.coverPainted = true;
    in.immutableCover = true;
    in.geometryDiagnostic = m_geometryOnly;
    in.fogEnabled = !m_geometryOnly;
    in.productFloor = !m_geometryOnly;
    in.floorColor = l.palette().floor.qt();
    in.shells = !m_geometryOnly;
    in.showPaper = !m_geometryOnly;
    in.captureSamples = 4;
    in.profile = m_telemetryEnabled;
    in.shadows = !m_geometryOnly && quality.shadows() > 0;
    in.shadowSize = qMax(1, quality.shadows());
    in.postprocess = !m_geometryOnly;
    in.aoSamples = m_geometryOnly ? 0 : quality.aoSamples();
    in.aoResolution = float(quality.aoResolution());
    in.bokeh = !m_geometryOnly && settings.value("depthOfField").toDouble() > 0;
    in.smaa = !m_geometryOnly && settings.value("antialias") == "smaa";
#ifdef YALAHOO_RHI_AUDIT
    if (property("_yalahooReviewOpticsOnly").toBool()) {
        in.postprocess = false;
        in.aoSamples = 0;
        in.bokeh = false;
        in.smaa = false;
    }
#endif
    in.focus = float(adapter.focalDepth(adapter.casePosition()));
    in.aperture = float((.0003 + .0005 * adapter.detail())
                        * settings.value("depthOfField").toDouble() / 100);
    in.clarity = adapter.frame().individualMaterials ? 0 : float(adapter.detail());
    in.background = l.backgroundLinear();
    in.keyColor = l.keyColor();
    in.skyColor = l.skyColor();
    in.groundColor = l.groundColor();
    in.lightIntensities = l.lightIntensities();
    in.spotPosition = l.spotPosition();
    in.spotTarget = l.spotTarget();
    in.spotColor = l.spotColor();
    in.spotIntensity = float(l.spotIntensity());
    in.environmentIntensity = float(l.environmentIntensity());
    in.rowSettings = l.rowSettings();
    in.rowColumn = l.rowColumn();
    in.rowDetail = float(l.rowDetail());
    in.rowFill = float(l.rowFill());
    in.lightColumn = l.lightColumn();
    in.printAmbient = float(l.printAmbient());
    in.scatterColor = l.scatterColor();
    in.scatterStrength = float(l.scatterStrength());
    in.areaPosition = l.areaPosition();
    in.areaTarget = l.areaTarget();
    in.areaIntensity = float(l.areaIntensity());
    in.warmth = float(l.warmth());
    in.exposure = float(l.exposure());
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
            && m_coverRevision == m_publishedCoverRevision
            && in.scene.inputRevision == old.scene.inputRevision
            && in.cover.cacheKey() == old.cover.cacheKey()
            && target == QSize(fixedColorBufferWidth(), fixedColorBufferHeight())
            && in.geometryDiagnostic == old.geometryDiagnostic && in.shells == old.shells
            && in.fogEnabled == old.fogEnabled && in.profile == old.profile
            && in.productFloor == old.productFloor && in.floorColor == old.floorColor
            && in.shadows == old.shadows && in.shadowSize == old.shadowSize
            && in.captureSamples == old.captureSamples && in.aoSamples == old.aoSamples
            && in.aoResolution == old.aoResolution && in.postprocess == old.postprocess
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
    m_publishedCallbackRevision = m_callbackRevision;
    m_publishedCoverRevision = m_coverRevision;
    in.entities = { in.scene.caseTransform };
    in.entityIds = { 1 };
    if (m_poolMode)
        preparePool(in);
    submit(std::move(in), target);
}
}
