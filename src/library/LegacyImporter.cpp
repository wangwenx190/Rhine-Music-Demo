#include "LegacyImporter.h"
#include "ArtworkStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

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
        error = QStringLiteral("无法读取旧索引/配置或文件超过大小上限：") + path;
        return false;
    }
    const auto bytes = file.read(limit + 1);
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (bytes.size() > limit || file.error() != QFileDevice::NoError
        || parse.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("旧索引/配置 JSON 无效：") + path;
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
    return relative != ".." && !relative.startsWith("../") && !QDir::isAbsolutePath(relative);
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
    const auto path = cover.value("path").toString();
    const auto allowed =
            cover.value("embedded").toBool() ? QDir(legacyRoot).filePath("artwork") : musicRoot;
    const auto canonical = QFileInfo(path).canonicalFilePath();
    const auto canonicalRoot = QFileInfo(allowed).canonicalFilePath();
    if (!absolute(path) || canonical.isEmpty() || canonicalRoot.isEmpty()
        || !inside(canonical, canonicalRoot)) {
        error = QStringLiteral("旧封面路径无效、丢失或超出来源目录。");
        return { };
    }
    const auto copied = ArtworkStore::copy(canonical, dataRoot, &error);
    if (copied.isEmpty())
        return { };
    return QJsonObject{ { "path", copied.toLocalFile() } };
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
        return fail(QStringLiteral("旧数据与原生数据必须使用互不嵌套的绝对目录。"));
    if (stopped()) {
        result.cancelled = true;
        return result;
    }
    QJsonObject index, config, rules;
    if (!readObject(QDir(sourceRoot).filePath("library-index.json"), index, result.error)
        || !readObject(QDir(sourceRoot).filePath("config.json"), config, result.error, true)
        || !readObject(QDir(sourceRoot).filePath("genre-rules.json"), rules, result.error, true))
        return result;
    if (index.value("version").toInt() != 1 || !index.value("albums").isArray())
        return fail(QStringLiteral("旧索引版本或 albums 格式不受支持，原文件已保留。"));
    QJsonArray roots;
    QList<QUrl> rootUrls, normalized;
    const auto configured = config.value("roots").toArray();
    if (config.contains("roots") && !config.value("roots").isArray())
        return fail(QStringLiteral("旧 roots 必须为数组。"));
    if (!configured.isEmpty()) {
        for (const auto &root : configured)
            rootUrls.append(QUrl::fromLocalFile(root.toString()));
    } else {
        for (const auto &root : index.value("roots").toArray())
            rootUrls.append(QUrl::fromLocalFile(
                    root.isString() ? root.toString() : root.toObject().value("path").toString()));
    }
    if (!DirectoryScanner::normalizeRoots(rootUrls, normalized, result.error))
        return result;
    for (const auto &root : normalized) {
        QJsonObject entry;
        for (const auto &value : index.value("roots").toArray())
            if (value.toObject().value("path").toString() == root.toLocalFile())
                entry = value.toObject();
        entry.insert("path", root.toString());
        roots.append(entry);
    }
    QJsonArray albums;
    QSet<QString> albumIds, trackIds;
    QHash<QString, QString> remappedAlbums;
    for (const auto &value : index.value("albums").toArray()) {
        if (stopped()) {
            result.cancelled = true;
            return result;
        }
        auto album = value.toObject();
        const auto folder = album.value("folder").toString();
        if (!absolute(folder) || !album.value("tracks").isArray())
            return fail(QStringLiteral("旧专辑缺少本机绝对 folder/tracks；不猜测跨系统路径。"));
        const auto oldTracks = album.value("tracks").toArray();
        const auto root = album.value("_root").toString();
        if (!absolute(root) || !inside(folder, root))
            return fail(QStringLiteral("旧专辑根目录无效。"));
        const bool single =
                oldTracks.size() == 1 && QDir::cleanPath(folder) == QDir::cleanPath(root);
        QString identity = folder;
        if (single)
            identity = oldTracks.first().toObject().value("_path").toString();
        if (!absolute(identity))
            return fail(QStringLiteral("旧单曲缺少绝对文件路径。"));
        const auto albumId = DirectoryScanner::stableId("album", identity);
        if (albumIds.contains(albumId))
            return fail(QStringLiteral("旧索引包含重复专辑路径。"));
        albumIds.insert(albumId);
        remappedAlbums.insert(album.value("id").toString(), albumId);
        QHash<QString, QString> remappedTracks;
        QJsonArray tracks;
        for (const auto &entry : oldTracks) {
            if (stopped()) {
                result.cancelled = true;
                return result;
            }
            auto track = entry.toObject();
            const auto path = track.value("_path").toString();
            const auto version = track.value("_metadataVersion").toInt();
            if (!absolute(path)
                || QDir::cleanPath(QFileInfo(path).absolutePath()) != QDir::cleanPath(folder)
                || (version != 3 && version != 4))
                return fail(QStringLiteral("旧曲目需要本机直属路径及 v3/v4 元数据。"));
            const auto id = DirectoryScanner::stableId("track", path);
            if (trackIds.contains(id))
                return fail(QStringLiteral("旧索引包含重复曲目路径。"));
            trackIds.insert(id);
            remappedTracks.insert(track.value("id").toString(), id);
            track.insert("id", id);
            track.insert("albumId", albumId);
            track.insert("sourceUrl", QUrl::fromLocalFile(path).toString());
            track.insert("mediaSupport", "Unknown");
            track.remove("_qtProbeVersion");
            QString coverError;
            const auto cover = copyCover(track.value("_embeddedCover").toObject(), sourceRoot, root,
                                         dataRoot, coverError);
            if (!coverError.isEmpty())
                track.insert("_coverImportError", coverError);
            if (cover.isEmpty())
                track.remove("_embeddedCover");
            else
                track.insert("_embeddedCover", cover);
            tracks.append(track);
        }
        album.insert("id", albumId);
        album.insert("folderUrl", QUrl::fromLocalFile(folder).toString());
        album.insert("_singleFile", single);
        album.insert("tracks", tracks);
        QJsonArray producers;
        for (const auto &entry : album.value("producers").toArray()) {
            auto person = entry.toObject();
            const auto old = person.value("trackId").toString();
            if (remappedTracks.contains(old))
                person.insert("trackId", remappedTracks.value(old));
            producers.append(person);
        }
        album.insert("producers", producers);
        QString coverError;
        const auto cover =
                copyCover(album.value("_cover").toObject(), sourceRoot, root, dataRoot, coverError);
        album.remove("coverUrl");
        if (!cover.isEmpty()) {
            album.insert("coverUrl",
                         QUrl::fromLocalFile(cover.value("path").toString()).toString());
        } else if (!coverError.isEmpty()) {
            album.insert("_coverImportError", coverError);
        }
        albums.append(album);
    }
    auto genres = rules.value("genres").toArray();
    if (genres.isEmpty())
        genres = index.value("genres").toArray();
    QSet<QString> genreIds;
    for (const auto &genre : genres)
        genreIds.insert(genre.toObject().value("id").toString());
    for (const auto &album : albums) {
        auto id = album.toObject().value("genreId").toString();
        if (id.isEmpty())
            id = "unclassified";
        if (!genreIds.contains(id)) {
            genres.append(QJsonObject{
                    { "id", id },
                    { "name", id == "unclassified" ? QStringLiteral("未分类") : id } });
            genreIds.insert(id);
        }
    }
    auto overrides = rules.value("albumOverrides").toObject();
    for (auto it = remappedAlbums.cbegin(); it != remappedAlbums.cend(); ++it) {
        if (it.key() != it.value() && overrides.contains(it.key())) {
            const auto genre = overrides.take(it.key());
            overrides.insert(it.value(), genre);
        }
    }
    if (rules.contains("albumOverrides"))
        rules.insert("albumOverrides", overrides);
    index.insert("albums", albums);
    index.insert("genres", genres);
    index.insert("roots", roots);
    QJsonObject metadataConfiguration = config;
    for (const auto &key : { "version", "roots" })
        metadataConfiguration.remove(key);
    index.insert("metadataConfiguration", metadataConfiguration);
    index.insert("genreOverrides", overrides);
    index.insert("onlineEnabled", false);
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
