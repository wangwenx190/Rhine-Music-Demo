#pragma once
#include "IMetadataProvider.h"
#include <QHash>

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
    void requestIntroduction(AlbumQuery query, RequestToken token) override;
    void requestCredits(AlbumQuery query, RequestToken token) override;
    void requestEnrichment(AlbumQuery query, RequestToken token) override;
    void cancel(RequestToken token) override;

private:
    void request(AlbumQuery query, RequestToken token, MetadataKind kind);
    Snapshot m_cache;
    QHash<RequestToken, bool> m_pending;
};
}
