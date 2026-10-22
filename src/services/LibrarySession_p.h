// Internal implementation detail. Include only from the owner, its collaborating peer or tests.
#pragma once
#include "LibrarySession.h"
#include <memory>
#include "LibraryModels.h"
#include "IMetadataProvider.h"
#include <QPointer>
#include <QSet>
namespace yalahoo {
struct LibrarySession::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(LibrarySession *owner, IMetadataProvider *provider)
        : q(owner),
          m_provider(provider),
          m_albums(owner),
          m_search(owner),
          m_snapshot(std::make_shared<const LibrarySnapshot>())
    {
    }
    LibrarySession *const q;

    void adoptPlaybackIds(const QString &albumId, const QString &trackId);
    struct Pending final
    {
        RequestToken token;
        MetadataKind kind;
    };
    void invalidate();
    void rebuildSummary();
    void rebuildBrowsePositions();
    void advanceBatch();
    void receive(const ProviderResult &result);
    const Album *album(const QString &id) const;
    const Track *track(const QString &albumId, const QString &trackId) const;
    QPointer<IMetadataProvider> m_provider;
    AlbumListModel m_albums;
    SearchListModel m_search;
    Snapshot m_snapshot;
    QHash<QString, qsizetype> m_albumIndices;
    QHash<QString, QString> m_genreNames;
    struct BrowsePosition final
    {
        int code = 0, number = 0, total = 0, lane = 0;
        QString label;
    };
    QHash<QString, BrowsePosition> m_browsePositions;
    QHash<QString, QHash<QString, qsizetype>> m_trackIndices;
    QHash<quint64, Pending> m_pending;
    quint64 m_generation = 0, m_requestId = 0, m_metadataRevision = 0;
    QString m_query, m_genre, m_navigationAlbumId, m_navigationTrackId, m_playbackAlbumId,
            m_playbackTrackId;
    QVariantList m_genres;
    QSet<QString> m_visibleGenres;
    QHash<QString, ProviderResult> m_metadata;
    QVariantMap m_summary;
    QStringList m_batchAlbums;
    QString m_batchKind, m_batchStatus;
    quint64 m_batchRequest = 0;
    int m_batchNext = 0, m_batchCompleted = 0, m_batchErrors = 0;
    bool m_batchActive = false;
};
}
