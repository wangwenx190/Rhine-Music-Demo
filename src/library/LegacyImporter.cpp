#include <QCoreApplication>
#include "LegacyImporter.h"
#include "ArtworkStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
constexpr qint64 maximumJsonBytes = 256 * 1024 * 1024;
bool readObject(const QString &path, QJsonObject &output, QString &error, bool optional = false)
{
    QFile file(path);
    if (optional && !file.exists())
        return true;
    QJsonParseError parse;
    const auto limit = optional ? qint64(1024 * 1024) : maximumJsonBytes;
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit) {
        error = QCoreApplication::translate(
                        "Yalahoo",
                        "Cannot read the legacy index or configuration, or it exceeds the size limit: ")
                + path;
        return false;
    }
    const auto bytes = file.read(limit + 1);
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (bytes.size() > limit || file.error() != QFileDevice::NoError
        || parse.error != QJsonParseError::NoError || !document.isObject()) {
        error = QCoreApplication::translate("Yalahoo",
                                            "Invalid legacy index or configuration JSON: ")
                + path;
        return false;
    }
    output = document.object();
    return true;
}
bool absolute(const QString &path)
{
    return QDir::isAbsolutePath(path) && !path.contains(QChar::Null);
}
bool inside(const QString &path, const QString &root)
{
    const auto relative = QDir(root).relativeFilePath(path);
    return relative != ".."_L1 && !relative.startsWith(u"../"_s) && !QDir::isAbsolutePath(relative);
}
QString resolvedDestination(const QString &path)
{
    const QFileInfo info(path);
    const auto canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;
    const auto parent = info.absolutePath();
    if (parent == path)
        return QDir::cleanPath(path);
    return QDir(resolvedDestination(parent)).filePath(info.fileName());
}
QJsonObject copyCover(QJsonObject cover, const QString &legacyRoot, const QString &musicRoot,
                      const QString &dataRoot, QString &error)
{
    if (cover.isEmpty())
        return cover;
    const auto path = cover.value(u"path"_s).toString();
    const auto allowed = cover.value(u"embedded"_s).toBool()
            ? QDir(legacyRoot).filePath(u"artwork"_s)
            : musicRoot;
    const auto canonical = QFileInfo(path).canonicalFilePath();
    const auto canonicalRoot = QFileInfo(allowed).canonicalFilePath();
    if (!absolute(path) || canonical.isEmpty() || canonicalRoot.isEmpty()
        || !inside(canonical, canonicalRoot)) {
        error = QCoreApplication::translate(
                "Yalahoo",
                "The legacy cover path is invalid, missing, or outside its source directory.");
        return { };
    }
    const auto copied = ArtworkStore::copy(canonical, dataRoot, &error);
    if (copied.isEmpty())
        return { };
    return QJsonObject{ { u"path"_s, copied.toLocalFile() } };
}
}
ScanResult LegacyImporter::read(const QString &sourceRoot, const QString &dataRoot,
                                const ScanCancellation &cancel)
{
    ScanResult result;
    const auto stopped = [&] {
        return cancel && cancel->load(std::memory_order_relaxed);
    };
    const auto fail = [&](const QString &error) {
        result.error = error;
        return result;
    };
    if (!absolute(sourceRoot) || !absolute(dataRoot)
        || inside(resolvedDestination(dataRoot), resolvedDestination(sourceRoot))
        || inside(resolvedDestination(sourceRoot), resolvedDestination(dataRoot)))
        return fail(QCoreApplication::translate(
                "Yalahoo",
                "Legacy and native data require separate, non-nested absolute directories."));
    if (stopped()) {
        result.cancelled = true;
        return result;
    }
    QJsonObject index, config, rules;
    if (!readObject(QDir(sourceRoot).filePath(u"library-index.json"_s), index, result.error)
        || !readObject(QDir(sourceRoot).filePath(u"config.json"_s), config, result.error, true)
        || !readObject(QDir(sourceRoot).filePath(u"genre-rules.json"_s), rules, result.error, true))
        return result;
    if (index.value(u"version"_s).toInt() != 1 || !index.value(u"albums"_s).isArray())
        return fail(QCoreApplication::translate(
                "Yalahoo",
                "Unsupported legacy index version or albums format. The original files were preserved."));
    QJsonArray roots;
    QList<QUrl> rootUrls, normalized;
    const auto configured = config.value(u"roots"_s).toArray();
    if (config.contains(u"roots"_s) && !config.value(u"roots"_s).isArray())
        return fail(QCoreApplication::translate("Yalahoo", "Legacy roots must be an array."));
    if (!configured.isEmpty()) {
        for (const auto &root : configured)
            rootUrls.append(QUrl::fromLocalFile(root.toString()));
    } else {
        for (const auto &root : index.value(u"roots"_s).toArray())
            rootUrls.append(QUrl::fromLocalFile(
                    root.isString() ? root.toString()
                                    : root.toObject().value(u"path"_s).toString()));
    }
    if (!DirectoryScanner::normalizeRoots(rootUrls, normalized, result.error))
        return result;
    for (const auto &root : normalized) {
        QJsonObject entry;
        for (const auto &value : index.value(u"roots"_s).toArray())
            if (value.toObject().value(u"path"_s).toString() == root.toLocalFile())
                entry = value.toObject();
        entry.insert(u"path"_s, root.toString());
        roots.append(entry);
    }
    QJsonArray albums;
    QSet<QString> albumIds, trackIds;
    QHash<QString, QString> remappedAlbums;
    for (const auto &value : index.value(u"albums"_s).toArray()) {
        if (stopped()) {
            result.cancelled = true;
            return result;
        }
        auto album = value.toObject();
        const auto folder = album.value(u"folder"_s).toString();
        if (!absolute(folder) || !album.value(u"tracks"_s).isArray())
            return fail(QCoreApplication::translate(
                    "Yalahoo",
                    "Legacy albums require native absolute folder and track paths. Paths cannot be inferred across systems."));
        const auto oldTracks = album.value(u"tracks"_s).toArray();
        const auto root = album.value(u"_root"_s).toString();
        if (!absolute(root) || !inside(folder, root))
            return fail(QCoreApplication::translate("Yalahoo", "Invalid legacy album root."));
        const bool single =
                oldTracks.size() == 1 && QDir::cleanPath(folder) == QDir::cleanPath(root);
        QString identity = folder;
        if (single)
            identity = oldTracks.first().toObject().value(u"_path"_s).toString();
        if (!absolute(identity))
            return fail(QCoreApplication::translate(
                    "Yalahoo", "A legacy single track requires an absolute file path."));
        const auto albumId = DirectoryScanner::stableId(u"album"_s, identity);
        if (albumIds.contains(albumId))
            return fail(QCoreApplication::translate(
                    "Yalahoo", "The legacy index contains duplicate album paths."));
        albumIds.insert(albumId);
        remappedAlbums.insert(album.value(u"id"_s).toString(), albumId);
        QHash<QString, QString> remappedTracks;
        QJsonArray tracks;
        for (const auto &entry : oldTracks) {
            if (stopped()) {
                result.cancelled = true;
                return result;
            }
            auto track = entry.toObject();
            const auto path = track.value(u"_path"_s).toString();
            const auto version = track.value(u"_metadataVersion"_s).toInt();
            if (!absolute(path)
                || QDir::cleanPath(QFileInfo(path).absolutePath()) != QDir::cleanPath(folder)
                || (version != 3 && version != 4))
                return fail(QCoreApplication::translate(
                        "Yalahoo",
                        "Legacy tracks require direct native paths and v3/v4 metadata."));
            const auto id = DirectoryScanner::stableId(u"track"_s, path);
            if (trackIds.contains(id))
                return fail(QCoreApplication::translate(
                        "Yalahoo", "The legacy index contains duplicate track paths."));
            trackIds.insert(id);
            remappedTracks.insert(track.value(u"id"_s).toString(), id);
            track.insert(u"id"_s, id);
            track.insert(u"albumId"_s, albumId);
            track.insert(u"sourceUrl"_s, QUrl::fromLocalFile(path).toString());
            track.insert(u"mediaSupport"_s, u"Unknown"_s);
            track.remove(u"_qtProbeVersion"_s);
            QString coverError;
            const auto cover = copyCover(track.value(u"_embeddedCover"_s).toObject(), sourceRoot,
                                         root, dataRoot, coverError);
            if (!coverError.isEmpty())
                track.insert(u"_coverImportError"_s, coverError);
            if (cover.isEmpty())
                track.remove(u"_embeddedCover"_s);
            else
                track.insert(u"_embeddedCover"_s, cover);
            tracks.append(track);
        }
        album.insert(u"id"_s, albumId);
        album.insert(u"folderUrl"_s, QUrl::fromLocalFile(folder).toString());
        album.insert(u"_singleFile"_s, single);
        album.insert(u"tracks"_s, tracks);
        QJsonArray producers;
        for (const auto &entry : album.value(u"producers"_s).toArray()) {
            auto person = entry.toObject();
            const auto old = person.value(u"trackId"_s).toString();
            if (remappedTracks.contains(old))
                person.insert(u"trackId"_s, remappedTracks.value(old));
            producers.append(person);
        }
        album.insert(u"producers"_s, producers);
        QString coverError;
        const auto cover = copyCover(album.value(u"_cover"_s).toObject(), sourceRoot, root,
                                     dataRoot, coverError);
        album.remove(u"coverUrl"_s);
        if (!cover.isEmpty()) {
            album.insert(u"coverUrl"_s,
                         QUrl::fromLocalFile(cover.value(u"path"_s).toString()).toString());
        } else if (!coverError.isEmpty()) {
            album.insert(u"_coverImportError"_s, coverError);
        }
        albums.append(album);
    }
    auto genres = rules.value(u"genres"_s).toArray();
    if (genres.isEmpty())
        genres = index.value(u"genres"_s).toArray();
    QSet<QString> genreIds;
    for (const auto &genre : genres)
        genreIds.insert(genre.toObject().value(u"id"_s).toString());
    for (const auto &album : albums) {
        auto id = album.toObject().value(u"genreId"_s).toString();
        if (id.isEmpty())
            id = "unclassified"_L1;
        if (!genreIds.contains(id)) {
            genres.append(QJsonObject{ { u"id"_s, id },
                                       { u"name"_s, id == "unclassified"_L1 ? QString{ } : id } });
            genreIds.insert(id);
        }
    }
    auto overrides = rules.value(u"albumOverrides"_s).toObject();
    for (auto it = remappedAlbums.cbegin(); it != remappedAlbums.cend(); ++it) {
        if (it.key() != it.value() && overrides.contains(it.key())) {
            const auto genre = overrides.take(it.key());
            overrides.insert(it.value(), genre);
        }
    }
    if (rules.contains(u"albumOverrides"_s))
        rules.insert(u"albumOverrides"_s, overrides);
    index.insert(u"albums"_s, albums);
    index.insert(u"genres"_s, genres);
    index.insert(u"roots"_s, roots);
    QJsonObject metadataConfiguration = config;
    for (const auto &key : { u"version"_s, u"roots"_s })
        metadataConfiguration.remove(key);
    index.insert(u"metadataConfiguration"_s, metadataConfiguration);
    index.insert(u"genreOverrides"_s, overrides);
    index.insert(u"onlineEnabled"_s, false);
    LibrarySnapshot next;
    if (!readLibrary(index, next, &result.error))
        return result;
    if (stopped()) {
        result.cancelled = true;
        return result;
    }
    result.snapshot = std::make_shared<const LibrarySnapshot>(std::move(next));
    return result;
}
}
