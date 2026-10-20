#pragma once
#include "MusicTypes.h"
#include <QObject>

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
class IMetadataProvider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IMetadataProvider)
public:
    IMetadataProvider() = default;
    using QObject::QObject;
    virtual ProviderCapabilities capabilities() const = 0;
    [[nodiscard]] virtual bool requestIntroduction(AlbumQuery query, RequestToken token) = 0;
    [[nodiscard]] virtual bool requestCredits(AlbumQuery query, RequestToken token) = 0;
    [[nodiscard]] virtual bool requestEnrichment(AlbumQuery query, RequestToken token) = 0;
    virtual void cancel(RequestToken token) = 0;
Q_SIGNALS:
    void capabilitiesChanged();
    void resultReady(const yalahoo::ProviderResult &result);
};
}
Q_DECLARE_METATYPE(yalahoo::ProviderResult)
