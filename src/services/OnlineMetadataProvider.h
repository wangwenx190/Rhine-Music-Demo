#pragma once
#include <memory>
#include "IMetadataProvider.h"
#include <functional>
class QNetworkAccessManager;

namespace yalahoo {
// The GUI facade has at most eight accepted requests. Network/JSON work lives on its thread.
class OnlineMetadataProvider final : public IMetadataProvider
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OnlineMetadataProvider)
public:
    explicit OnlineMetadataProvider(QObject *parent = nullptr,
                                    std::function<QNetworkAccessManager *()> networkFactory = { });
    ~OnlineMetadataProvider() override;
    void configure(const QJsonObject &configuration);
    ProviderCapabilities capabilities() const override;
    bool requestIntroduction(AlbumQuery query, RequestToken token) override;
    bool requestCredits(AlbumQuery query, RequestToken token) override;
    bool requestEnrichment(AlbumQuery query, RequestToken token) override;
    void cancel(RequestToken token) override;

private:
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind);
    struct Private;
    std::unique_ptr<Private> d;
};
}
