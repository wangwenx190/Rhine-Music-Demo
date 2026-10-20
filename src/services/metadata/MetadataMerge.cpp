#include "MetadataMerge.h"
#include <utility>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSet>

using namespace Qt::StringLiterals;

namespace yalahoo {
ProviderResult cachedMetadata(const Album &album, MetadataKind kind, RequestToken token)
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
    if (status == "matched"_L1)
        result.status = ProviderStatus::Matched;
    else if (status == "partial"_L1)
        result.status = ProviderStatus::Partial;
    else if (status == "not-found"_L1)
        result.status = ProviderStatus::NotFound;
    else if (status == "uncertain"_L1)
        result.status = ProviderStatus::Uncertain;
    else if (status == "error"_L1)
        result.status = ProviderStatus::Error;
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
    if (result.kind == MetadataKind::Introduction) {
        album.introduction = state;
        const auto text = result.payload.value(u"description"_s).toString();
        // A failed refresh must not remove an existing introduction.
        if (!text.isEmpty() && result.status == ProviderStatus::Matched) {
            album.description = text;
            album.descriptionSource = result.source;
        }
    } else if (result.kind == MetadataKind::Credits) {
        album.creditsLookup = state;
        if (result.status == ProviderStatus::Matched || result.status == ProviderStatus::Partial) {
            QList<Producer> producers;
            for (const auto &person : std::as_const(album.producers))
                if (person.source != "MusicBrainz"_L1)
                    producers.append(person);
            for (const auto &value : result.payload.value(u"producers"_s).toArray()) {
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
    } else {
        album.online = state;
        if (result.status == ProviderStatus::Matched || result.status == ProviderStatus::Partial)
            album.fields.insert(u"_onlineGenres"_s, result.payload.value(u"genres"_s).toArray());
    }
    return true;
}
}
