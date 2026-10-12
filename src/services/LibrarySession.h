#pragma once
#include "LibraryModels.h"
#include "IMetadataProvider.h"
#include <QPointer>

namespace yalahoo {
// GUI-thread boundary; providers are injected, IDs never derive from view positions.
class LibrarySession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(yalahoo::AlbumListModel *albums READ albums CONSTANT)
    Q_PROPERTY(yalahoo::SearchListModel *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(QString navigationAlbumId READ navigationAlbumId NOTIFY navigationChanged)
    Q_PROPERTY(QString navigationTrackId READ navigationTrackId NOTIFY navigationChanged)
    Q_PROPERTY(QString playbackAlbumId READ playbackAlbumId NOTIFY playbackChanged)
    Q_PROPERTY(QString playbackTrackId READ playbackTrackId NOTIFY playbackChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged)
    Q_PROPERTY(QVariantMap providerCapabilities READ providerCapabilities NOTIFY providerCapabilitiesChanged)
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
    Q_INVOKABLE void search(const QString &query, const QString &genreId = { });
    Q_INVOKABLE void sort(const QString &mode);
    Q_INVOKABLE bool navigate(const QString &albumId, const QString &trackId = { });
    // Explicit playback-state synchronization; navigation/search/provider never call this.
    bool setPlaybackIds(const QString &albumId, const QString &trackId);
    Q_INVOKABLE quint64 requestMetadata(const QString &albumId, const QString &kind);
    Q_INVOKABLE void cancelMetadata(quint64 requestId);
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
    QVariantMap metadata() const
    {
        return m_metadata;
    }
    QVariantMap providerCapabilities() const;
signals:
    void navigationChanged();
    void playbackChanged();
    void metadataChanged();
    void providerCapabilitiesChanged();
    void metadataDelivered(const yalahoo::ProviderResult &result);

private:
    struct Pending
    {
        RequestToken token;
        MetadataKind kind;
    };
    void invalidate();
    void receive(const ProviderResult &result);
    QPointer<IMetadataProvider> m_provider;
    AlbumListModel m_albums;
    SearchListModel m_search;
    Snapshot m_snapshot;
    QHash<quint64, Pending> m_pending;
    quint64 m_generation = 0, m_requestId = 0;
    QString m_query, m_genre, m_navigationAlbumId, m_navigationTrackId, m_playbackAlbumId,
            m_playbackTrackId;
    QVariantMap m_metadata;
};
}
