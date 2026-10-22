#include "Units.h"
#include "MusicBrainzSource.h"
#include <QRegularExpression>
#include <QSet>
#include <QHash>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Candidate matching requires authored score/duration tolerances and bounded remote lists.
constexpr auto MaximumReleaseCandidates = 10;
constexpr auto MinimumSearchScore = 95;
constexpr auto MaximumReleaseRecordings = 2000;
constexpr auto TrackDurationToleranceSeconds = 3;
constexpr auto MaximumGenres = 128;
constexpr auto MaximumCredits = 4096;

QString artistName(const QJsonObject &object)
{
    QString result;
    for (const auto &value : object.value(u"artist-credit"_s).toArray()) {
        const auto credit = value.toObject();
        auto name = credit.value(u"name"_s).toString();
        if (name.isEmpty())
            name = credit.value(u"artist"_s).toObject().value(u"name"_s).toString();
        result += name;
        result += credit.value(u"joinphrase"_s).toString();
    }
    return result;
}
bool basicMatch(const Album &album, const QJsonObject &release, const QString &albumTitle,
                const QString &albumArtist)
{
    const auto date = release.value(u"date"_s).toString();
    return metadataName(release.value(u"title"_s).toString()) == albumTitle
            && metadataName(artistName(release)) == albumArtist
            && (!album.year || date.isEmpty() || date.startsWith(QString::number(*album.year)));
}

auto collectMusicBrainzCredits(const QSet<QString> &roles, QJsonArray &credits, QSet<QString> &seen,
                               const QJsonObject &object, const Track *track)
{
    for (const auto &value : object.value(u"relations"_s).toArray()) {
        if (credits.size() >= MaximumCredits)
            return;
        const auto relation = value.toObject();
        const auto artist = relation.value(u"artist"_s).toObject();
        const auto role = relation.value(u"type"_s).toString();
        const auto creditField = relation.value(u"direction"_s) == "backward"_L1
                ? u"source-credit"_s
                : u"target-credit"_s;
        auto name = relation.value(creditField).toString();
        if (name.isEmpty())
            name = artist.value(u"name"_s).toString();
        if (name.isEmpty() || !roles.contains(role))
            continue;
        const auto attributes = relation.value(u"attributes"_s).toArray();
        QStringList labels;
        labels.reserve(attributes.size());
        for (const auto &attribute : attributes)
            labels.append(attribute.toString());
        const auto label = labels.isEmpty() ? role : role + u" ("_s + labels.join(u", "_s) + ')'_L1;
        const auto key = name + '\n'_L1 + label + '\n'_L1 + (track ? track->id : QString());
        const auto count = seen.size();
        seen.insert(key);
        if (seen.size() == count)
            continue;
        credits.append(QJsonObject{
                { u"name"_s, name },
                { u"role"_s, label },
                { u"source"_s, u"MusicBrainz"_s },
                { u"trackId"_s, track ? track->id : QString() },
                { u"trackTitle"_s, track ? track->title : QString() },
                { u"url"_s,
                  u"https://musicbrainz.org/artist/"_s + artist.value(u"id"_s).toString() } });
    }
}
}
QString metadataName(const QString &text)
{
    auto result = text.normalized(QString::NormalizationForm_KC).toCaseFolded();
    static const QRegularExpression punctuation(u"[\\s\\p{P}\\p{S}]"_s);
    result.remove(punctuation);
    return result;
}
QJsonArray musicBrainzCandidates(const Album &album, const QJsonObject &search)
{
    QJsonArray candidates;
    QSet<QString> seen;
    const auto albumTitle = metadataName(album.title);
    const auto albumArtist = metadataName(album.artist);
    for (const auto &value : search.value(u"releases"_s).toArray()) {
        if (candidates.size() >= MaximumReleaseCandidates)
            break;
        const auto candidate = value.toObject();
        const auto id = candidate.value(u"id"_s).toString();
        if (basicMatch(album, candidate, albumTitle, albumArtist)
            && candidate.value(u"score"_s).toVariant().toInt() >= MinimumSearchScore
            && (album.tracks.isEmpty()
                || candidate.value(u"track-count"_s).toInt(-1) == album.tracks.size())) {
            const auto count = seen.size();
            seen.insert(id);
            if (seen.size() == count)
                continue;
            const auto labelInfo = candidate.value(u"label-info"_s).toArray();
            QStringList labels;
            labels.reserve(labelInfo.size() * 2);
            for (const auto &label : labelInfo) {
                const auto info = label.toObject();
                labels.append(info.value(u"label"_s).toObject().value(u"name"_s).toString());
                labels.append(info.value(u"catalog-number"_s).toString());
            }
            labels.removeAll(QString());
            candidates.append(QJsonObject{ { u"id"_s, id },
                                           { u"title"_s, candidate.value(u"title"_s) },
                                           { u"date"_s, candidate.value(u"date"_s) },
                                           { u"country"_s, candidate.value(u"country"_s) },
                                           { u"label"_s, labels.join(u" / "_s) } });
        }
    }
    return candidates;
}
// Require a plausible album identity and compatible track durations before accepting a release.
// Search rank alone cannot establish that two editions contain the same recordings.
QString musicBrainzRelease(const Album &album, const QJsonObject &search)
{
    const auto candidates = musicBrainzCandidates(album, search);
    // A truncated search cannot prove global uniqueness.
    if (search.value(u"count"_s).toInteger() > search.value(u"releases"_s).toArray().size())
        return { };
    return candidates.size() == 1 ? candidates.first().toObject().value(u"id"_s).toString()
                                  : QString();
}

ProviderResult musicBrainzMetadata(const Album &album, MetadataKind kind,
                                   const QJsonObject &release)
{
    ProviderResult result;
    result.kind = kind;
    result.status = ProviderStatus::Uncertain;
    if (!basicMatch(album, release, metadataName(album.title), metadataName(album.artist)))
        return result;
    struct Recording final
    {
        QJsonObject object;
        int disc = 0, position = 0;
        double duration = 0;
    };
    const auto media = release.value(u"media"_s).toArray();
    qsizetype recordingCount = 0;
    for (const auto &value : media) {
        const auto count = value.toObject().value(u"tracks"_s).toArray().size();
        if (count > MaximumReleaseRecordings - recordingCount)
            return result;
        recordingCount += count;
    }
    if (!album.tracks.isEmpty() && recordingCount != album.tracks.size())
        return result;
    QList<Recording> recordings;
    recordings.reserve(recordingCount);
    // Normalize each remote title once and restrict matching to its title bucket.
    // Repeated titles still require the original disc/position/duration uniqueness checks.
    QHash<QString, QList<qsizetype>> recordingsByTitle;
    recordingsByTitle.reserve(recordingCount);
    for (const auto &mediumValue : media) {
        const auto medium = mediumValue.toObject();
        for (const auto &trackValue : medium.value(u"tracks"_s).toArray()) {
            const auto track = trackValue.toObject();
            const auto recording = track.value(u"recording"_s).toObject();
            recordings.append(
                    { recording, medium.value(u"position"_s).toInt(),
                      track.value(u"position"_s).toInt(),
                      track.value(u"length"_s).toDouble(recording.value(u"length"_s).toDouble())
                              / yalahoo::units::MillisecondsPerSecond });
            const auto title =
                    track.value(u"title"_s).toString(recording.value(u"title"_s).toString());
            recordingsByTitle[metadataName(title)].append(recordings.size() - 1);
        }
    }
    QList<qsizetype> matches;
    matches.reserve(album.tracks.size());
    QList<bool> used(recordings.size(), false);
    for (const auto &track : album.tracks) {
        const auto candidates = recordingsByTitle.constFind(metadataName(track.title));
        if (candidates == recordingsByTitle.cend())
            return result;
        qsizetype match = -1;
        for (const qsizetype i : *candidates) {
            const auto &remote = recordings[i];
            if (used[i] || (track.discNumber && *track.discNumber != remote.disc)
                || (track.trackNumber && *track.trackNumber != remote.position)
                || (track.duration > 0
                    && (remote.duration <= 0
                        || std::abs(track.duration - remote.duration)
                                > TrackDurationToleranceSeconds)))
                continue;
            if (match >= 0)
                return result;
            match = i;
        }
        if (match < 0)
            return result;
        matches.append(match);
        used[match] = true;
    }
    const auto releaseId = release.value(u"id"_s).toString();
    result.source = { { u"name"_s, u"MusicBrainz"_s },
                      { u"url"_s, u"https://musicbrainz.org/release/"_s + releaseId },
                      { u"license"_s, u"Core: CC0; supplementary: CC BY-NC-SA 3.0"_s },
                      { u"licenseUrl"_s, u"https://musicbrainz.org/doc/About/Data_License"_s } };
    QJsonArray genres;
    QSet<QString> seenGenres;
    const auto collectGenres = [&](const QJsonObject &object) {
        for (const auto &value : object.value(u"genres"_s).toArray()) {
            if (genres.size() >= MaximumGenres)
                return;
            const auto name = value.toObject().value(u"name"_s).toString();
            if (!name.isEmpty()) {
                const auto count = seenGenres.size();
                seenGenres.insert(name);
                if (seenGenres.size() != count)
                    genres.append(name);
            }
        }
    };
    collectGenres(release);
    collectGenres(release.value(u"release-group"_s).toObject());
    result.payload = { { u"online"_s,
                         QJsonObject{
                                 { u"releaseId"_s, releaseId },
                                 { u"releaseGroupId"_s,
                                   release.value(u"release-group"_s).toObject().value(u"id"_s) },
                                 { u"sourceUrl"_s, result.source.value(u"url"_s) },
                                 { u"title"_s, release.value(u"title"_s) },
                                 { u"artist"_s, artistName(release) } } },
                       { u"genres"_s, genres } };
    if (kind == MetadataKind::Enrichment) {
        result.status = genres.isEmpty() ? ProviderStatus::Partial : ProviderStatus::Matched;
        return result;
    }
    QJsonArray credits;
    QSet<QString> seen;
    static const QSet<QString> roles{ u"composer"_s,
                                      u"lyricist"_s,
                                      u"writer"_s,
                                      u"arranger"_s,
                                      u"instrument arranger"_s,
                                      u"vocal arranger"_s,
                                      u"orchestrator"_s,
                                      u"producer"_s,
                                      u"executive producer"_s,
                                      u"co-producer"_s,
                                      u"mix"_s,
                                      u"mixing"_s,
                                      u"mastering"_s,
                                      u"engineer"_s,
                                      u"recording"_s,
                                      u"audio"_s,
                                      u"sound"_s,
                                      u"programming"_s,
                                      u"editor"_s,
                                      u"performer"_s,
                                      u"instrument"_s,
                                      u"vocal"_s,
                                      u"conductor"_s,
                                      u"orchestra"_s,
                                      u"chorus master"_s,
                                      u"concertmaster"_s,
                                      u"remixer"_s };
    const auto collect = [&](const QJsonObject &object, const Track *track) {
        return collectMusicBrainzCredits(roles, credits, seen, object, track);
    };
    collect(release, nullptr);
    collect(release.value(u"release-group"_s).toObject(), nullptr);
    int tracksWithCredits = 0;
    for (qsizetype i = 0; i < matches.size(); ++i) {
        const auto &recording = recordings[matches[i]].object;
        const auto before = credits.size();
        collect(recording, &album.tracks[i]);
        for (const auto &value : recording.value(u"relations"_s).toArray()) {
            const auto relation = value.toObject();
            if (relation.value(u"type"_s) == "performance"_L1)
                collect(relation.value(u"work"_s).toObject(), &album.tracks[i]);
        }
        if (credits.size() > before)
            ++tracksWithCredits;
    }
    // Even populated credits cannot prove that every role on the release was documented.
    result.status = credits.isEmpty() ? ProviderStatus::NotFound : ProviderStatus::Partial;
    result.payload.insert(u"producers"_s, credits);
    result.payload.insert(u"creditsLookup"_s,
                          QJsonObject{ { u"source"_s, u"MusicBrainz"_s },
                                       { u"matchedTracks"_s, tracksWithCredits },
                                       { u"totalTracks"_s, album.tracks.size() },
                                       { u"releaseId"_s, releaseId } });
    return result;
}
}
