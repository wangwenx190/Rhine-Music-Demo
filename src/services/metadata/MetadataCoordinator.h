// Owns metadata requests, accepted results and serial batch progression. It borrows the
// session's index, which must outlive it; provider callbacks are queued to this QObject.
#pragma once
#include "IMetadataProvider.h"
#include <QVariantMap>
#include <memory>

namespace yalahoo {
class LibraryIndex;
class MetadataCoordinator final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MetadataCoordinator)
public:
    explicit MetadataCoordinator(IMetadataProvider *provider, const LibraryIndex &library,
                                 QObject *parent = nullptr);
    ~MetadataCoordinator() override;
    // Session transactions invalidate before replacing the index, then publish the new cache.
    [[nodiscard]] bool invalidate();
    void shutdown();
    void libraryReplaced();
    void metadataCommitted();
    void configurationChanged();
    bool busy() const;
    quint64 revision() const;
    QVariantMap metadata() const;
    QVariantMap capabilities() const;
    QVariantMap configuration() const;
    QVariantMap progress() const;
    Album resolvedAlbum(const QString &albumId) const;
    quint64 request(const QString &albumId, const QString &kind, bool force = false,
                    const QString &releaseId = { });
    void cancel(quint64 requestId);
    void cancelAll();
    bool requestBatch(const QString &kind, bool missingOnly);
    QVariantMap forAlbum(const QString &albumId, const QString &kind, quint64 revision) const;
Q_SIGNALS:
    void changed();
    void progressChanged();
    void capabilitiesChanged();
    void delivered(const yalahoo::ProviderResult &result);
    void albumChanged(const QString &albumId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
