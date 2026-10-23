#include "IMetadataProvider.h"
#include "OnlineMetadataProvider.h"
#include "metadata/MetadataWorker.h"
#include <QThread>
#include <QSet>
#include <QHash>

using namespace Qt::StringLiterals;

namespace yalahoo {
struct OnlineMetadataProvider::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
    QThread m_thread;
    MetadataWorker *m_worker;
    QHash<RequestToken, MetadataKind> m_pending;
    QSet<RequestToken> m_outstanding;
    bool m_enabled = false;
};
OnlineMetadataProvider::OnlineMetadataProvider(
        QObject *parent, const std::function<QNetworkAccessManager *()> &networkFactory)
    : IMetadataProvider(parent), d(std::make_unique<Private>())
{
    d->m_worker = new MetadataWorker(networkFactory ? networkFactory() : nullptr);
    qRegisterMetaType<ProviderResult>();
    d->m_worker->moveToThread(&d->m_thread);
    connect(&d->m_thread, &QThread::finished, d->m_worker, &QObject::deleteLater);
    connect(d->m_worker, &MetadataWorker::ready, this, [this](const ProviderResult &result) {
        const RequestToken token{ result.requestId, result.generation, result.albumId };
        d->m_outstanding.remove(token);
        const auto it = d->m_pending.find(token);
        if (it == d->m_pending.end() || *it != result.kind)
            return;
        d->m_pending.erase(it);
        Q_EMIT resultReady(result);
    });
    d->m_thread.setObjectName(u"metadata-network"_s);
    d->m_thread.start();
}
OnlineMetadataProvider::~OnlineMetadataProvider()
{
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = d->m_worker] {
                worker->stop();
                QThread::currentThread()->quit();
            },
            Qt::QueuedConnection);
    d->m_thread.wait();
}
void OnlineMetadataProvider::configure(const QJsonObject &configuration)
{
    const bool enabled = configuration.value(u"networkEnabled"_s).toBool();
    if (enabled == d->m_enabled)
        return;
    d->m_enabled = enabled;
    const auto pending = d->m_pending;
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        const auto &token = it.key();
        cancel(token);
        ProviderResult result;
        result.setRequest(token, it.value());
        result.status = ProviderStatus::Cancelled;
        Q_EMIT resultReady(result);
    }
    Q_EMIT capabilitiesChanged();
}
ProviderCapabilities OnlineMetadataProvider::capabilities() const
{
    return { d->m_enabled, true, true, true, true };
}
// Reserve capacity in the GUI facade before posting to the network thread.
// The token remains the acceptance key even if cancellation races with a queued reply.
bool OnlineMetadataProvider::request(AlbumQuery query, RequestToken token, MetadataKind kind)
{
    if (d->m_pending.contains(token))
        return d->m_pending.value(token) == kind;
    if (d->m_outstanding.size() >= yalahoo::metadataLimits::MaximumPendingRequests
        || d->m_outstanding.contains(token))
        return false;
    d->m_outstanding.insert(token);
    query.enabled = d->m_enabled && query.enabled;
    d->m_pending.insert(token, kind);
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = d->m_worker, query = std::move(query), token = std::move(token),
             kind]() mutable {
                worker->enqueue(std::move(query), std::move(token), kind);
            },
            Qt::QueuedConnection);
    return true;
}
void OnlineMetadataProvider::cancel(RequestToken token)
{
    if (!d->m_pending.remove(token))
        return;
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = d->m_worker, token = std::move(token)] {
                worker->cancel(token);
            },
            Qt::QueuedConnection);
}
}
