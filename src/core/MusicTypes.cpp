#include "MusicTypes.h"

#include <QDir>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QTextBoundaryFinder>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

using namespace Qt::StringLiterals;

namespace yalahoo {
QString graphemePrefix(const QString &text, qsizetype maxCodeUnits)
{
    if (maxCodeUnits <= 0)
        return { };
    if (text.size() <= maxCodeUnits)
        return text;
    // A complete following scalar is sufficient to classify the cut boundary.
    // Do not allocate boundary attributes for an arbitrarily long metadata tail.
    const auto examined = maxCodeUnits + qMin(qsizetype(2), text.size() - maxCodeUnits);
    QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme, text.constData(), examined);
    if (!boundary.isValid())
        return { };
    boundary.setPosition(maxCodeUnits);
    if (!boundary.isAtBoundary())
        boundary.toPreviousBoundary();
    return text.left(qMax(qsizetype(0), boundary.position()));
}
AlbumTitleParts albumTitleParts(const QString &text)
{
    static const QRegularExpression pattern(u"^(.+?)[\uff08(]([^\uff08\uff09()]+)[\uff09)]\\s*$"_s);
    static const QRegularExpression han(u"\\p{Han}"_s);
    static const QRegularExpression latin(u"[a-z]"_s, QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text.trimmed());
    if (match.hasMatch() && match.captured(1).contains(han) && match.captured(2).contains(latin)
        && !match.captured(2).contains(han))
        return { match.captured(1).trimmed(), match.captured(2).trimmed() };
    return { text, { } };
}

namespace {
QJsonObject extensionFields(const QJsonObject &object,
                            std::initializer_list<QLatin1StringView> known)
{
    // Build only the extension object. Removing keys from a copy can retain the CBOR
    // backing storage of large, now-obsolete typed metadata after a later refresh.
    QJsonObject result;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        if (std::find(known.begin(), known.end(), it.keyView()) == known.end())
            result.insert(it.key(), it.value());
    return result;
}
bool validFields(const QJsonObject &json, QJsonValue::Type type,
                 std::initializer_list<QLatin1StringView> keys)
{
    for (const auto key : keys) {
        const auto value = json.value(key);
        if (!value.isUndefined() && !value.isNull() && value.type() != type)
            return false;
    }
    return true;
}
bool validStringArray(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull())
        return true;
    if (!value.isArray())
        return false;
    for (const auto &entry : value.toArray())
        if (!entry.isString())
            return false;
    return true;
}
bool validIntegers(const QJsonObject &json, std::initializer_list<QLatin1StringView> keys)
{
    for (const auto key : keys) {
        const auto value = json.value(key);
        if (value.isUndefined() || value.isNull())
            continue;
        if (!value.isDouble())
            return false;
        const double number = value.toDouble();
        if (!std::isfinite(number) || std::trunc(number) != number
            || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
            return false;
    }
    return true;
}
std::optional<int> integer(const QJsonObject &json, QLatin1StringView key)
{
    const auto value = json.value(key);
    return value.isDouble() ? std::optional(value.toInt()) : std::nullopt;
}
QUrl localUrl(const QString &value)
{
    if (value.isEmpty() || value.contains(QChar::Null))
        return { };
    const QUrl url(value);
    if (url.isLocalFile()) {
        const auto path = url.toLocalFile();
        return url.isValid() && !url.hasQuery() && !url.hasFragment() && !path.contains(QChar::Null)
                        && QDir::isAbsolutePath(path)
                ? url
                : QUrl{ };
    }
    return QDir::isAbsolutePath(value) && !value.startsWith("//"_L1) ? QUrl::fromLocalFile(value)
                                                                     : QUrl{ };
}
QStringList strings(const QJsonArray &array)
{
    QStringList result;
    result.reserve(array.size());
    for (const auto &value : array)
        result.append(value.toString());
    return result;
}
void put(QJsonObject &json, QLatin1StringView key, std::optional<int> value)
{
    if (value)
        json.insert(key, *value);
    else
        json.remove(key);
}
void putObject(QJsonObject &json, QLatin1StringView key, const QJsonObject &value)
{
    if (!value.isEmpty())
        json.insert(key, value);
    else
        json.remove(key);
}
}
const Album *LibrarySnapshot::album(const QString &id) const
{
    for (const auto &value : albums)
        if (value.id == id)
            return &value;
    return nullptr;
}
const Track *LibrarySnapshot::track(const QString &albumId, const QString &trackId) const
{
    if (const auto *value = album(albumId))
        for (const auto &track : value->tracks)
            if (track.id == trackId)
                return &track;
    return nullptr;
}
QString supportName(MediaSupport support)
{
    switch (support) {
    case MediaSupport::Supported:
        return u"Supported"_s;
    case MediaSupport::Unsupported:
        return u"Unsupported"_s;
    default:
        return u"Unknown"_s;
    }
}
bool readLibrary(const QJsonObject &json, LibrarySnapshot &output, QString *error)
{
    auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (!json.value(u"albums"_s).isArray() || !json.value(u"genres"_s).isArray())
        return fail(u"Library requires albums and genres arrays"_s);
    if (json.contains(u"roots"_s) && !json.value(u"roots"_s).isArray())
        return fail(u"Library roots must be an array"_s);
    if (!validFields(json, QJsonValue::Object,
                     { "scan"_L1, "enrich"_L1, "introductions"_L1, "credits"_L1 })
        || !validFields(json, QJsonValue::Bool, { "onlineEnabled"_L1 }))
        return fail(u"Invalid library metadata types"_s);
    LibrarySnapshot library;
    library.fields = extensionFields(json,
                                     { "albums"_L1, "genres"_L1, "roots"_L1, "scan"_L1, "enrich"_L1,
                                       "introductions"_L1, "credits"_L1, "onlineEnabled"_L1 });
    library.scan = json.value(u"scan"_s).toObject();
    library.enrich = json.value(u"enrich"_s).toObject();
    library.introductions = json.value(u"introductions"_s).toObject();
    library.credits = json.value(u"credits"_s).toObject();
    library.onlineEnabled = json.value(u"onlineEnabled"_s).toBool();
    QSet<QString> albumIds, trackIds, genreIds;
    const auto genres = json.value(u"genres"_s).toArray();
    const auto albums = json.value(u"albums"_s).toArray();
    const auto roots = json.value(u"roots"_s).toArray();
    library.genres.reserve(genres.size());
    library.albums.reserve(albums.size());
    library.roots.reserve(roots.size());
    genreIds.reserve(genres.size());
    albumIds.reserve(albums.size());
    qsizetype trackCount = 0;
    for (const auto &value : albums)
        trackCount += value.toObject().value(u"tracks"_s).toArray().size();
    trackIds.reserve(trackCount);
    for (const auto &value : genres) {
        const auto object = value.toObject();
        if (!value.isObject() || !validFields(object, QJsonValue::String, { "id"_L1, "name"_L1 })
            || !validStringArray(object.value("aliases"_L1)))
            return fail(u"Invalid genre metadata types"_s);
        if (!validIntegers(object, { "albumCount"_L1 }))
            return fail(QCoreApplication::translate("Yalahoo", "Invalid genre integer metadata"));
        Genre genre{
            object.value(u"id"_s).toString(), object.value(u"name"_s).toString(),
            strings(object.value(u"aliases"_s).toArray()), integer(object, "albumCount"_L1),
            extensionFields(object, { "id"_L1, "name"_L1, "aliases"_L1, "albumCount"_L1 })
        };
        const auto count = genreIds.size();
        genreIds.insert(genre.id);
        if (genre.id.isEmpty() || genreIds.size() == count)
            return fail(u"Invalid or duplicate genre ID"_s);
        library.genres.append(std::move(genre));
    }
    for (const auto &value : albums) {
        const auto object = value.toObject();
        Album album;
        if (!value.isObject()
            || !validFields(object, QJsonValue::String,
                            { "id"_L1, "title"_L1, "artist"_L1, "genreId"_L1, "description"_L1,
                              "localNote"_L1, "folder"_L1, "folderUrl"_L1, "coverUrl"_L1 })
            || !validFields(
                    object, QJsonValue::Object,
                    { "descriptionSource"_L1, "introduction"_L1, "creditsLookup"_L1, "online"_L1 })
            || !validFields(object, QJsonValue::Bool, { "offline"_L1 })
            || !validStringArray(object.value("rawGenres"_L1)))
            return fail(u"Invalid album metadata types"_s);
        if (!object.value(u"tracks"_s).isArray()
            || (object.contains(u"producers"_s) && !object.value(u"producers"_s).isArray()))
            return fail(u"Album tracks/producers must be arrays"_s);
        album.fields = extensionFields(
                object,
                { "id"_L1, "title"_L1, "artist"_L1, "genreId"_L1, "description"_L1, "localNote"_L1,
                  "folder"_L1, "folderUrl"_L1, "coverUrl"_L1, "year"_L1, "discCount"_L1,
                  "rawGenres"_L1, "descriptionSource"_L1, "introduction"_L1, "creditsLookup"_L1,
                  "online"_L1, "offline"_L1, "tracks"_L1, "producers"_L1 });
        album.id = object.value(u"id"_s).toString();
        const auto count = albumIds.size();
        albumIds.insert(album.id);
        if (album.id.isEmpty() || albumIds.size() == count)
            return fail(u"Invalid or duplicate album ID"_s);
        album.title = object.value(u"title"_s).toString();
        album.artist = object.value(u"artist"_s).toString();
        album.genreId = object.value(u"genreId"_s).toString();
        if (!validIntegers(object, { "year"_L1, "discCount"_L1 }))
            return fail(QCoreApplication::translate("Yalahoo", "Invalid album integer metadata"));
        album.year = integer(object, "year"_L1);
        album.discCount = integer(object, "discCount"_L1);
        album.rawGenres = strings(object.value(u"rawGenres"_s).toArray());
        album.description = object.value(u"description"_s).toString();
        album.localNote = object.value(u"localNote"_s).toString();
        album.descriptionSource = object.value(u"descriptionSource"_s).toObject();
        album.introduction = object.value(u"introduction"_s).toObject();
        album.creditsLookup = object.value(u"creditsLookup"_s).toObject();
        album.online = object.value(u"online"_s).toObject();
        album.offline = object.value(u"offline"_s).toBool();
        album.folderLabel = object.value(u"folder"_s).toString();
        album.folder = localUrl(object.value(u"folderUrl"_s).toString(album.folderLabel));
        const QString cover = object.value(u"coverUrl"_s).toString();
        album.coverUrl = cover.startsWith("qrc:/"_L1) ? QUrl(cover) : localUrl(cover);
        const auto tracks = object.value(u"tracks"_s).toArray();
        album.tracks.reserve(tracks.size());
        for (const auto &entry : tracks) {
            const auto trackObject = entry.toObject();
            Track track;
            if (!entry.isObject()
                || !validFields(trackObject, QJsonValue::String,
                                { "id"_L1, "albumId"_L1, "title"_L1, "artist"_L1, "format"_L1,
                                  "codec"_L1, "relativePath"_L1, "sourceUrl"_L1,
                                  "mediaSupport"_L1 })
                || !validFields(trackObject, QJsonValue::Bool, { "lossless"_L1 }))
                return fail(u"Invalid track metadata types"_s);
            track.fields = extensionFields(trackObject,
                                           { "id"_L1, "albumId"_L1, "title"_L1, "artist"_L1,
                                             "format"_L1, "codec"_L1, "relativePath"_L1,
                                             "duration"_L1, "sourceUrl"_L1, "mediaSupport"_L1,
                                             "trackNumber"_L1, "discNumber"_L1, "bitsPerSample"_L1,
                                             "sampleRate"_L1, "bitrate"_L1, "lossless"_L1 });
            track.id = trackObject.value(u"id"_s).toString();
            track.albumId = trackObject.value(u"albumId"_s).toString(album.id);
            const auto count = trackIds.size();
            trackIds.insert(track.id);
            if (track.id.isEmpty() || trackIds.size() == count || track.albumId != album.id)
                return fail(u"Invalid, duplicate or mismatched track ID"_s);
            track.title = trackObject.value(u"title"_s).toString();
            track.artist = trackObject.value(u"artist"_s).toString();
            track.format = trackObject.value(u"format"_s).toString();
            track.codec = trackObject.value(u"codec"_s).toString();
            track.relativePath = trackObject.value(u"relativePath"_s).toString();
            if (!validIntegers(trackObject,
                               { "trackNumber"_L1, "discNumber"_L1, "bitsPerSample"_L1,
                                 "sampleRate"_L1, "bitrate"_L1 }))
                return fail(
                        QCoreApplication::translate("Yalahoo", "Invalid track integer metadata"));
            const auto duration = trackObject.value(u"duration"_s);
            if (!duration.isUndefined() && !duration.isNull()
                && (!duration.isDouble() || !std::isfinite(duration.toDouble())
                    || duration.toDouble() < 0))
                return fail(QCoreApplication::translate("Yalahoo", "Invalid track duration"));
            track.duration = duration.toDouble();
            track.trackNumber = integer(trackObject, "trackNumber"_L1);
            track.discNumber = integer(trackObject, "discNumber"_L1);
            track.bitsPerSample = integer(trackObject, "bitsPerSample"_L1);
            track.sampleRate = integer(trackObject, "sampleRate"_L1);
            track.bitrate = integer(trackObject, "bitrate"_L1);
            if (trackObject.value(u"lossless"_s).isBool())
                track.lossless = trackObject.value(u"lossless"_s).toBool();
            track.sourceUrl = localUrl(trackObject.value(u"sourceUrl"_s).toString());
            const QString support = trackObject.value(u"mediaSupport"_s).toString();
            if (support == "Supported"_L1)
                track.mediaSupport = MediaSupport::Supported;
            else if (support == "Unsupported"_L1)
                track.mediaSupport = MediaSupport::Unsupported;
            album.tracks.append(std::move(track));
        }
        const auto producers = object.value(u"producers"_s).toArray();
        album.producers.reserve(producers.size());
        for (const auto &entry : producers) {
            const auto p = entry.toObject();
            if (!entry.isObject()
                || !validFields(p, QJsonValue::String,
                                { "name"_L1, "role"_L1, "source"_L1, "trackId"_L1, "trackTitle"_L1,
                                  "url"_L1 }))
                return fail(u"Invalid producer metadata types"_s);
            album.producers.append(
                    { p.value(u"name"_s).toString(), p.value(u"role"_s).toString(),
                      p.value(u"source"_s).toString(), p.value(u"trackId"_s).toString(),
                      p.value(u"trackTitle"_s).toString(), p.value(u"url"_s).toString(),
                      extensionFields(p,
                                      { "name"_L1, "role"_L1, "source"_L1, "trackId"_L1,
                                        "trackTitle"_L1, "url"_L1 }) });
        }
        library.albums.append(std::move(album));
    }
    for (const auto &entry : roots) {
        const auto r = entry.toObject();
        if (!entry.isObject()
            || !validFields(r, QJsonValue::String, { "path"_L1, "status"_L1, "error"_L1 }))
            return fail(u"Invalid root metadata types"_s);
        library.roots.append({ localUrl(r.value(u"path"_s).toString()),
                               r.value(u"status"_s).toString(), r.value(u"error"_s).toString(),
                               r });
    }
    output = std::move(library);
    if (error)
        error->clear();
    return true;
}
QJsonObject toJson(const Track &track)
{
    QJsonObject json = track.fields;
    json.insert(u"id"_s, track.id);
    json.insert(u"albumId"_s, track.albumId);
    json.insert(u"title"_s, track.title);
    json.insert(u"artist"_s, track.artist);
    json.insert(u"format"_s, track.format);
    json.insert(u"codec"_s, track.codec);
    json.insert(u"relativePath"_s, track.relativePath);
    json.insert(u"duration"_s, track.duration);
    json.insert(u"sourceUrl"_s, track.sourceUrl.toString());
    json.insert(u"mediaSupport"_s, supportName(track.mediaSupport));
    put(json, "trackNumber"_L1, track.trackNumber);
    put(json, "discNumber"_L1, track.discNumber);
    put(json, "bitsPerSample"_L1, track.bitsPerSample);
    put(json, "sampleRate"_L1, track.sampleRate);
    put(json, "bitrate"_L1, track.bitrate);
    if (track.lossless)
        json.insert(u"lossless"_s, *track.lossless);
    else
        json.remove(u"lossless"_s);
    return json;
}
QJsonArray producerData(const QList<Producer> &entries)
{
    QJsonArray result;
    for (const auto &p : entries) {
        auto object = p.fields;
        object.insert(u"name"_s, p.name);
        object.insert(u"role"_s, p.role);
        object.insert(u"source"_s, p.source);
        object.insert(u"trackId"_s, p.trackId);
        object.insert(u"trackTitle"_s, p.trackTitle);
        object.insert(u"url"_s, p.url);
        result.append(std::move(object));
    }
    return result;
}
QJsonObject toJson(const Album &album)
{
    QJsonObject json = album.fields;
    json.insert(u"id"_s, album.id);
    json.insert(u"title"_s, album.title);
    json.insert(u"artist"_s, album.artist);
    json.insert(u"genreId"_s, album.genreId);
    json.insert(u"description"_s, album.description);
    json.insert(u"localNote"_s, album.localNote);
    json.insert(u"folder"_s, album.folderLabel);
    json.insert(u"folderUrl"_s, album.folder.toString());
    if (!album.coverUrl.isEmpty())
        json.insert(u"coverUrl"_s, album.coverUrl.toString());
    else
        json.remove(u"coverUrl"_s);
    json.insert(u"offline"_s, album.offline);
    json.insert(u"rawGenres"_s, QJsonArray::fromStringList(album.rawGenres));
    put(json, "year"_L1, album.year);
    put(json, "discCount"_L1, album.discCount);
    putObject(json, "descriptionSource"_L1, album.descriptionSource);
    putObject(json, "introduction"_L1, album.introduction);
    putObject(json, "creditsLookup"_L1, album.creditsLookup);
    putObject(json, "online"_L1, album.online);
    QJsonArray tracks;
    for (const auto &track : album.tracks)
        tracks.append(toJson(track));
    json.insert(u"tracks"_s, tracks);
    json.insert(u"producers"_s, producerData(album.producers));
    return json;
}
QJsonObject toJson(const LibrarySnapshot &library)
{
    auto json = library.fields;
    QJsonArray albums, genres, roots;
    for (const auto &album : library.albums)
        albums.append(toJson(album));
    for (const auto &g : library.genres) {
        auto object = g.fields;
        object.insert(u"id"_s, g.id);
        object.insert(u"name"_s, g.name);
        object.insert(u"aliases"_s, QJsonArray::fromStringList(g.aliases));
        put(object, "albumCount"_L1, g.albumCount);
        genres.append(std::move(object));
    }
    for (const auto &r : library.roots) {
        auto object = r.fields;
        if (!r.path.isEmpty())
            object.insert(u"path"_s, r.path.toLocalFile());
        object.insert(u"status"_s, r.status);
        object.insert(u"error"_s, r.error);
        roots.append(std::move(object));
    }
    json.insert(u"albums"_s, albums);
    json.insert(u"genres"_s, genres);
    json.insert(u"roots"_s, roots);
    json.insert(u"scan"_s, library.scan);
    json.insert(u"onlineEnabled"_s, library.onlineEnabled);
    putObject(json, "enrich"_L1, library.enrich);
    putObject(json, "introductions"_L1, library.introductions);
    putObject(json, "credits"_L1, library.credits);
    return json;
}
}
