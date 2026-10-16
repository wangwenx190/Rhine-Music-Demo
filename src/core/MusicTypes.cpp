#include "MusicTypes.h"

#include <QDir>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>

namespace yalahoo {
QPair<QString, QString> albumTitleParts(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^(.+?)[（(]([^（）()]+)[）)]\\s*$"));
    static const QRegularExpression han(QStringLiteral("\\p{Han}"));
    static const QRegularExpression latin(QStringLiteral("[a-z]"),
                                          QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text.trimmed());
    if (match.hasMatch() && match.captured(1).contains(han) && match.captured(2).contains(latin)
        && !match.captured(2).contains(han))
        return { match.captured(1).trimmed(), match.captured(2).trimmed() };
    return { text, { } };
}

namespace {
std::optional<int> integer(const QJsonObject &json, const char *key)
{
    return json.value(QLatin1String(key)).isDouble()
            ? std::optional(json.value(QLatin1String(key)).toInt())
            : std::nullopt;
}
QUrl localUrl(const QString &value)
{
    if (value.isEmpty())
        return { };
    const QUrl url(value);
    if (url.isLocalFile())
        return url;
    return QDir::isAbsolutePath(value) && !value.startsWith(QLatin1String("//"))
            ? QUrl::fromLocalFile(value)
            : QUrl{ };
}
QStringList strings(const QJsonArray &array)
{
    QStringList result;
    for (const auto &value : array)
        result.append(value.toString());
    return result;
}
void put(QJsonObject &json, const char *key, std::optional<int> value)
{
    if (value)
        json.insert(QLatin1String(key), *value);
    else
        json.remove(QLatin1String(key));
}
void putObject(QJsonObject &json, const char *key, const QJsonObject &value)
{
    if (!value.isEmpty())
        json.insert(QLatin1String(key), value);
    else
        json.remove(QLatin1String(key));
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
        return QStringLiteral("Supported");
    case MediaSupport::Unsupported:
        return QStringLiteral("Unsupported");
    default:
        return QStringLiteral("Unknown");
    }
}
bool readLibrary(const QJsonObject &json, LibrarySnapshot &output, QString *error)
{
    auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (!json.value("albums").isArray() || !json.value("genres").isArray())
        return fail(QStringLiteral("Library requires albums and genres arrays"));
    if (json.contains("roots") && !json.value("roots").isArray())
        return fail(QStringLiteral("Library roots must be an array"));
    LibrarySnapshot library;
    library.fields = json;
    for (const auto &key : { "albums", "genres", "roots" })
        library.fields.remove(key);
    library.scan = json.value("scan").toObject();
    library.enrich = json.value("enrich").toObject();
    library.introductions = json.value("introductions").toObject();
    library.credits = json.value("credits").toObject();
    library.onlineEnabled = json.value("onlineEnabled").toBool();
    QSet<QString> albumIds, trackIds, genreIds;
    for (const auto &value : json.value("genres").toArray()) {
        const auto object = value.toObject();
        Genre genre{ object.value("id").toString(), object.value("name").toString(),
                     strings(object.value("aliases").toArray()), integer(object, "albumCount"),
                     object };
        if (genre.id.isEmpty() || genreIds.contains(genre.id))
            return fail(QStringLiteral("Invalid or duplicate genre ID"));
        genreIds.insert(genre.id);
        library.genres.append(genre);
    }
    for (const auto &value : json.value("albums").toArray()) {
        const auto object = value.toObject();
        Album album;
        if (!object.value("tracks").isArray()
            || (object.contains("producers") && !object.value("producers").isArray()))
            return fail(QStringLiteral("Album tracks/producers must be arrays"));
        album.fields = object;
        album.fields.remove("tracks");
        album.fields.remove("producers");
        album.id = object.value("id").toString();
        if (album.id.isEmpty() || albumIds.contains(album.id))
            return fail(QStringLiteral("Invalid or duplicate album ID"));
        albumIds.insert(album.id);
        album.title = object.value("title").toString();
        album.artist = object.value("artist").toString();
        album.genreId = object.value("genreId").toString();
        album.year = integer(object, "year");
        album.discCount = integer(object, "discCount");
        album.rawGenres = strings(object.value("rawGenres").toArray());
        album.description = object.value("description").toString();
        album.localNote = object.value("localNote").toString();
        album.descriptionSource = object.value("descriptionSource").toObject();
        album.introduction = object.value("introduction").toObject();
        album.creditsLookup = object.value("creditsLookup").toObject();
        album.online = object.value("online").toObject();
        album.offline = object.value("offline").toBool();
        album.folderLabel = object.value("folder").toString();
        album.folder = localUrl(object.value("folderUrl").toString(album.folderLabel));
        const QString cover = object.value("coverUrl").toString();
        album.coverUrl = cover.startsWith(QLatin1String("qrc:/")) ? QUrl(cover) : localUrl(cover);
        for (const auto &entry : object.value("tracks").toArray()) {
            const auto trackObject = entry.toObject();
            Track track;
            track.fields = trackObject;
            track.id = trackObject.value("id").toString();
            track.albumId = trackObject.value("albumId").toString(album.id);
            if (track.id.isEmpty() || trackIds.contains(track.id) || track.albumId != album.id)
                return fail(QStringLiteral("Invalid, duplicate or mismatched track ID"));
            trackIds.insert(track.id);
            track.title = trackObject.value("title").toString();
            track.artist = trackObject.value("artist").toString();
            track.format = trackObject.value("format").toString();
            track.codec = trackObject.value("codec").toString();
            track.relativePath = trackObject.value("relativePath").toString();
            track.duration = trackObject.value("duration").toDouble();
            track.trackNumber = integer(trackObject, "trackNumber");
            track.discNumber = integer(trackObject, "discNumber");
            track.bitsPerSample = integer(trackObject, "bitsPerSample");
            track.sampleRate = integer(trackObject, "sampleRate");
            track.bitrate = integer(trackObject, "bitrate");
            if (trackObject.value("lossless").isBool())
                track.lossless = trackObject.value("lossless").toBool();
            track.sourceUrl = localUrl(trackObject.value("sourceUrl").toString());
            const QString support = trackObject.value("mediaSupport").toString();
            if (support == QLatin1String("Supported"))
                track.mediaSupport = MediaSupport::Supported;
            else if (support == QLatin1String("Unsupported"))
                track.mediaSupport = MediaSupport::Unsupported;
            album.tracks.append(track);
        }
        for (const auto &entry : object.value("producers").toArray()) {
            const auto p = entry.toObject();
            album.producers.append({ p.value("name").toString(), p.value("role").toString(),
                                     p.value("source").toString(), p.value("trackId").toString(),
                                     p.value("trackTitle").toString(), p.value("url").toString(),
                                     p });
        }
        library.albums.append(album);
    }
    for (const auto &entry : json.value("roots").toArray()) {
        const auto r = entry.toObject();
        library.roots.append({ localUrl(r.value("path").toString()), r.value("status").toString(),
                               r.value("error").toString(), r });
    }
    output = std::move(library);
    if (error)
        error->clear();
    return true;
}
QJsonObject toJson(const Track &track)
{
    QJsonObject json = track.fields;
    json.insert("id", track.id);
    json.insert("albumId", track.albumId);
    json.insert("title", track.title);
    json.insert("artist", track.artist);
    json.insert("format", track.format);
    json.insert("codec", track.codec);
    json.insert("relativePath", track.relativePath);
    json.insert("duration", track.duration);
    json.insert("sourceUrl", track.sourceUrl.toString());
    json.insert("mediaSupport", supportName(track.mediaSupport));
    put(json, "trackNumber", track.trackNumber);
    put(json, "discNumber", track.discNumber);
    put(json, "bitsPerSample", track.bitsPerSample);
    put(json, "sampleRate", track.sampleRate);
    put(json, "bitrate", track.bitrate);
    if (track.lossless)
        json.insert("lossless", *track.lossless);
    else
        json.remove("lossless");
    return json;
}
QJsonObject toJson(const Album &album)
{
    QJsonObject json = album.fields;
    json.insert("id", album.id);
    json.insert("title", album.title);
    json.insert("artist", album.artist);
    json.insert("genreId", album.genreId);
    json.insert("description", album.description);
    json.insert("localNote", album.localNote);
    json.insert("folder", album.folderLabel);
    json.insert("folderUrl", album.folder.toString());
    if (!album.coverUrl.isEmpty())
        json.insert("coverUrl", album.coverUrl.toString());
    else
        json.remove("coverUrl");
    json.insert("offline", album.offline);
    json.insert("rawGenres", QJsonArray::fromStringList(album.rawGenres));
    put(json, "year", album.year);
    put(json, "discCount", album.discCount);
    putObject(json, "descriptionSource", album.descriptionSource);
    putObject(json, "introduction", album.introduction);
    putObject(json, "creditsLookup", album.creditsLookup);
    putObject(json, "online", album.online);
    QJsonArray tracks, producers;
    for (const auto &track : album.tracks)
        tracks.append(toJson(track));
    for (const auto &p : album.producers) {
        auto object = p.fields;
        object.insert("name", p.name);
        object.insert("role", p.role);
        object.insert("source", p.source);
        object.insert("trackId", p.trackId);
        object.insert("trackTitle", p.trackTitle);
        object.insert("url", p.url);
        producers.append(object);
    }
    json.insert("tracks", tracks);
    json.insert("producers", producers);
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
        object.insert("id", g.id);
        object.insert("name", g.name);
        object.insert("aliases", QJsonArray::fromStringList(g.aliases));
        put(object, "albumCount", g.albumCount);
        genres.append(object);
    }
    for (const auto &r : library.roots) {
        auto object = r.fields;
        if (!r.path.isEmpty())
            object.insert("path", r.path.toLocalFile());
        object.insert("status", r.status);
        object.insert("error", r.error);
        roots.append(object);
    }
    json.insert("albums", albums);
    json.insert("genres", genres);
    json.insert("roots", roots);
    json.insert("scan", library.scan);
    json.insert("onlineEnabled", library.onlineEnabled);
    putObject(json, "enrich", library.enrich);
    putObject(json, "introductions", library.introductions);
    putObject(json, "credits", library.credits);
    return json;
}
}
