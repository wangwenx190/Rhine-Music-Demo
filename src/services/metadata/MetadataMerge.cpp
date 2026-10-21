#include <algorithm>
#include <iterator>
#include "MetadataMerge.h"
#include <utility>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSet>

using namespace Qt::StringLiterals;

namespace yalahoo {
ProviderResult cachedMetadata(const Album &album, MetadataKind kind, const RequestToken &token)
{
    ProviderResult result;
    result.requestId = token.requestId;
    result.generation = token.generation;
    result.albumId = token.albumId;
    result.kind = kind;
    QJsonObject state;
    if (kind == MetadataKind::Introduction) {
        state = album.introduction;
        result.payload = { { u"description"_s, album.description }, { u"introduction"_s, state } };
        result.source = album.descriptionSource;
    } else if (kind == MetadataKind::Credits) {
        state = album.creditsLookup;
        result.payload = { { u"producers"_s, producerData(album.producers) },
                           { u"creditsLookup"_s, state } };
        result.source = { { u"name"_s, state.value(u"source"_s) },
                          { u"url"_s, state.value(u"sourceUrl"_s) } };
    } else {
        state = album.online;
        result.payload = { { u"online"_s, state },
                           { u"genres"_s, album.fields.value(u"_onlineGenres"_s) } };
        result.source = { { u"name"_s, u"MusicBrainz"_s },
                          { u"url"_s, state.value(u"sourceUrl"_s) } };
    }
    if (kind != MetadataKind::Introduction && !state.value(u"sourceDetails"_s).toObject().isEmpty())
        result.source = state.value(u"sourceDetails"_s).toObject();
    const auto status = state.value(u"status"_s).toString().toLower();
    static constexpr std::pair<QLatin1StringView, ProviderStatus> statuses[] = {
        { "matched"_L1, ProviderStatus::Matched },
        { "partial"_L1, ProviderStatus::Partial },
        { "not-found"_L1, ProviderStatus::NotFound },
        { "uncertain"_L1, ProviderStatus::Uncertain },
        { "error"_L1, ProviderStatus::Error }
    };
    const auto found =
            std::find_if(std::begin(statuses), std::end(statuses), [&](const auto &entry) {
                return entry.first == status;
            });
    if (found != std::end(statuses))
        result.status = found->second;
    else if (kind == MetadataKind::Introduction && !album.description.isEmpty())
        result.status = ProviderStatus::Matched;
    else if (kind == MetadataKind::Credits && !album.producers.isEmpty())
        result.status = ProviderStatus::Partial;
    result.checkedAt =
            state.value(u"checkedAt"_s).toString(result.source.value(u"checkedAt"_s).toString());
    result.retryAt = state.value(u"retryAt"_s).toString();
    result.error = state.value(u"error"_s).toString();
    return result;
}
QString metadataKey(const Album &album, MetadataKind kind)
{
    QJsonArray tracks;
    for (const auto &track : album.tracks)
        tracks.append(QJsonArray{ track.id, track.title, track.artist, track.duration,
                                  track.discNumber.value_or(1), track.trackNumber.value_or(0) });
    const auto bytes = QJsonDocument(QJsonArray{ kindName(kind), album.title, album.artist,
                                                 album.year.value_or(0), tracks })
                               .toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
namespace {
void mergeIntroduction(Album &album, const ProviderResult &result, const QJsonObject &state)
{
    album.introduction = state;
    const auto text = result.payload.value(u"description"_s).toString();
    // A failed refresh must not remove an existing introduction.
    if (!text.isEmpty() && result.status == ProviderStatus::Matched) {
        album.description = text;
        album.descriptionSource = result.source;
    }
}
void mergeCredits(Album &album, const ProviderResult &result, const QJsonObject &state)
{
    album.creditsLookup = state;
    if (result.status == ProviderStatus::Matched || result.status == ProviderStatus::Partial) {
        QList<Producer> producers;
        const auto incoming = result.payload.value(u"producers"_s).toArray();
        producers.reserve(album.producers.size() + incoming.size());
        for (const auto &person : std::as_const(album.producers))
            if (person.source != "MusicBrainz"_L1)
                producers.append(person);
        for (const auto &value : incoming) {
            const auto object = value.toObject();
            if (object.value(u"source"_s).toString() != "MusicBrainz"_L1)
                continue;
            Producer person;
            person.name = object.value(u"name"_s).toString();
            person.role = object.value(u"role"_s).toString();
            person.source = u"MusicBrainz"_s;
            person.trackId = object.value(u"trackId"_s).toString();
            person.trackTitle = object.value(u"trackTitle"_s).toString();
            person.url = object.value(u"url"_s).toString();
            if (!person.name.isEmpty() && !person.role.isEmpty())
                producers.append(std::move(person));
        }
        album.producers = std::move(producers);
    }
}
void mergeEnrichment(Album &album, const ProviderResult &result, const QJsonObject &state)
{
    album.online = state;
    if (result.status == ProviderStatus::Matched || result.status == ProviderStatus::Partial)
        album.fields.insert(u"_onlineGenres"_s, result.payload.value(u"genres"_s).toArray());
}
}
bool mergeMetadata(Album &album, const ProviderResult &result)
{
    if (album.id != result.albumId || result.status == ProviderStatus::Deferred
        || result.status == ProviderStatus::Cancelled)
        return false;
    const QString stateKey = result.kind == MetadataKind::Introduction ? u"introduction"_s
            : result.kind == MetadataKind::Credits                     ? u"creditsLookup"_s
                                                                       : u"online"_s;
    const auto state = result.payload.value(stateKey).toObject();
    if (state.isEmpty())
        return false;
    struct Merge final
    {
        MetadataKind kind;
        void (*apply)(Album &, const ProviderResult &, const QJsonObject &);
    };
    static constexpr Merge handlers[] = { { MetadataKind::Introduction, &mergeIntroduction },
                                          { MetadataKind::Credits, &mergeCredits },
                                          { MetadataKind::Enrichment, &mergeEnrichment } };
    const auto handler =
            std::find_if(std::begin(handlers), std::end(handlers), [&](const Merge &entry) {
                return entry.kind == result.kind;
            });
    if (handler == std::end(handlers))
        return false;
    handler->apply(album, result, state);
    return true;
}
}
