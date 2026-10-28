#include "MetadataCachePolicy.h"
#include "MetadataMerge.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
constexpr auto PositiveCacheDays = 30;
constexpr auto RetryDelaySeconds = 60;
QString transportKey(const AlbumQuery &query)
{
    const auto &configuration = query.configuration;
    const auto bytes =
            QJsonDocument(QJsonArray{ configuration.value(u"proxyEnabled"_s).toBool(true),
                                      configuration.value(u"proxyMode"_s).toString(u"system"_s),
                                      configuration.value(u"proxyUrl"_s).toString(),
                                      query.contact })
                    .toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString verifiedReleaseId(const QJsonObject &state)
{
    const auto status = state.value(u"status"_s).toString().toLower();
    return state.contains(u"releaseVerifiedAt"_s) || status.isEmpty() || status == "matched"_L1
                    || status == "partial"_L1
            ? state.value(u"releaseId"_s).toString()
            : QString();
}
QDateTime verifiedReleaseTime(const QJsonObject &state)
{
    return QDateTime::fromString(
            state.value(u"releaseVerifiedAt"_s).toString(state.value(u"checkedAt"_s).toString()),
            Qt::ISODateWithMs);
}
QString stateName(ProviderStatus status)
{
    return status == ProviderStatus::NotFound ? u"not-found"_s : statusName(status).toLower();
}
}
QString preferredRelease(const AlbumQuery &query)
{
    if (!query.releaseId.isEmpty())
        return query.releaseId;
    const auto credits = query.album.creditsLookup;
    const auto online = query.album.online;
    const auto creditsId = verifiedReleaseId(credits);
    const auto onlineId = verifiedReleaseId(online);
    const auto creditsTime = verifiedReleaseTime(credits);
    const auto onlineTime = verifiedReleaseTime(online);
    return !creditsId.isEmpty()
                    && (onlineId.isEmpty()
                        || (creditsTime.isValid()
                            && (!onlineTime.isValid() || creditsTime > onlineTime)))
            ? creditsId
            : onlineId;
}
std::optional<ProviderResult> reusableMetadata(const AlbumQuery &query, const RequestToken &token,
                                               MetadataKind kind, const QString &inputKey,
                                               const QDateTime &now)
{
    const auto cached = cachedMetadata(query.album, kind, token);
    const auto state = metadataState(query.album, kind);
    const auto expires =
            QDateTime::fromString(state.value(u"expiresAt"_s).toString(), Qt::ISODateWithMs);
    const auto retry = QDateTime::fromString(cached.retryAt, Qt::ISODateWithMs);
    const auto desiredRelease =
            kind == MetadataKind::Introduction ? QString() : preferredRelease(query);
    const bool same =
            (!state.contains(u"inputKey"_s) || state.value(u"inputKey"_s).toString() == inputKey)
            && (desiredRelease.isEmpty()
                || state.value(u"lookupReleaseId"_s)
                                .toString(state.value(u"releaseId"_s).toString())
                        == desiredRelease);
    const auto checked = QDateTime::fromString(cached.checkedAt, Qt::ISODateWithMs);
    const bool positive =
            cached.status == ProviderStatus::Matched || cached.status == ProviderStatus::Partial;
    const auto expiry = expires.isValid() ? expires
            : checked.isValid()           ? checked.addDays(positive ? PositiveCacheDays : 1)
                                          : QDateTime();
    const bool fresh = expiry.isValid() ? expiry > now : positive;
    const bool hasCache = kind != MetadataKind::Credits || !state.isEmpty();
    const bool sameTransport = cached.status != ProviderStatus::Error
            || state.value(u"transportKey"_s).toString() == transportKey(query);
    if (same && (sameTransport || !query.enabled) && (hasCache || !query.enabled)
        && cached.status != ProviderStatus::Deferred
        && ((!query.force && fresh) || !query.enabled || retry > now)) {
        auto result = cached;
        result.fromCache = true;
        return result;
    }
    return std::nullopt;
}
void stampMetadata(ProviderResult &result, const AlbumQuery &query, const QString &releaseId,
                   const QDateTime &now)
{
    result.checkedAt = now.toString(Qt::ISODateWithMs);
    if (result.status == ProviderStatus::Error && result.retryAt.isEmpty())
        result.retryAt = now.addSecs(RetryDelaySeconds).toString(Qt::ISODateWithMs);
    const auto field = metadataStateKey(result.kind);
    auto state = metadataState(query.album, result.kind);
    const auto previousId = verifiedReleaseId(state);
    const auto previousTime = verifiedReleaseTime(state);
    const auto verifiedId =
            result.payload.value(u"online"_s).toObject().value(u"releaseId"_s).toString();
    const bool verified = !releaseId.isEmpty() && verifiedId == releaseId
            && (result.status == ProviderStatus::Matched || result.status == ProviderStatus::Partial
                || result.status == ProviderStatus::NotFound);
    const auto patch = result.payload.value(field).toObject();
    for (auto it = patch.begin(); it != patch.end(); ++it)
        state.insert(it.key(), it.value());
    if (!patch.contains(u"candidates"_s))
        state.remove(u"candidates"_s);
    if (result.source.isEmpty())
        result.source = { { u"name"_s,
                            result.kind == MetadataKind::Introduction ? u"Wikipedia / Wikidata"_s
                                                                      : u"MusicBrainz"_s } };
    if (result.kind != MetadataKind::Introduction) {
        // Query attempts and verified identities have independent clocks.
        state.insert(u"lookupReleaseId"_s, releaseId);
        if (verified || !previousId.isEmpty()) {
            state.insert(u"releaseId"_s, verified ? releaseId : previousId);
            state.insert(u"releaseVerifiedAt"_s,
                         (verified ? now : previousTime).toString(Qt::ISODateWithMs));
        } else {
            state.remove(u"releaseId"_s);
            state.remove(u"releaseVerifiedAt"_s);
        }
    }
    state.insert(u"source"_s, result.source.value(u"name"_s));
    state.insert(u"sourceDetails"_s, result.source);
    if (result.source.contains(u"url"_s))
        state.insert(u"sourceUrl"_s, result.source.value(u"url"_s));
    else
        state.remove(u"sourceUrl"_s);
    state.insert(u"status"_s, stateName(result.status));
    state.insert(u"checkedAt"_s, result.checkedAt);
    state.insert(u"retryAt"_s, result.retryAt);
    state.insert(u"error"_s, result.error);
    state.insert(u"inputKey"_s, result.inputKey);
    if (result.status == ProviderStatus::Error)
        state.insert(u"transportKey"_s, transportKey(query));
    else
        state.remove(u"transportKey"_s);
    state.insert(u"expiresAt"_s,
                 result.status == ProviderStatus::Error
                         ? result.retryAt
                         : now.addDays(result.status == ProviderStatus::Matched
                                                       || result.status == ProviderStatus::Partial
                                               ? PositiveCacheDays
                                               : 1)
                                   .toString(Qt::ISODateWithMs));
    result.payload.insert(field, state);
    if (!result.source.isEmpty())
        result.source.insert(u"checkedAt"_s, result.checkedAt);
}
}
