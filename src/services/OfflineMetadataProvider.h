// Supplies the explicit offline metadata behavior through the provider interface. Results
// preserve missing/offline distinctions rather than inventing remote content.

#pragma once
#include "IMetadataProvider.h"
#include <QHash>
#include <QList>
#include <QTimer>

namespace yalahoo {
class OfflineMetadataProvider final : public IMetadataProvider
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OfflineMetadataProvider)
public:
    explicit OfflineMetadataProvider(QObject *parent = nullptr);
    ProviderCapabilities capabilities() const override
    {
        return { };
    }
    void setCache(Snapshot cache);
    void cancel(RequestToken token) override;

private:
    struct Pending final
    {
        AlbumQuery query;
        RequestToken token;
        MetadataKind kind;
        Snapshot cache;
        qsizetype albumIndex = -1;
    };
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind) override;
    void dispatch();
    Snapshot m_cache;
    QHash<QString, qsizetype> m_albumIndices;
    QList<Pending> m_pending;
    QTimer m_dispatch;
};
}
