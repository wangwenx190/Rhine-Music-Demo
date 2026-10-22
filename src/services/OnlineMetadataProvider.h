// Runs bounded asynchronous metadata requests on a dedicated network thread. Tokens and
// cancellation guard every publication back to the GUI facade.

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
    explicit OnlineMetadataProvider(
            QObject *parent = nullptr,
            const std::function<QNetworkAccessManager *()> &networkFactory = { });
    ~OnlineMetadataProvider() override;
    void configure(const QJsonObject &configuration);
    ProviderCapabilities capabilities() const override;
    void cancel(RequestToken token) override;

protected:
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
