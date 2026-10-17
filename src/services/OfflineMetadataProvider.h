#pragma once
#include "IMetadataProvider.h"
#include <QHash>
#include <QList>
#include <QTimer>

namespace yalahoo {
class OfflineMetadataProvider final : public IMetadataProvider
{
    Q_OBJECT
public:
    explicit OfflineMetadataProvider(QObject *parent = nullptr);
    ProviderCapabilities capabilities() const override
    {
        return { };
    }
    void setCache(Snapshot cache);
    bool requestIntroduction(AlbumQuery query, RequestToken token) override;
    bool requestCredits(AlbumQuery query, RequestToken token) override;
    bool requestEnrichment(AlbumQuery query, RequestToken token) override;
    void cancel(RequestToken token) override;

private:
    struct Pending
    {
        AlbumQuery query;
        RequestToken token;
        MetadataKind kind;
        Snapshot cache;
        qsizetype albumIndex = -1;
    };
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind);
    void dispatch();
    Snapshot m_cache;
    QHash<QString, qsizetype> m_albumIndices;
    QList<Pending> m_pending;
    QTimer m_dispatch;
};
}
