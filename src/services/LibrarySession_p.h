// Internal implementation detail. Include only from the owner, its collaborating peer or tests.
#pragma once
#include "LibrarySession.h"
#include <memory>
#include "LibraryModels.h"
#include "IMetadataProvider.h"
#include "LibraryIndex.h"
#include "metadata/MetadataCoordinator.h"
#include <QSet>
namespace yalahoo {
struct LibrarySession::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(LibrarySession *owner, IMetadataProvider *provider)
        : q(owner), m_albums(owner), m_search(owner), m_metadata(provider, m_index, owner)
    {
    }
    LibrarySession *const q;

    void adoptPlaybackIds(const QString &albumId, const QString &trackId);
    void rebuildSummary();
    void rebuildBrowsePositions();
    AlbumListModel m_albums;
    SearchListModel m_search;
    LibraryIndex m_index;
    MetadataCoordinator m_metadata;
    QHash<QString, QString> m_genreNames;
    struct BrowsePosition final
    {
        int code = 0, number = 0, total = 0, lane = 0;
        QString label;
    };
    QHash<QString, BrowsePosition> m_browsePositions;
    QString m_query, m_genre, m_navigationAlbumId, m_navigationTrackId, m_playbackAlbumId,
            m_playbackTrackId;
    QVariantList m_genres;
    QSet<QString> m_visibleGenres;
    QVariantMap m_summary;
};
}
