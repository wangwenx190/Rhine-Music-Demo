#include "Units.h"
#include "RenderData.h"
#include "SelectionLighting.h"
#include "RenderQuality.h"
#include "SceneAdapter.h"
#include "SelectionController.h"
#include <QCoreApplication>
#include "SceneItem.h"
#include <QLineF>
#include <QImageReader>
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

namespace {
// Projected paper demand uses physical DPR; owned covers reserve two rows beyond the fixed base atlas.
constexpr auto MaximumFeedbackSamples = 128;
constexpr auto ClickSlopPixels = 6;
constexpr auto PaperTileExtent = 64;
constexpr auto PaperInteriorExtent = 62;
constexpr auto MaximumResolutionShift = 29;
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
    Q_EMIT textReelsChanged();
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
    const auto covers = guard->coverStatistics();
    if (covers.value(u"ready"_s).toInt() != covers.value(u"identities"_s).toInt()
        || (covers.value(u"owned"_s).toInt() > 0 && !covers.value(u"selectedDetailed"_s).toBool()))
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
void SceneItem::handlePoolCoverReady(const CoverSnapshot &pixels)
{
    if (!m_adapter)
        return;
    if (m_poolSelection && !pixels->error.isEmpty()
        && pixels->fallbackTitle != m_poolSelection->coverSource(pixels->identity).second) {
        schedule();
        return;
    }
    const auto &instances = m_adapter->frame().instances;
    const auto activeObject = instances.isEmpty() ? 0 : instances.last().objectId;
    if (pixels->resolution == renderData::CoverTilePixels) {
        const auto lease = m_poolLeases.constFind(pixels->identity);
        if (lease != m_poolLeases.cend() && m_atlas.publish(*lease, pixels))
            m_readyCovers.insert(pixels->identity, pixels->error.isEmpty());
    }
    for (auto &owned : m_ownedCovers)
        if (owned.objectId == activeObject && owned.pixels
            && owned.pixels->identity == pixels->identity) {
            if (pixels->resolution >= m_detailResolution)
                owned.highResolutionFailed = !pixels->error.isEmpty();
            m_coverPool.upgrade(owned.pixels, pixels);
        }
    ++m_coverRevision;
    schedule();
    return;
}
void SceneItem::handleCoverReady(const CoverSnapshot &pixels)
{
    if (m_poolMode) {
        handlePoolCoverReady(pixels);
        return;
    }
    if (m_coverPool.upgrade(m_selectedCover, pixels)) {
        m_cover = pixels->image;
        schedule();
    }
}
SceneItem::SceneItem(QQuickItem *parent)
    : RhiItem(parent),
      m_cover(renderData::CoverTilePixels, renderData::CoverTilePixels,
              QImage::Format_RGBA8888_Premultiplied)
{
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            &SceneItem::configureDiagnostics);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
    setFlag(QQuickItem::ItemIsFocusScope, true);
    setFocus(true);
    m_cover.fill(Qt::transparent);
    m_selectedCover = m_coverPool.acquire(u"demo-square"_s);
    connect(&m_covers, &CoverCache::ready, this, [this](const CoverSnapshot &pixels) {
        return handleCoverReady(pixels);
    });
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
        connect(adapter, &SceneAdapter::frameChanged, this, &SceneItem::schedule);
        connect(adapter->lighting(), &SelectionLighting::changed, this, &SceneItem::schedule);
        connect(adapter->quality(), &RenderQuality::changed, this, &SceneItem::schedule);
        connect(adapter, &SceneAdapter::selectionControllerChanged, this, &SceneItem::schedule);
    }
    schedule();
    Q_EMIT adapterChanged();
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
    m_ownedSide = renderData::CoverTilePixels;
    m_detailResolution = renderData::CoverTilePixels;
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
        const auto next = m_poolSelection->coverSource(it.key());
        if (it.value() == next) {
            ++it;
            continue;
        }
        const bool ownedPlaceholder = std::any_of(
                m_ownedCovers.cbegin(), m_ownedCovers.cend(), [&it](const OwnedCover &owned) {
                    return owned.pixels && owned.pixels->identity == it.key()
                            && owned.pixels->pixels && !owned.pixels->pixels->error.isEmpty();
                });
        if (!next.first.isEmpty() && next.first == it->first && m_readyCovers.value(it.key(), false)
            && !ownedPlaceholder) {
            it.value() = next;
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
// Resolve leases by stable identity and content generation. Detailed tiles share an owned image
// snapshot, so a GUI publication never borrows pixels from a temporary cache lookup.
void SceneItem::ensureBaseCover(const QString &id, const QPair<QString, QString> &source,
                                const AtlasLease &lease)
{
    if (m_readyCovers.contains(id))
        return;

    if (auto pixels =
                m_covers.lookup(id, source.first, renderData::CoverTilePixels, source.second)) {
        if (m_atlas.publish(lease, pixels))
            m_readyCovers.insert(id, pixels->error.isEmpty());
    } else if (m_covers.hasPendingCapacity())
        m_covers.request(id, source.first, renderData::CoverTilePixels, source.second);
}
std::pair<QRect, quint64> SceneItem::prepareOwnedCover(Input &in, const ArchiveInstance &instance,
                                                       const QPair<QString, QString> &source,
                                                       QRect tile, quint64 coverLease)
{
    auto *it = acquireOwnedCover(instance, source);
    if (!it || !it->pixels)
        return { tile, coverLease };
    requestDetailedCover(*it, instance.albumId, source);
    const auto pixels = it->pixels->pixels;
    if (!pixels || pixels->resolution <= renderData::CoverTilePixels)
        return { tile, coverLease };
    const int block = int(it - m_ownedCovers.data());
    const int top = ((yalahoo::rhi::sceneAtlas::BaseWidthPixels + m_ownedSide - 1) / m_ownedSide)
            * m_ownedSide;
    tile = { (block % 2) * m_ownedSide, top + (block / 2) * m_ownedSide, pixels->resolution,
             pixels->resolution };
    coverLease = it->lease;
    if (it->tiledPixels != pixels || it->tiledRegion != tile) {
        it->uploads.clear();
        it->uploads.reserve(pixels->tiles.size());
        for (const auto &part : pixels->tiles) {
            const int slot = ((tile.y() + part.origin.y()) / renderData::CoverTilePixels)
                            * (m_atlasSize.width() / renderData::CoverTilePixels)
                    + (tile.x() + part.origin.x()) / renderData::CoverTilePixels;
            it->uploads.append({ m_atlas.generation(), it->lease, ++m_ownedSerial, slot, part.image,
                                 true, part.mipmaps });
        }
        it->tiledPixels = pixels;
        it->tiledRegion = tile;
    }
    in.atlasUploads += it->uploads;
    return { tile, coverLease };
}
SceneItem::OwnedCover *SceneItem::acquireOwnedCover(const ArchiveInstance &instance,
                                                    const QPair<QString, QString> &source)
{
    auto it =
            std::find_if(m_ownedCovers.begin(), m_ownedCovers.end(), [&](const OwnedCover &cover) {
                return cover.objectId == instance.objectId;
            });
    if (it != m_ownedCovers.end())
        return &*it;
    it = std::find_if(m_ownedCovers.begin(), m_ownedCovers.end(), [](const OwnedCover &cover) {
        return cover.objectId == 0;
    });
    if (it == m_ownedCovers.end())
        return nullptr;
    it->objectId = instance.objectId;
    it->lease = ++m_ownedSerial;
    it->pixels = m_coverPool.acquire(instance.albumId,
                                     m_covers.lookup(instance.albumId, source.first,
                                                     renderData::CoverTilePixels, source.second),
                                     m_covers.generation());
    return &*it;
}
void SceneItem::requestDetailedCover(OwnedCover &owned, const QString &id,
                                     const QPair<QString, QString> &source)
{
    if (owned.objectId != m_activeObject
        || (owned.highResolutionFailed
            && (!owned.pixels->pixels || owned.pixels->pixels->error.isEmpty()))
        || (owned.pixels->pixels && owned.pixels->pixels->resolution >= m_detailResolution))
        return;
    if (auto pixels = m_covers.lookup(id, source.first, m_detailResolution, source.second)) {
        owned.highResolutionFailed = !pixels->error.isEmpty();
        m_coverPool.upgrade(owned.pixels, pixels);
        return;
    }
    if (m_covers.hasPendingCapacity())
        m_covers.request(id, source.first, m_detailResolution, source.second);
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
    wanted.reserve(count);
    ownedIds.reserve(SelectedCoverPool::capacity);
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
        const auto entry = m_poolLeases.tryEmplace(id);
        if (entry.inserted) {
            *entry.iterator = m_atlas.acquire(id, m_covers.generation());
            m_poolSources.insert(id, source);
        }
        const auto lease = *entry.iterator;
        auto tile = m_atlas.tile(lease);
        auto coverLease = lease.lease;
        in.fallbackCoverTiles[index] = tile;
        in.fallbackCoverLeases[index] = coverLease;
        ensureBaseCover(id, source, lease);

        if (instance.owned) {
            const auto owned = prepareOwnedCover(in, instance, source, tile, coverLease);
            tile = owned.first;
            coverLease = owned.second;
        }

        in.entities[index] = instance.transform;
        in.entityIds[index] = instance.objectId;
        in.coverTiles[index] = tile;
        in.coverLeases[index] = coverLease;
    }
    in.atlasSize = m_atlasSize;
    in.atlasGeneration = m_atlas.generation();
    in.poolFrame = true;
    // Immutable replay handles render-context recreation and superseded GUI submissions;
    // the render queue rejects known revisions before conversion or upload.
    in.atlasUploads += m_atlas.replay(0, m_atlasSize.width() / renderData::CoverTilePixels);
    in.immutableCover = true;
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
    const auto cache = guard->m_covers.statistics();
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
                { u"objectId"_s, qint64(cover.objectId) },
                { u"albumId"_s, cover.pixels->identity },
                { u"lease"_s, qint64(cover.lease) },
                { u"resolution"_s, cover.pixels->pixels ? cover.pixels->pixels->resolution : 0 } });
        if (cover.pixels->pixels) {
            ownedBytes += cover.pixels->pixels->byteSize();
            detailed += cover.pixels->pixels->resolution >= m_detailResolution;
            if (cover.objectId == m_activeObject)
                selectedDetailed = cover.pixels->pixels->resolution >= m_detailResolution;
        }
        for (const auto &upload : cover.uploads)
            tiledBytes += AtlasUploadQueue::payloadBytes(upload);
    }
    return { { u"owners"_s, owners },
             { u"cache"_s, m_covers.statistics() },
             { u"identities"_s, m_poolLeases.size() },
             { u"ready"_s, m_readyCovers.size() },
             { u"owned"_s, owned },
             { u"detailed"_s, detailed },
             { u"selectedDetailed"_s, selectedDetailed },
             { u"atlasRetainedBytes"_s, m_atlas.retainedBytes() },
             { u"ownedBytes"_s, ownedBytes },
             { u"tiledBytes"_s, tiledBytes },
             { u"atlasSize"_s, QJsonArray{ m_atlasSize.width(), m_atlasSize.height() } } };
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
            next.cover = m_cover;
        auto target = QSize(fixedColorBufferWidth(), fixedColorBufferHeight());
        if (target.isEmpty())
            target = m_adapter->quality()->targetSize();
        submit(std::move(next), target.expandedTo(QSize(1, 1)));
    }
    return;
}
void SceneItem::requestDemoCover()
{
    const auto identity = u"demo-square"_s;
    const auto path = u":/assets/demo-covers/square.png"_s;
    if (auto pixels = m_covers.lookup(identity, path, m_detailResolution)) {
        if (m_coverPool.upgrade(m_selectedCover, pixels))
            m_cover = pixels->image;
    } else {
        m_covers.request(identity, path, renderData::CoverTilePixels);
        m_covers.request(identity, path, m_detailResolution);
    }
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
    for (const auto edge : { std::pair{ 0, 1 }, { 0, 2 }, { 1, 3 }, { 2, 3 } })
        demand = std::max(demand,
                          QLineF(points[edge.first], points[edge.second]).length()
                                  * window()->effectiveDevicePixelRatio() * PaperTileExtent
                                  / PaperInteriorExtent);

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
    if (m_poolSelection != adapter.selection()) {
        disconnect(m_poolResetConnection);
        m_poolSelection = adapter.selection();
        m_poolMode = m_poolSelection;
        resetPool();
        if (m_poolSelection)
            m_poolResetConnection = connect(m_poolSelection, &SelectionController::libraryChanged,
                                            this, &SceneItem::refreshPool);
        else {
            m_selectedCover = m_coverPool.acquire(u"demo-square"_s);
        }
    }
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
    m_detailResolution = renderData::CoverTilePixels;
    while (m_detailResolution < demand && m_detailResolution < (1 << MaximumResolutionShift))
        m_detailResolution *= 2;
    const auto allocationLimit = QImageReader::allocationLimit();
    if (allocationLimit > 0
        && qint64(m_detailResolution) * m_detailResolution * yalahoo::units::RgbaBytesPerPixel
                > qint64(allocationLimit) * yalahoo::units::BytesPerKiB
                        * yalahoo::units::BytesPerKiB)
        throw std::runtime_error("Projected cover exceeds the image allocation capability");
    m_ownedSide = std::max(m_ownedSide, m_detailResolution);
    for (const auto &owned : m_ownedCovers)
        if (owned.pixels && owned.pixels->pixels)
            m_ownedSide = std::max(m_ownedSide, owned.pixels->pixels->resolution);
    const int top = ((yalahoo::rhi::sceneAtlas::BaseWidthPixels + m_ownedSide - 1) / m_ownedSide)
            * m_ownedSide;
    const QSize nextAtlasSize(std::max(yalahoo::rhi::sceneAtlas::BaseWidthPixels, 2 * m_ownedSide),
                              top + 2 * m_ownedSide);
    if (nextAtlasSize != m_atlasSize) {
        m_atlasSize = nextAtlasSize;
        m_atlas.advanceGeneration();
        for (auto &lease : m_poolLeases)
            lease.generation = m_atlas.generation();
        for (auto &owned : m_ownedCovers)
            owned.tiledPixels.reset();
        ++m_coverRevision;
    }
    if (!m_poolMode && m_selectedCover
        && (!m_selectedCover->pixels || m_selectedCover->pixels->resolution < m_detailResolution)) {
        requestDemoCover();
    }
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
    in.cover = m_cover;
    if (m_selectedCover && m_selectedCover->pixels)
        in.coverMipmaps = m_selectedCover->pixels->mipmaps;
    in.coverPainted = true;
    in.filterCovers = m_poolMode || !in.coverMipmaps.isEmpty();
    in.immutableCover = true;
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
    m_publishedCallbackRevision = m_callbackRevision;
    m_publishedCoverRevision = m_coverRevision;
    in.entities = { in.scene.caseTransform };
    in.entityIds = { 1 };
    if (m_poolMode)
        preparePool(in);
    submit(std::move(in), target);
} catch (const std::runtime_error &error) {
    m_errorText = QString::fromUtf8(error.what());
    setRenderingActive(false);
    Q_EMIT errorTextChanged();
}
}
