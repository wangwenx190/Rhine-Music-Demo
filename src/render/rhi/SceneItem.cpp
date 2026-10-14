#include "SceneItem.h"
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
    m_covers.request(QStringLiteral("demo-square"),
                     QStringLiteral(":/assets/demo-covers/square.png"), 256);
    m_covers.request(QStringLiteral("demo-square"),
                     QStringLiteral(":/assets/demo-covers/square.png"), 1024);
    connect(this, &QQuickItem::widthChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::heightChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::visibleChanged, this, &SceneItem::schedule);
    connect(this, &RhiItem::renderFailed, this, [this](const QString &message) {
        m_errorText = message;
        emit errorTextChanged();
    });
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
        disconnect(m_visibilityConnection);
        disconnect(m_activeConnection);
        mouseUngrabEvent();
        if (window) {
            connect(window, &QWindow::screenChanged, this, &SceneItem::schedule,
                    Qt::UniqueConnection);
            m_visibilityConnection =
                    connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
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
            m_activeConnection = connect(window, &QWindow::activeChanged, this, [this, window] {
                if (!window->isActive())
                    mouseUngrabEvent();
            });
        }
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
    m_readyCovers.clear();
    m_atlas.reset();
    m_ownedCovers = { };
    m_activeObject = 0;
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
            it = m_poolLeases.erase(it);
        } else
            ++it;
    }
    for (auto &owned : m_ownedCovers)
        if (owned.objectId && !ownedIds.contains(owned.objectId))
            owned = { };
    // Source descriptors are indexed at library/sort boundaries, outside the frame path.
    for (int offset = 0; offset < count; ++offset) {
        // Admit the selected cover first, without changing the instance/sort ordering.
        const int index = offset == 0 ? count - 1 : offset - 1;
        const auto &instance = in.scene.instances[index];
        const auto &id = instance.albumId;
        const auto source = selection->coverSource(id);
        if (!m_poolLeases.contains(id))
            m_poolLeases.insert(id, m_atlas.acquire(id, m_covers.generation()));
        const auto lease = m_poolLeases.value(id);
        auto tile = m_atlas.tile(lease);
        auto coverLease = lease.lease;
        in.fallbackCoverTiles[index] = tile;
        in.fallbackCoverLeases[index] = coverLease;
        if (!m_readyCovers.contains(id)) {
            if (auto pixels = m_covers.lookup(id, source.first, 256)) {
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
                    it->pixels = m_coverPool.acquire(id, m_covers.lookup(id, source.first, 256),
                                                     m_covers.generation());
                }
            }
            if (it != m_ownedCovers.end() && it->pixels) {
                if (instance.objectId == m_activeObject
                    && (!it->pixels->pixels || it->pixels->pixels->resolution != 1024)) {
                    if (auto pixels = m_covers.lookup(id, source.first, 1024))
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
void SceneItem::setTelemetryEnabled(bool enabled)
{
    if (m_telemetryEnabled == enabled)
        return;
    m_telemetryEnabled = enabled;
    profiled = { };
    if (enabled) {
        struct Mailbox
        {
            QMutex mutex;
            QVector<QJsonObject> samples;
            bool queued = false;
            quint64 coalesced = 0;
        };
        auto mailbox = std::make_shared<Mailbox>();
        QPointer<SceneItem> guard(this);
        profiled = [mailbox, guard](QJsonObject sample) {
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
                        [mailbox, guard] {
                            QVector<QJsonObject> batch;
                            quint64 dropped = 0;
                            {
                                const QMutexLocker lock(&mailbox->mutex);
                                batch.swap(mailbox->samples);
                                dropped = mailbox->coalesced;
                                mailbox->queued = false;
                            }
                            if (guard && guard->telemetryEnabled())
                                for (auto &sample : batch) {
                                    sample["coalescedSamples"] = qint64(dropped);
                                    emit guard->frameSubmitted(std::move(sample));
                                }
                        },
                        Qt::QueuedConnection);
        };
    }
    schedule();
    emit telemetryEnabledChanged();
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
void SceneItem::schedule()
{
    // Qt coalesces polish: camera, lighting and quality notifications publish once per frame.
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
    if (selected)
        m_adapter->recordNavigationCommit(m_adapter->selection()->revision());
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
    if (!m_adapter || !window() || !isVisible() || width() <= 0 || height() <= 0)
        return;
    auto &adapter = *m_adapter;
    if (m_poolSelection != adapter.selection()) {
        disconnect(m_poolResetConnection);
        m_poolSelection = adapter.selection();
        m_poolMode = m_poolSelection;
        resetPool();
        if (m_poolSelection)
            m_poolResetConnection = connect(m_poolSelection, &SelectionController::libraryChanged,
                                            this, &SceneItem::resetPool);
        else {
            m_selectedCover = m_coverPool.acquire(QStringLiteral("demo-square"));
            m_covers.request(QStringLiteral("demo-square"),
                             QStringLiteral(":/assets/demo-covers/square.png"), 1024);
        }
    }
    auto &quality = *adapter.quality();
    quality.resize(width(), height(), m_geometryOnly ? 1 : qMin(width() / 1920, height() / 1080),
                   window()->effectiveDevicePixelRatio(), std::numeric_limits<int>::max(),
                   m_geometryOnly);
    const auto &l = *adapter.lighting();
    const auto settings = quality.values();
    Input in;
    // Review capture tokens survive a coalesced camera/cover polish before synchronization.
    in.request = input().request;
    in.captureAtlas = input().captureAtlas;
    in.scene = adapter.frame();
    in.navigationDpr = m_inputRouter ? window()->effectiveDevicePixelRatio() : 0;
    in.textLayerSizes = m_textSizes;
    in.textResourceRevision = m_textRevision;
    in.cover = m_cover;
    in.coverPainted = true;
    in.immutableCover = true;
    in.geometryDiagnostic = m_geometryOnly;
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
    const bool unchanged = in.scene.revision == old.scene.revision
            && in.scene.navigationRevision == old.scene.navigationRevision
            && in.scene.navigationCommittedNs == old.scene.navigationCommittedNs
            && in.navigationDpr == old.navigationDpr
            && in.textResourceRevision == old.textResourceRevision
            && m_coverRevision == m_publishedCoverRevision
            && in.scene.inputRevision == old.scene.inputRevision
            && in.cover.cacheKey() == old.cover.cacheKey()
            && target == QSize(fixedColorBufferWidth(), fixedColorBufferHeight())
            && in.geometryDiagnostic == old.geometryDiagnostic && in.shells == old.shells
            && in.profile == old.profile && in.shadows == old.shadows
            && in.shadowSize == old.shadowSize && in.captureSamples == old.captureSamples
            && in.aoSamples == old.aoSamples && in.aoResolution == old.aoResolution
            && in.postprocess == old.postprocess && in.bokeh == old.bokeh && in.smaa == old.smaa
            && in.focus == old.focus && in.aperture == old.aperture && in.clarity == old.clarity
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
    m_publishedCoverRevision = m_coverRevision;
    in.entities = { in.scene.caseTransform };
    in.entityIds = { 1 };
    if (m_poolMode)
        preparePool(in);
    submit(std::move(in), target);
}
}
