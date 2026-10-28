// Supplies the explicit offline metadata behavior through the provider interface. Results
// preserve missing/offline distinctions rather than inventing remote content.

#pragma once
#include <memory>
#include "IMetadataProvider.h"

namespace yalahoo {
class OfflineMetadataProvider final : public IMetadataProvider
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OfflineMetadataProvider)
public:
    explicit OfflineMetadataProvider(QObject *parent = nullptr);
    ~OfflineMetadataProvider() override;
    ProviderCapabilities capabilities() const override;
    void setCache(Snapshot cache);
    void cancel(RequestToken token) override;

protected:
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
