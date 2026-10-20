#pragma once
#include "IMetadataProvider.h"
#include <QHash>
#include <QSet>
#include <QThread>
#include <functional>
class QNetworkAccessManager;

namespace yalahoo {
class MetadataWorker;
// The GUI facade has at most eight accepted requests. Network/JSON work lives on its thread.
class OnlineMetadataProvider final : public IMetadataProvider
{
    Q_OBJECT
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
    QThread m_thread;
    MetadataWorker *m_worker;
    QHash<RequestToken, MetadataKind> m_pending;
    QSet<RequestToken> m_outstanding;
    bool m_enabled = false;
};
}
