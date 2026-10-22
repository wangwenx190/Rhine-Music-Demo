#include "SceneCovers.h"
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
namespace sceneAtlas {
// The fixed archive plus owned detail tiles determine the initial atlas allocation.
inline constexpr auto MaximumAtlasSide = 8192;
inline constexpr auto BaseColumns = 24;
inline constexpr auto OwnedCoverCount = 4;
inline constexpr auto BaseWidthPixels = 6144;
inline constexpr auto InitialHeightPixels = 6656;
}

constexpr auto MaximumResolutionShift = 29;
}
struct SceneCovers::OwnedCover final
{
    quint64 objectId = 0, lease = 0;
    std::shared_ptr<SelectedCoverLease> pixels;
    CoverSnapshot tiledPixels;
    QRect tiledRegion;
    QVector<AtlasUpload> uploads;
    bool highResolutionFailed = false;
};
struct SceneCovers::CoverRegion final
{
    QRect tile;
    quint64 lease;
};
struct SceneCovers::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
    AtlasAllocator m_atlas{ sceneAtlas::MaximumAtlasSide,
                            yalahoo::rhi::renderData::MaximumCaseEntities,
                            sceneAtlas::BaseColumns };
    struct PoolCover final
    {
        AtlasLease lease;
        CoverSource source;
        bool wanted = false;
    };
    QHash<QString, PoolCover> m_poolLeases;
    // Ready identities map to whether the tile contains real artwork rather than a placeholder.
    QHash<QString, bool> m_readyCovers;
    std::array<OwnedCover, sceneAtlas::OwnedCoverCount> m_ownedCovers;
    quint64 m_ownedSerial = 0;
    int m_detailResolution = yalahoo::rhi::renderData::CoverTilePixels;
    int m_publishedDetailResolution = 0;
    int m_ownedSide = yalahoo::rhi::renderData::CoverTilePixels;
    QSize m_atlasSize{ sceneAtlas::BaseWidthPixels, sceneAtlas::InitialHeightPixels };
    quint64 m_activeObject = 0;
    QPointer<SelectionController> m_poolSelection;
    QImage m_cover;
    CoverCache m_covers;
    SelectedCoverPool m_coverPool;
    std::shared_ptr<SelectedCoverLease> m_selectedCover;
    QMetaObject::Connection m_poolResetConnection;
    bool m_poolMode = false;
    quint64 m_coverRevision = 0, m_publishedCoverRevision = 0;
};
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
SceneCovers::SceneCovers(QObject *parent) : QObject(parent), d(std::make_unique<Private>())
{
    d->m_cover = QImage(renderData::CoverTilePixels, renderData::CoverTilePixels,
                        QImage::Format_RGBA8888_Premultiplied);
    d->m_cover.fill(Qt::transparent);
    d->m_selectedCover = d->m_coverPool.acquire(u"demo-square"_s);
    connect(&d->m_covers, &CoverCache::ready, this, &SceneCovers::handleCoverReady);
}
void SceneCovers::setSelection(SelectionController *selection)
{
    if (d->m_poolSelection == selection && d->m_poolMode == bool(selection))
        return;
    disconnect(d->m_poolResetConnection);
    d->m_poolSelection = selection;
    d->m_poolMode = selection;
    resetPool();
    if (selection)
        d->m_poolResetConnection = connect(selection, &SelectionController::libraryChanged, this,
                                           &SceneCovers::refreshPool);
    else
        d->m_selectedCover = d->m_coverPool.acquire(u"demo-square"_s);
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
        requestDemoCover();
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
    preparePool(in, previous);
    d->m_publishedCoverRevision = d->m_coverRevision;
    d->m_publishedDetailResolution = d->m_detailResolution;
}
bool SceneCovers::prepared() const
{
    if (d->m_readyCovers.size() != d->m_poolLeases.size())
        return false;
    const bool hasOwned = std::any_of(d->m_ownedCovers.cbegin(), d->m_ownedCovers.cend(),
                                      [](const OwnedCover &cover) {
                                          return bool(cover.pixels);
                                      });
    // A failed upgrade is terminal for this demand; retained artwork is still usable.
    const bool selectedUsable = std::any_of(
            d->m_ownedCovers.cbegin(), d->m_ownedCovers.cend(), [this](const OwnedCover &cover) {
                return cover.objectId == d->m_activeObject && cover.pixels && cover.pixels->pixels
                        && (cover.pixels->pixels->resolution >= d->m_detailResolution
                            || cover.highResolutionFailed);
            });
    if (hasOwned && !selectedUsable)
        return false;
    return true;
}
void SceneCovers::handlePoolCoverReady(const CoverSnapshot &pixels)
{
    if (d->m_poolSelection && !pixels->error.isEmpty()
        && pixels->fallbackTitle
                != d->m_poolSelection->coverSource(pixels->identity).placeholderTitle) {
        Q_EMIT changed();
        return;
    }
    if (pixels->resolution == renderData::CoverTilePixels) {
        const auto lease = d->m_poolLeases.constFind(pixels->identity);
        if (lease != d->m_poolLeases.cend() && d->m_atlas.publish(lease->lease, pixels))
            d->m_readyCovers.insert(pixels->identity, pixels->error.isEmpty());
    }
    for (auto &owned : d->m_ownedCovers)
        if (owned.objectId == d->m_activeObject && owned.pixels
            && owned.pixels->identity == pixels->identity) {
            if (pixels->resolution >= d->m_detailResolution)
                owned.highResolutionFailed = !pixels->error.isEmpty();
            d->m_coverPool.upgrade(owned.pixels, pixels);
        }
    ++d->m_coverRevision;
    Q_EMIT changed();
    return;
}
void SceneCovers::handleCoverReady(const CoverSnapshot &pixels)
{
    if (d->m_poolMode) {
        handlePoolCoverReady(pixels);
        return;
    }
    if (d->m_coverPool.upgrade(d->m_selectedCover, pixels)) {
        d->m_cover = pixels->image;
        Q_EMIT changed();
    }
}
void SceneCovers::resetPool()
{
    d->m_covers.reset();
    d->m_coverPool.reset();
    d->m_selectedCover.reset();
    d->m_poolLeases.clear();
    d->m_readyCovers.clear();
    d->m_atlas.reset();
    d->m_ownedCovers = { };
    d->m_ownedSide = renderData::CoverTilePixels;
    d->m_detailResolution = renderData::CoverTilePixels;
    d->m_activeObject = 0;
    ++d->m_coverRevision;
    Q_EMIT changed();
}
void SceneCovers::refreshPool()
{
    if (!d->m_poolSelection)
        return;
    // Only the bounded visible pool is reconciled; content-addressed native paths
    // carry revisions, while missing artwork also depends on its fallback title.
    for (auto it = d->m_poolLeases.begin(); it != d->m_poolLeases.end();) {
        const auto next = d->m_poolSelection->coverSource(it.key());
        if (it->source == next) {
            ++it;
            continue;
        }
        const bool ownedPlaceholder = std::any_of(
                d->m_ownedCovers.cbegin(), d->m_ownedCovers.cend(), [&it](const OwnedCover &owned) {
                    return owned.pixels && owned.pixels->identity == it.key()
                            && owned.pixels->pixels && !owned.pixels->pixels->error.isEmpty();
                });
        if (!next.path.isEmpty() && next.path == it->source.path
            && d->m_readyCovers.value(it.key(), false) && !ownedPlaceholder) {
            it->source = next;
            ++it;
            continue;
        }
        const auto id = it.key();
        d->m_covers.invalidate(id);
        d->m_atlas.release(it->lease);
        d->m_readyCovers.remove(id);
        for (auto &owned : d->m_ownedCovers)
            if (owned.pixels && owned.pixels->identity == id)
                owned = { };
        it = d->m_poolLeases.erase(it);
    }
    ++d->m_coverRevision;
    Q_EMIT changed();
}
// Resolve leases by stable identity and content generation. Detailed tiles share an owned image
// snapshot, so a GUI publication never borrows pixels from a temporary cache lookup.
void SceneCovers::ensureBaseCover(const QString &id, const CoverSource &source,
                                  const AtlasLease &lease)
{
    if (d->m_readyCovers.contains(id))
        return;

    if (auto pixels = d->m_covers.lookup(id, source.path, renderData::CoverTilePixels,
                                         source.placeholderTitle)) {
        if (d->m_atlas.publish(lease, pixels))
            d->m_readyCovers.insert(id, pixels->error.isEmpty());
    } else if (d->m_covers.hasPendingCapacity())
        d->m_covers.request(id, source.path, renderData::CoverTilePixels, source.placeholderTitle);
}
SceneCovers::CoverRegion SceneCovers::prepareOwnedCover(Input &in, const ArchiveInstance &instance,
                                                        const CoverSource &source, QRect tile,
                                                        quint64 coverLease)
{
    auto *it = acquireOwnedCover(instance, source);
    if (!it || !it->pixels)
        return { tile, coverLease };
    requestDetailedCover(*it, instance.albumId, source);
    const auto pixels = it->pixels->pixels;
    if (!pixels || pixels->resolution <= renderData::CoverTilePixels)
        return { tile, coverLease };
    const int block = int(it - d->m_ownedCovers.data());
    const int top =
            ((yalahoo::rhi::sceneAtlas::BaseWidthPixels + d->m_ownedSide - 1) / d->m_ownedSide)
            * d->m_ownedSide;
    tile = { (block % 2) * d->m_ownedSide, top + (block / 2) * d->m_ownedSide, pixels->resolution,
             pixels->resolution };
    coverLease = it->lease;
    if (it->tiledPixels != pixels || it->tiledRegion != tile) {
        it->uploads.clear();
        it->uploads.reserve(pixels->tiles.size());
        for (const auto &part : pixels->tiles) {
            const int slot = ((tile.y() + part.origin.y()) / renderData::CoverTilePixels)
                            * (d->m_atlasSize.width() / renderData::CoverTilePixels)
                    + (tile.x() + part.origin.x()) / renderData::CoverTilePixels;
            it->uploads.append({ d->m_atlas.generation(), it->lease, ++d->m_ownedSerial, slot,
                                 part.image, true, part.mipmaps });
        }
        it->tiledPixels = pixels;
        it->tiledRegion = tile;
    }
    in.atlasUploads += it->uploads;
    return { tile, coverLease };
}
SceneCovers::OwnedCover *SceneCovers::acquireOwnedCover(const ArchiveInstance &instance,
                                                        const CoverSource &source)
{
    auto it = std::find_if(d->m_ownedCovers.begin(), d->m_ownedCovers.end(),
                           [&](const OwnedCover &cover) {
                               return cover.objectId == instance.objectId;
                           });
    if (it != d->m_ownedCovers.end())
        return &*it;
    it = std::find_if(d->m_ownedCovers.begin(), d->m_ownedCovers.end(),
                      [](const OwnedCover &cover) {
                          return cover.objectId == 0;
                      });
    if (it == d->m_ownedCovers.end())
        return nullptr;
    it->objectId = instance.objectId;
    it->lease = ++d->m_ownedSerial;
    it->pixels = d->m_coverPool.acquire(instance.albumId,
                                        d->m_covers.lookup(instance.albumId, source.path,
                                                           renderData::CoverTilePixels,
                                                           source.placeholderTitle),
                                        d->m_covers.generation());
    return &*it;
}
void SceneCovers::requestDetailedCover(OwnedCover &owned, const QString &id,
                                       const CoverSource &source)
{
    if (owned.objectId != d->m_activeObject
        || (owned.highResolutionFailed
            && (!owned.pixels->pixels || owned.pixels->pixels->error.isEmpty()))
        || (owned.pixels->pixels && owned.pixels->pixels->resolution >= d->m_detailResolution))
        return;
    if (auto pixels = d->m_covers.lookup(id, source.path, d->m_detailResolution,
                                         source.placeholderTitle)) {
        owned.highResolutionFailed = !pixels->error.isEmpty();
        d->m_coverPool.upgrade(owned.pixels, pixels);
        return;
    }
    if (d->m_covers.hasPendingCapacity())
        d->m_covers.request(id, source.path, d->m_detailResolution, source.placeholderTitle);
}
bool SceneCovers::reusePool(Input &in, const Input &previous) const
{
    const auto &instances = std::as_const(in.scene.instances);
    if (!previous.poolFrame || previous.atlasGeneration != d->m_atlas.generation()
        || d->m_coverRevision != d->m_publishedCoverRevision
        || d->m_detailResolution != d->m_publishedDetailResolution
        || d->m_readyCovers.size() != d->m_poolLeases.size()
        || !std::equal(instances.cbegin(), instances.cend(), previous.entityIds.cbegin(),
                       previous.entityIds.cend(), [](const ArchiveInstance &instance, quint64 id) {
                           return instance.objectId == id;
                       }))
        return false;
    // Unready work must keep visiting admission until it has a usable selected snapshot.
    if (!instances.isEmpty() && instances.last().owned) {
        const auto selected = std::find_if(d->m_ownedCovers.cbegin(), d->m_ownedCovers.cend(),
                                           [this](const OwnedCover &cover) {
                                               return cover.objectId == d->m_activeObject;
                                           });
        if (selected == d->m_ownedCovers.cend() || !selected->pixels || !selected->pixels->pixels
            || (selected->pixels->pixels->resolution < d->m_detailResolution
                && !selected->highResolutionFailed))
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
void SceneCovers::preparePool(Input &in, const Input &previous)
{
    auto *selection = d->m_poolSelection.data();
    const auto &instances = std::as_const(in.scene.instances);
    d->m_activeObject = instances.isEmpty() ? 0 : instances.last().objectId;
    if (reusePool(in, previous))
        return;
    in.entities.clear();
    in.entityIds.clear();
    std::array<quint64, SelectedCoverPool::capacity> ownedIds;
    qsizetype ownedCount = 0;
    const int count = int(instances.size());
    for (auto &cover : d->m_poolLeases)
        cover.wanted = false;
    in.entities.resize(count);
    in.entityIds.resize(count);
    in.coverTiles.resize(count);
    in.coverLeases.resize(count);
    in.fallbackCoverTiles.resize(count);
    in.fallbackCoverLeases.resize(count);
    for (const auto &instance : instances) {
        if (auto cover = d->m_poolLeases.find(instance.albumId); cover != d->m_poolLeases.end())
            cover->wanted = true;
        if (instance.owned) {
            if (ownedCount >= SelectedCoverPool::capacity)
                throw std::runtime_error("Owned cover capacity exceeded");
            ownedIds[ownedCount++] = instance.objectId;
        }
    }
    for (auto it = d->m_poolLeases.begin(); it != d->m_poolLeases.end();) {
        if (!it->wanted) {
            d->m_covers.cancel(it.key());
            d->m_atlas.release(it->lease);
            d->m_readyCovers.remove(it.key());
            it = d->m_poolLeases.erase(it);
        } else
            ++it;
    }
    for (auto &owned : d->m_ownedCovers) {
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
            && std::none_of(d->m_ownedCovers.begin(), d->m_ownedCovers.end(),
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
        const auto entry = d->m_poolLeases.tryEmplace(id);
        if (entry.inserted) {
            entry.iterator->lease = d->m_atlas.acquire(id, d->m_covers.generation());
            entry.iterator->source = selection->coverSource(id);
        }
        const auto &source = entry.iterator->source;
        const auto &lease = entry.iterator->lease;
        auto tile = d->m_atlas.tile(lease);
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
    in.atlasSize = d->m_atlasSize;
    in.atlasGeneration = d->m_atlas.generation();
    in.poolFrame = true;
    // Immutable replay handles render-context recreation and superseded GUI submissions;
    // the render queue rejects known revisions before conversion or upload.
    in.atlasUploads += d->m_atlas.replay(0, d->m_atlasSize.width() / renderData::CoverTilePixels);
    in.immutableCover = true;
}
QJsonObject SceneCovers::statistics() const
{
    int detailed = 0, owned = 0;
    qint64 ownedBytes = 0, tiledBytes = 0;
    bool selectedDetailed = false;
    QJsonArray owners;
    for (const auto &cover : d->m_ownedCovers) {
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
            detailed += cover.pixels->pixels->resolution >= d->m_detailResolution;
            if (cover.objectId == d->m_activeObject)
                selectedDetailed = cover.pixels->pixels->resolution >= d->m_detailResolution;
        }
        for (const auto &upload : cover.uploads)
            tiledBytes += AtlasUploadQueue::payloadBytes(upload);
    }
    return { { u"owners"_s, owners },
             { u"cache"_s, d->m_covers.statistics() },
             { u"identities"_s, d->m_poolLeases.size() },
             { u"ready"_s, d->m_readyCovers.size() },
             { u"owned"_s, owned },
             { u"detailed"_s, detailed },
             { u"selectedDetailed"_s, selectedDetailed },
             { u"atlasRetainedBytes"_s, d->m_atlas.retainedBytes() },
             { u"ownedBytes"_s, ownedBytes },
             { u"tiledBytes"_s, tiledBytes },
             { u"atlasSize"_s, QJsonArray{ d->m_atlasSize.width(), d->m_atlasSize.height() } } };
}
void SceneCovers::requestDemoCover()
{
    const auto identity = u"demo-square"_s;
    const auto path = u":/assets/demo-covers/square.png"_s;
    if (auto pixels = d->m_covers.lookup(identity, path, d->m_detailResolution)) {
        if (d->m_coverPool.upgrade(d->m_selectedCover, pixels))
            d->m_cover = pixels->image;
    } else {
        d->m_covers.request(identity, path, renderData::CoverTilePixels);
        d->m_covers.request(identity, path, d->m_detailResolution);
    }
}
}
