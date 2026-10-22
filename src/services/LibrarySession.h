// Coordinates immutable library models, browsing and metadata requests. Stable identities and
// request tokens separate navigation from persistence and playback.

#pragma once
#include <memory>
#include "LibraryModels.h"
#include "IMetadataProvider.h"

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
    AlbumListModel *albums();
    SearchListModel *searchResults();
    void setSnapshot(LibrarySnapshot snapshot);
    Snapshot snapshot() const;
    const Album *navigationAlbum() const;
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
    bool metadataBusy() const;
    Q_INVOKABLE quint64 requestMetadata(const QString &albumId, const QString &kind,
                                        bool force = false, const QString &releaseId = { });
    Q_INVOKABLE void cancelMetadata(quint64 requestId);
    Q_INVOKABLE void cancelAllMetadata();
    Q_INVOKABLE bool requestMetadataBatch(const QString &kind, bool missingOnly = false);
    Q_INVOKABLE QVariantMap metadataFor(const QString &albumId, const QString &kind,
                                        quint64 revision = 0) const;
    QVariantMap metadataConfiguration() const;
    QVariantMap metadataProgress() const;
    QString navigationAlbumId() const;
    QString navigationTrackId() const;
    QString playbackAlbumId() const;
    QString playbackTrackId() const;
    quint64 metadataRevision() const;
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
    struct Private;
    std::unique_ptr<Private> d;
};
}
