// Private cover ownership and atlas state shared with read-only observers.
#pragma once
#include "SceneCovers.h"
#include "RhiItem.h"
#include "RenderData.h"
#include "SelectionController.h"
#include "AtlasAllocator.h"
#include <array>
namespace yalahoo::rhi {
namespace coverDetail {
namespace sceneAtlas {
// The fixed archive plus owned detail tiles determine the initial atlas allocation.
inline constexpr auto MaximumAtlasSide = 8192;
inline constexpr auto BaseColumns = 24;
inline constexpr auto OwnedCoverCount = 4;
inline constexpr auto BaseWidthPixels = 6144;
inline constexpr auto InitialHeightPixels = 6656;
}
}
using namespace coverDetail;
struct SceneCovers::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(SceneCovers *owner) : q(owner)
    {
    }
    SceneCovers *const q;

    struct OwnedCover final
    {
        quint64 objectId = 0, lease = 0;
        std::shared_ptr<SelectedCoverLease> pixels;
        CoverSnapshot tiledPixels;
        QRect tiledRegion;
        QVector<AtlasUpload> uploads;
        int failedResolution = 0;
    };
    struct CoverRegion final
    {
        QRect tile;
        quint64 lease;
    };
    void requestDemoCover();
    void handlePoolCoverReady(const std::shared_ptr<const CoverPixels> &pixels);
    void handleCoverReady(const std::shared_ptr<const CoverPixels> &pixels);
    void ensureBaseCover(const QString &id, const CoverSource &source, const AtlasLease &lease);
    CoverRegion prepareOwnedCover(Input &in, const ArchiveInstance &instance,
                                  const CoverSource &source, QRect tile, quint64 coverLease);
    OwnedCover *acquireOwnedCover(const ArchiveInstance &instance, const CoverSource &source);
    void requestDetailedCover(OwnedCover &owned, const QString &id, const CoverSource &source);
    bool reusePool(Input &input, const Input &previous) const;
    void preparePool(Input &input, const Input &previous);
    void resetPool();
    void refreshPool();
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
}
