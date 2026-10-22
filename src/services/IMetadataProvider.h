// Defines asynchronous album metadata requests, capabilities and results. Tokens identify the
// accepted request so cancellation and replacement remain explicit.

#pragma once
#include "MusicTypes.h"
#include <QObject>
#include <QStringView>
#include <optional>
#include <utility>

namespace yalahoo {
enum class MetadataKind {
    Introduction,
    Credits,
    Enrichment
};
enum class ProviderStatus {
    Deferred,
    Matched,
    Partial,
    NotFound,
    Uncertain,
    Error,
    Cancelled
};
QString kindName(MetadataKind kind);
QString statusName(ProviderStatus status);
struct RequestToken final
{
    quint64 requestId = 0, generation = 0;
    QString albumId;
    bool operator==(const RequestToken &) const = default;
};
inline size_t qHash(const RequestToken &token, size_t seed = 0)
{
    return qHashMulti(seed, token.requestId, token.generation, token.albumId);
}
struct AlbumQuery final
{
    QString albumId, contact, releaseId;
    Album album;
    QStringList albumIds, trackIds;
    bool enabled = false, force = false;
    QJsonObject configuration;
};
struct ProviderCapabilities final
{
    bool networkEnabled = false;
    bool introductions = true, credits = true, enrichment = true, batch = false;
};
struct ProviderResult final
{
    quint64 requestId = 0, generation = 0;
    QString albumId;
    MetadataKind kind = MetadataKind::Introduction;
    ProviderStatus status = ProviderStatus::Deferred;
    QJsonObject payload, source;
    QString checkedAt, retryAt, error;
    QString inputKey;
    bool fromCache = false;
    qsizetype payloadBytes = 0;
};
// A single descriptor maps public names, album cache fields and provider capabilities.
std::optional<MetadataKind> metadataKind(QStringView name);
QString metadataStateKey(MetadataKind kind);
QJsonObject metadataState(const Album &album, MetadataKind kind);
bool supportsMetadata(const ProviderCapabilities &capabilities, MetadataKind kind);
class IMetadataProvider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IMetadataProvider)
public:
    IMetadataProvider() = default;
    using QObject::QObject;
    virtual ProviderCapabilities capabilities() const = 0;
    [[nodiscard]] bool requestIntroduction(AlbumQuery query, RequestToken token)
    {
        return request(std::move(query), std::move(token), MetadataKind::Introduction);
    }
    [[nodiscard]] bool requestCredits(AlbumQuery query, RequestToken token)
    {
        return request(std::move(query), std::move(token), MetadataKind::Credits);
    }
    [[nodiscard]] bool requestEnrichment(AlbumQuery query, RequestToken token)
    {
        return request(std::move(query), std::move(token), MetadataKind::Enrichment);
    }
    virtual void cancel(RequestToken token) = 0;

protected:
    [[nodiscard]] virtual bool request(AlbumQuery query, RequestToken token, MetadataKind kind) = 0;
Q_SIGNALS:
    void capabilitiesChanged();
    void resultReady(const yalahoo::ProviderResult &result);
};
}
Q_DECLARE_METATYPE(yalahoo::ProviderResult)

namespace yalahoo::metadataLimits {
inline constexpr auto MaximumPendingRequests = 8;
}
