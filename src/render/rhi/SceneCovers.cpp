#include "SceneCovers.h"
#include "SceneCoversState_p.h"
#include <utility>
#include "RhiItem.h"
#include "RenderData.h"
#include "SelectionController.h"
#include "AtlasAllocator.h"
#include "Units.h"
#include <QImageReader>
#include <QJsonArray>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
namespace {

constexpr auto MaximumResolutionShift = 29;
}

SceneCovers::~SceneCovers() = default;
bool SceneCovers::poolMode() const
{
    return d->m_poolMode;
}
quint64 SceneCovers::revision() const
{
    return d->m_coverRevision;
}
QImage SceneCovers::image() const
{
    return d->m_cover;
}
QJsonObject SceneCovers::cacheStatistics() const
{
    return d->m_covers.statistics();
}
SceneCovers::SceneCovers(QObject *parent) : QObject(parent), d(std::make_unique<Private>(this))
{
    d->m_cover = QImage(renderData::CoverTilePixels, renderData::CoverTilePixels,
                        QImage::Format_RGBA8888_Premultiplied);
    d->m_cover.fill(Qt::transparent);
    d->m_selectedCover = d->m_coverPool.acquire(u"demo-square"_s, { }, d->m_covers.generation());
    connect(&d->m_covers, &CoverCache::ready, this,
            [this](const std::shared_ptr<const CoverPixels> &pixels) {
                d->handleCoverReady(pixels);
            });
}
void SceneCovers::setSelection(SelectionController *selection)
{
    if (d->m_poolSelection == selection && d->m_poolMode == bool(selection))
        return;
    disconnect(d->m_poolResetConnection);
    d->m_poolSelection = selection;
    d->m_poolMode = selection;
    d->resetPool();
    if (selection)
        d->m_poolResetConnection =
                connect(selection, &SelectionController::libraryChanged, this, [this] {
                    d->refreshPool();
                });
    else
        d->m_selectedCover =
                d->m_coverPool.acquire(u"demo-square"_s, { }, d->m_covers.generation());
}
void SceneCovers::observeFrame(const SceneFrame &frame)
{
    d->m_activeObject = frame.instances.isEmpty() ? 0 : frame.instances.last().objectId;
}
void SceneCovers::setSuspended(bool suspended)
{
    d->m_covers.setSuspended(suspended);
}
void SceneCovers::setDemand(double demand)
{
    if (!std::isfinite(demand) || demand <= 0 || demand > (1 << MaximumResolutionShift))
        throw std::runtime_error("Invalid projected cover demand");
    int resolution = renderData::CoverTilePixels;
    while (resolution < demand && resolution < (1 << MaximumResolutionShift))
        resolution *= 2;
    const auto allocationLimit = QImageReader::allocationLimit();
    if (allocationLimit > 0
        && qint64(resolution) * resolution * yalahoo::units::RgbaBytesPerPixel
                > qint64(allocationLimit) * yalahoo::units::BytesPerKiB
                        * yalahoo::units::BytesPerKiB)
        throw std::runtime_error("Projected cover exceeds the image allocation capability");
    d->m_detailResolution = resolution;
    d->m_ownedSide = std::max(d->m_ownedSide, d->m_detailResolution);
    for (const auto &owned : d->m_ownedCovers)
        if (owned.pixels && owned.pixels->pixels)
            d->m_ownedSide = std::max(d->m_ownedSide, owned.pixels->pixels->resolution);
    const int top =
            ((yalahoo::rhi::sceneAtlas::BaseWidthPixels + d->m_ownedSide - 1) / d->m_ownedSide)
            * d->m_ownedSide;
    const QSize nextAtlasSize(
            std::max(yalahoo::rhi::sceneAtlas::BaseWidthPixels, 2 * d->m_ownedSide),
            top + 2 * d->m_ownedSide);
    if (nextAtlasSize != d->m_atlasSize) {
        d->m_atlasSize = nextAtlasSize;
        d->m_atlas.advanceGeneration();
        for (auto &cover : d->m_poolLeases)
            cover.lease.generation = d->m_atlas.generation();
        for (auto &owned : d->m_ownedCovers)
            owned.tiledPixels.reset();
        ++d->m_coverRevision;
    }
    if (!d->m_poolMode && d->m_selectedCover
        && (!d->m_selectedCover->pixels
            || d->m_selectedCover->pixels->resolution < d->m_detailResolution)) {
        d->requestDemoCover();
    }
}
void SceneCovers::applyImage(Input &in) const
{
    in.cover = d->m_cover;
    if (d->m_selectedCover && d->m_selectedCover->pixels)
        in.coverMipmaps = d->m_selectedCover->pixels->mipmaps;
    in.coverPainted = true;
    in.filterCovers = d->m_poolMode || !in.coverMipmaps.isEmpty();
    in.immutableCover = true;
}
void SceneCovers::prepare(Input &in, const Input &previous)
{
    if (!d->m_poolSelection)
        return;
    d->preparePool(in, previous);
    d->m_publishedCoverRevision = d->m_coverRevision;
    d->m_publishedDetailResolution = d->m_detailResolution;
}
bool SceneCovers::prepared() const
{
    if (d->m_readyCovers.size() != d->m_poolLeases.size())
        return false;
    const bool hasOwned = std::any_of(d->m_ownedCovers.cbegin(), d->m_ownedCovers.cend(),
                                      [](const Private::OwnedCover &cover) {
                                          return bool(cover.pixels);
                                      });
    // A failed upgrade is terminal for this demand; retained artwork is still usable.
    const bool selectedUsable =
            std::any_of(d->m_ownedCovers.cbegin(), d->m_ownedCovers.cend(),
                        [this](const Private::OwnedCover &cover) {
                            return cover.objectId == d->m_activeObject && cover.pixels
                                    && cover.pixels->pixels
                                    && (cover.pixels->pixels->resolution >= d->m_detailResolution
                                        || cover.failedResolution == d->m_detailResolution);
                        });
    if (hasOwned && !selectedUsable)
        return false;
    return true;
}
void SceneCovers::Private::handlePoolCoverReady(const CoverSnapshot &pixels)
{
    if (m_poolSelection && !pixels->error.isEmpty()
        && pixels->fallbackTitle
                != m_poolSelection->coverSource(pixels->identity).placeholderTitle) {
        Q_EMIT q->changed();
        return;
    }
    if (pixels->resolution == renderData::CoverTilePixels) {
        const auto lease = m_poolLeases.constFind(pixels->identity);
        if (lease != m_poolLeases.cend() && m_atlas.publish(lease->lease, pixels))
            m_readyCovers.insert(pixels->identity, pixels->error.isEmpty());
    }
    for (auto &owned : m_ownedCovers)
        if (owned.objectId == m_activeObject && owned.pixels
            && owned.pixels->identity == pixels->identity) {
            if (pixels->resolution == m_detailResolution)
                owned.failedResolution = pixels->error.isEmpty() ? 0 : pixels->resolution;
            m_coverPool.upgrade(owned.pixels, pixels);
        }
    ++m_coverRevision;
    Q_EMIT q->changed();
    return;
}
void SceneCovers::Private::handleCoverReady(const CoverSnapshot &pixels)
{
    if (m_poolMode) {
        handlePoolCoverReady(pixels);
        return;
    }
    if (m_coverPool.upgrade(m_selectedCover, pixels)) {
        m_cover = pixels->image;
        Q_EMIT q->changed();
    }
}
void SceneCovers::Private::resetPool()
{
    m_covers.reset();
    m_coverPool.reset();
    m_selectedCover.reset();
    m_poolLeases.clear();
    m_readyCovers.clear();
    m_atlas.reset();
    m_ownedCovers = { };
    m_ownedSide = renderData::CoverTilePixels;
    m_detailResolution = renderData::CoverTilePixels;
    m_activeObject = 0;
    ++m_coverRevision;
    Q_EMIT q->changed();
}
void SceneCovers::Private::refreshPool()
{
    if (!m_poolSelection)
        return;
    // Only the bounded visible pool is reconciled; content-addressed native paths
    // carry revisions, while missing artwork also depends on its fallback title.
    for (auto it = m_poolLeases.begin(); it != m_poolLeases.end();) {
        const auto next = m_poolSelection->coverSource(it.key());
        if (it->source == next) {
            ++it;
            continue;
        }
        const bool ownedPlaceholder = std::any_of(
                m_ownedCovers.cbegin(), m_ownedCovers.cend(), [&it](const OwnedCover &owned) {
                    return owned.pixels && owned.pixels->identity == it.key()
                            && owned.pixels->pixels && !owned.pixels->pixels->error.isEmpty();
                });
        if (!next.path.isEmpty() && next.path == it->source.path
            && m_readyCovers.value(it.key(), false) && !ownedPlaceholder) {
            it->source = next;
            ++it;
            continue;
        }
        const auto id = it.key();
        m_covers.invalidate(id);
        m_atlas.release(it->lease);
        m_readyCovers.remove(id);
        for (auto &owned : m_ownedCovers)
            if (owned.pixels && owned.pixels->identity == id)
                owned = { };
        it = m_poolLeases.erase(it);
    }
    ++m_coverRevision;
    Q_EMIT q->changed();
}
// Resolve leases by stable identity and content generation. Detailed tiles share an owned image
// snapshot, so a GUI publication never borrows pixels from a temporary cache lookup.
void SceneCovers::Private::ensureBaseCover(const QString &id, const CoverSource &source,
                                           const AtlasLease &lease)
{
    if (m_readyCovers.contains(id))
        return;

    if (auto pixels = m_covers.lookup(id, source.path, renderData::CoverTilePixels,
                                      source.placeholderTitle)) {
        if (m_atlas.publish(lease, pixels))
            m_readyCovers.insert(id, pixels->error.isEmpty());
    } else if (m_covers.hasPendingCapacity())
        m_covers.request(id, source.path, renderData::CoverTilePixels, source.placeholderTitle);
}
SceneCovers::Private::CoverRegion
SceneCovers::Private::prepareOwnedCover(Input &in, const ArchiveInstance &instance,
                                        const CoverSource &source, QRect tile, quint64 coverLease)
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
SceneCovers::Private::OwnedCover *
SceneCovers::Private::acquireOwnedCover(const ArchiveInstance &instance, const CoverSource &source)
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
                                     m_covers.lookup(instance.albumId, source.path,
                                                     renderData::CoverTilePixels,
                                                     source.placeholderTitle),
                                     m_covers.generation());
    return &*it;
}
void SceneCovers::Private::requestDetailedCover(Private::OwnedCover &owned, const QString &id,
                                                const CoverSource &source)
{
    if (owned.objectId != m_activeObject || owned.failedResolution == m_detailResolution
        || (owned.pixels->pixels && owned.pixels->pixels->resolution >= m_detailResolution))
        return;
    if (auto pixels =
                m_covers.lookup(id, source.path, m_detailResolution, source.placeholderTitle)) {
        owned.failedResolution = pixels->error.isEmpty() ? 0 : pixels->resolution;
        m_coverPool.upgrade(owned.pixels, pixels);
        return;
    }
    if (m_covers.hasPendingCapacity())
        m_covers.request(id, source.path, m_detailResolution, source.placeholderTitle);
}
bool SceneCovers::Private::reusePool(Input &in, const Input &previous) const
{
    const auto &instances = std::as_const(in.scene.instances);
    if (!previous.poolFrame || previous.atlasGeneration != m_atlas.generation()
        || m_coverRevision != m_publishedCoverRevision
        || m_detailResolution != m_publishedDetailResolution
        || m_readyCovers.size() != m_poolLeases.size()
        || !std::equal(instances.cbegin(), instances.cend(), previous.entityIds.cbegin(),
                       previous.entityIds.cend(), [](const ArchiveInstance &instance, quint64 id) {
                           return instance.objectId == id;
                       }))
        return false;
    // Unready work must keep visiting admission until it has a usable selected snapshot.
    if (!instances.isEmpty() && instances.last().owned) {
        const auto selected = std::find_if(m_ownedCovers.cbegin(), m_ownedCovers.cend(),
                                           [this](const OwnedCover &cover) {
                                               return cover.objectId == m_activeObject;
                                           });
        if (selected == m_ownedCovers.cend() || !selected->pixels || !selected->pixels->pixels
            || (selected->pixels->pixels->resolution < m_detailResolution
                && selected->failedResolution != m_detailResolution))
            return false;
    }
    if (instances.isSharedWith(previous.scene.instances)) {
        in.entities = previous.entities;
    } else {
        in.entities.resize(instances.size());
        for (qsizetype i = 0; i < in.entities.size(); ++i)
            in.entities[i] = instances[i].transform;
    }
    // Geometry/camera animation changes no cover ownership. Retain the immutable descriptor
    // storage until identity order, content, atlas layout or projected demand changes.
    in.entityIds = previous.entityIds;
    in.coverTiles = previous.coverTiles;
    in.coverLeases = previous.coverLeases;
    in.fallbackCoverTiles = previous.fallbackCoverTiles;
    in.fallbackCoverLeases = previous.fallbackCoverLeases;
    in.atlasUploads = previous.atlasUploads;
    in.atlasSize = previous.atlasSize;
    in.atlasGeneration = previous.atlasGeneration;
    in.poolFrame = true;
    return true;
}
void SceneCovers::Private::preparePool(Input &in, const Input &previous)
{
    auto *selection = m_poolSelection.data();
    const auto &instances = std::as_const(in.scene.instances);
    m_activeObject = instances.isEmpty() ? 0 : instances.last().objectId;
    if (reusePool(in, previous))
        return;
    in.entities.clear();
    in.entityIds.clear();
    std::array<quint64, SelectedCoverPool::capacity> ownedIds;
    qsizetype ownedCount = 0;
    const int count = int(instances.size());
    for (auto &cover : m_poolLeases)
        cover.wanted = false;
    in.entities.resize(count);
    in.entityIds.resize(count);
    in.coverTiles.resize(count);
    in.coverLeases.resize(count);
    in.fallbackCoverTiles.resize(count);
    in.fallbackCoverLeases.resize(count);
    for (const auto &instance : instances) {
        if (auto cover = m_poolLeases.find(instance.albumId); cover != m_poolLeases.end())
            cover->wanted = true;
        if (instance.owned) {
            if (ownedCount >= SelectedCoverPool::capacity)
                throw std::runtime_error("Owned cover capacity exceeded");
            ownedIds[ownedCount++] = instance.objectId;
        }
    }
    for (auto it = m_poolLeases.begin(); it != m_poolLeases.end();) {
        if (!it->wanted) {
            m_covers.cancel(it.key());
            m_atlas.release(it->lease);
            m_readyCovers.remove(it.key());
            it = m_poolLeases.erase(it);
        } else
            ++it;
    }
    for (auto &owned : m_ownedCovers) {
        if (!owned.objectId
            || std::find(ownedIds.begin(), ownedIds.begin() + ownedCount, owned.objectId)
                    != ownedIds.begin() + ownedCount)
            continue;
        if (instances.isEmpty()) {
            owned = { };
            continue;
        }
        const auto &active = instances.last();
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
        const auto &instance = instances[index];
        const auto &id = instance.albumId;
        const auto entry = m_poolLeases.tryEmplace(id);
        if (entry.inserted) {
            entry.iterator->lease = m_atlas.acquire(id, m_covers.generation());
            entry.iterator->source = selection->coverSource(id);
        }
        const auto &source = entry.iterator->source;
        const auto &lease = entry.iterator->lease;
        auto tile = m_atlas.tile(lease);
        auto coverLease = lease.lease;
        in.fallbackCoverTiles[index] = tile;
        in.fallbackCoverLeases[index] = coverLease;
        ensureBaseCover(id, source, lease);

        if (instance.owned) {
            const auto owned = prepareOwnedCover(in, instance, source, tile, coverLease);
            tile = owned.tile;
            coverLease = owned.lease;
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
void SceneCovers::Private::requestDemoCover()
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
}
