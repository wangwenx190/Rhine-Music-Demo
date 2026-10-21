// Coordinates immutable library models, browsing and metadata requests. Stable identities and
// request tokens separate navigation from persistence and playback.

#pragma once
#include "LibraryModels.h"
#include "IMetadataProvider.h"
#include <QPointer>
#include <QSet>

namespace yalahoo {
// GUI-thread boundary; providers are injected, IDs never derive from view positions.
class LibrarySession final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LibrarySession)
    Q_PROPERTY(yalahoo::AlbumListModel *albums READ albums CONSTANT FINAL)
    Q_PROPERTY(yalahoo::SearchListModel *searchResults READ searchResults CONSTANT FINAL)
    Q_PROPERTY(QString navigationAlbumId READ navigationAlbumId NOTIFY navigationChanged FINAL)
    Q_PROPERTY(QString navigationTrackId READ navigationTrackId NOTIFY navigationChanged FINAL)
    Q_PROPERTY(QString playbackAlbumId READ playbackAlbumId NOTIFY playbackChanged FINAL)
    Q_PROPERTY(QString playbackTrackId READ playbackTrackId NOTIFY playbackChanged FINAL)
    Q_PROPERTY(QVariantList genres READ genres NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged FINAL)
    Q_PROPERTY(quint64 metadataRevision READ metadataRevision NOTIFY metadataChanged FINAL)
    Q_PROPERTY(QVariantMap metadataProgress READ metadataProgress NOTIFY metadataProgressChanged FINAL)
    Q_PROPERTY(bool metadataBusy READ metadataBusy NOTIFY metadataProgressChanged FINAL)
    Q_PROPERTY(QVariantMap metadataConfiguration READ metadataConfiguration NOTIFY metadataConfigurationChanged FINAL)
    Q_PROPERTY(QVariantMap browseInfo READ browseInfo NOTIFY browseInfoChanged FINAL)
    Q_PROPERTY(QVariantMap providerCapabilities READ providerCapabilities NOTIFY providerCapabilitiesChanged FINAL)
public:
    explicit LibrarySession(IMetadataProvider *provider, QObject *parent = nullptr);
    ~LibrarySession() override;
    AlbumListModel *albums()
    {
        return &m_albums;
    }
    SearchListModel *searchResults()
    {
        return &m_search;
    }
    void setSnapshot(LibrarySnapshot snapshot);
    Snapshot snapshot() const
    {
        return m_snapshot;
    }
    const Album *navigationAlbum() const
    {
        return album(m_navigationAlbumId);
    }
    QVariantList genres() const;
    Q_INVOKABLE bool hasGenre(const QString &id) const;
    QVariantMap summary() const;
    int trackIndex(const QString &albumId, const QString &trackId) const;
    bool contains(const QString &albumId, const QString &trackId = { }) const;
    Q_INVOKABLE void search(const QString &query, const QString &genreId = { });
    Q_INVOKABLE void sort(const QString &mode);
    Q_INVOKABLE bool navigate(const QString &albumId, const QString &trackId = { });
    // Explicit playback-state synchronization; navigation/search/provider never call this.
    bool setPlaybackIds(const QString &albumId, const QString &trackId);
    Album resolvedAlbum(const QString &albumId) const;
    void adoptMetadataSnapshot(Snapshot snapshot);
    void adoptMetadataConfiguration(const Snapshot &snapshot);
    bool metadataBusy() const
    {
        return !m_pending.isEmpty();
    }
    Q_INVOKABLE quint64 requestMetadata(const QString &albumId, const QString &kind,
                                        bool force = false, const QString &releaseId = { });
    Q_INVOKABLE void cancelMetadata(quint64 requestId);
    Q_INVOKABLE void cancelAllMetadata();
    Q_INVOKABLE bool requestMetadataBatch(const QString &kind, bool missingOnly = false);
    Q_INVOKABLE QVariantMap metadataFor(const QString &albumId, const QString &kind,
                                        quint64 revision = 0) const;
    QVariantMap metadataConfiguration() const;
    QVariantMap metadataProgress() const;
    QString navigationAlbumId() const
    {
        return m_navigationAlbumId;
    }
    QString navigationTrackId() const
    {
        return m_navigationTrackId;
    }
    QString playbackAlbumId() const
    {
        return m_playbackAlbumId;
    }
    QString playbackTrackId() const
    {
        return m_playbackTrackId;
    }
    quint64 metadataRevision() const
    {
        return m_metadataRevision;
    }
    QVariantMap metadata() const;
    QVariantMap providerCapabilities() const;
    QVariantMap browseInfo() const;
Q_SIGNALS:
    void snapshotChanged();
    void navigationChanged();
    void browseInfoChanged();
    void playbackChanged();
    void metadataChanged();
    void metadataProgressChanged();
    void providerCapabilitiesChanged();
    void metadataDelivered(const yalahoo::ProviderResult &result);
    void albumMetadataChanged(const QString &albumId);
    void metadataConfigurationChanged();

private:
    friend class PlaybackController;
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
