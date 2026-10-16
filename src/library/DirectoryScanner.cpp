#include "DirectoryScanner.h"

#include <QCollator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace yalahoo {
namespace {
namespace fs = std::filesystem;
fs::path nativePath(const QString &value)
{
#ifdef Q_OS_WIN
    return fs::path(value.toStdWString());
#else
    return fs::path(value.toUtf8().toStdString());
#endif
}
QString qtPath(const fs::path &value)
{
#ifdef Q_OS_WIN
    return QDir::fromNativeSeparators(QString::fromStdWString(value.native()));
#else
    return QString::fromUtf8(value.native());
#endif
}
constexpr Qt::CaseSensitivity pathCase =
#ifdef Q_OS_WIN
        Qt::CaseInsensitive;
#else
        Qt::CaseSensitive;
#endif
bool within(const QString &path, const QString &parent)
{
    const QString prefix = parent.endsWith('/') ? parent : parent + '/';
    return path.compare(parent, pathCase) == 0 || path.startsWith(prefix, pathCase);
}
QString fileError(const QString &path, const std::error_code &error)
{
    return path + QStringLiteral(": ") + QString::fromStdString(error.message());
}
bool stopped(const ScanCancellation &cancel)
{
    return cancel && cancel->load(std::memory_order_relaxed);
}
const QSet<QString> audioExtensions{ "ape", "wv",  "wma", "flac", "wav",  "m4a", "mp4", "alac",
                                     "dsf", "dff", "mp3", "aac",  "aiff", "aif", "ogg", "opus" };

struct Walker
{
    LibrarySnapshot &next;
    const Snapshot &previous;
    const ScanCancellation &cancel;
    QHash<QString, const Album *> albums;
    QHash<QString, const Track *> tracks;
    QCollator collator{ QLocale(QStringLiteral("zh_CN")) };
    quint64 filesVisited = 0;

    Walker(LibrarySnapshot &next, const Snapshot &previous, const ScanCancellation &cancel)
        : next(next), previous(previous), cancel(cancel)
    {
        collator.setNumericMode(true);
        if (previous) {
            for (const auto &album : previous->albums) {
                if (stopped(cancel))
                    return;
                albums.insert(album.id, &album);
                for (const auto &track : album.tracks) {
                    if (stopped(cancel))
                        return;
                    tracks.insert(track.sourceUrl.toLocalFile(), &track);
                }
            }
        }
    }
    bool less(const QString &a, const QString &b) const
    {
        const int order = collator.compare(a, b);
        return order == 0 ? a < b : order < 0;
    }
    bool appendAlbum(const QString &root, const QString &folder, const QStringList &files,
                     bool single, QString &error)
    {
        const auto id = DirectoryScanner::stableId("album", single ? files.first() : folder);
        const Album *old = albums.value(id);
        Album album = old ? *old : Album{ };
        album.id = id;
        album.folder = QUrl::fromLocalFile(folder);
        album.folderLabel = folder;
        album.fields.insert("_root", root);
        album.fields.insert("_singleFile", single);
        album.offline = false;
        album.tracks.clear();
        album.tracks.reserve(files.size());
        for (const auto &file : files) {
            if (stopped(cancel))
                return false;
            const auto stamp = DirectoryScanner::fingerprint(file, &error);
            if (!error.isEmpty())
                return false;
            const Track *oldTrack = tracks.value(file);
            const bool unchanged = oldTrack && oldTrack->fields.value("_fingerprint") == stamp;
            Track track = unchanged ? *oldTrack : Track{ };
            if (oldTrack && !unchanged) {
                track.fields = oldTrack->fields;
                for (const auto *key : { "_metadataVersion", "_embeddedCover", "_common" })
                    track.fields.remove(QLatin1String(key));
            }
            const QFileInfo info(file);
            track.id = DirectoryScanner::stableId("track", file);
            track.albumId = id;
            track.sourceUrl = QUrl::fromLocalFile(file);
            track.relativePath = QDir(root).relativeFilePath(file);
            track.format = info.suffix().toUpper();
            track.fields.insert("_path", file);
            track.fields.insert("_fingerprint", stamp);
            if (!unchanged) {
                track.title = info.completeBaseName();
                track.artist = QStringLiteral("未知艺术家");
                static const QRegularExpression disc(
                        QStringLiteral("^(\\d{1,2})[-_](\\d{1,3})(?:[\\s._-]|$)"));
                const auto match = disc.match(info.fileName());
                if (match.hasMatch()) {
                    if (match.captured(1).toInt() > 0)
                        track.discNumber = match.captured(1).toInt();
                    if (match.captured(2).toInt() > 0)
                        track.trackNumber = match.captured(2).toInt();
                }
            }
            album.tracks.append(std::move(track));
        }
        std::sort(album.tracks.begin(), album.tracks.end(), [this](const Track &a, const Track &b) {
            if (a.discNumber.value_or(1) != b.discNumber.value_or(1))
                return a.discNumber.value_or(1) < b.discNumber.value_or(1);
            if (a.trackNumber.value_or(9999) != b.trackNumber.value_or(9999))
                return a.trackNumber.value_or(9999) < b.trackNumber.value_or(9999);
            return less(a.relativePath, b.relativePath);
        });
        if (!old) {
            album.title = single ? album.tracks.first().title : QFileInfo(folder).fileName();
            album.artist = QStringLiteral("未知艺术家");
            album.genreId = QStringLiteral("unclassified");
        }
        next.albums.append(std::move(album));
        return true;
    }
    bool walk(const QString &root, const QString &folder, QString &error)
    {
        if (stopped(cancel))
            return false;
        std::error_code ec;
        fs::directory_iterator it(nativePath(folder), ec), end;
        if (ec) {
            error = fileError(folder, ec);
            return false;
        }
        QStringList files, directories;
        for (; it != end; it.increment(ec)) {
            if (ec)
                break;
            if (stopped(cancel))
                return false;
            const auto status = it->symlink_status(ec);
            if (ec)
                break;
            const QString path = qtPath(it->path());
            const QString name = QFileInfo(path).fileName();
            if (fs::is_symlink(status))
                continue;
            if (fs::is_directory(status) && !name.startsWith('.'))
                directories.append(path);
            if (fs::is_regular_file(status)) {
                ++filesVisited;
                if (audioExtensions.contains(QFileInfo(name).suffix().toLower()))
                    files.append(path);
            }
        }
        if (ec) {
            error = fileError(folder, ec);
            return false;
        }
        std::sort(files.begin(), files.end(), [this](const QString &a, const QString &b) {
            return less(a, b);
        });
        std::sort(directories.begin(), directories.end(),
                  [this](const QString &a, const QString &b) {
                      return less(a, b);
                  });
        if (folder == root) {
            for (const auto &file : files)
                if (!appendAlbum(root, folder, { file }, true, error))
                    return false;
        } else if (!files.isEmpty() && !appendAlbum(root, folder, files, false, error))
            return false;
        // Do not recurse on the native stack; the caller drains one bounded path frontier.
        for (auto it = directories.crbegin(); it != directories.crend(); ++it)
            frontier.append(*it);
        return true;
    }
    QStringList frontier;
};
}

bool DirectoryScanner::normalizeRoots(const QList<QUrl> &input, QList<QUrl> &output, QString &error)
{
    error.clear();
    if (input.size() > 64) {
        error = QStringLiteral("音乐目录最多 64 个。");
        return false;
    }
    QStringList paths;
    for (const auto &url : input) {
        const auto path = QDir::fromNativeSeparators(url.toLocalFile());
        if (!url.isLocalFile() || !url.query().isEmpty() || !url.fragment().isEmpty()
            || !QDir::isAbsolutePath(path) || path.contains(QChar::Null)) {
            error = QStringLiteral("音乐目录必须为本地绝对路径。");
            return false;
        }
        const auto clean = QDir::cleanPath(path);
        if (std::none_of(paths.cbegin(), paths.cend(), [&](const QString &other) {
                return clean.compare(other, pathCase) == 0;
            }))
            paths.append(clean);
    }
    QList<QUrl> result;
    for (const auto &path : paths) {
        if (std::none_of(paths.cbegin(), paths.cend(), [&](const QString &other) {
                return path != other && within(path, other);
            }))
            result.append(QUrl::fromLocalFile(path));
    }
    output = std::move(result);
    return true;
}
QString DirectoryScanner::stableId(const QString &prefix, const QString &absolutePath)
{
    // Hash native separators exactly as Node path.resolve did on the target OS.
    const auto bytes = QDir::toNativeSeparators(absolutePath).toUtf8();
    return prefix + '-'
            + QString::fromLatin1(
                    QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().first(24));
}
QString DirectoryScanner::fingerprint(const QString &absolutePath, QString *error)
{
    std::error_code ec;
    const auto path = nativePath(absolutePath);
    const auto size = fs::file_size(path, ec);
    if (!ec) {
        const auto time = fs::last_write_time(path, ec);
        if (!ec) {
#ifdef _MSC_VER
            // MSVC file_clock provides to_utc, while libc++/libstdc++ provide to_sys.
            const auto epoch =
                    std::chrono::clock_cast<std::chrono::system_clock>(time).time_since_epoch();
#else
            const auto epoch = fs::file_time_type::clock::to_sys(time).time_since_epoch();
#endif
            const auto seconds = std::chrono::floor<std::chrono::seconds>(epoch);
            const auto nanoseconds =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(epoch - seconds).count();
            // Match Node's seconds*1000 + nanoseconds/1e6 without narrowing the epoch to ns.
            const double ms = static_cast<double>(seconds.count()) * 1000
                    + static_cast<double>(nanoseconds) / 1e6;
            auto encoded = QJsonDocument(QJsonArray{ ms }).toJson(QJsonDocument::Compact);
            encoded = encoded.mid(1, encoded.size() - 2);
            if (error)
                error->clear();
            return QString::number(size) + ':' + QString::fromLatin1(encoded);
        }
    }
    if (error)
        *error = fileError(absolutePath, ec);
    return { };
}
ScanResult DirectoryScanner::scan(const QList<QUrl> &roots, Snapshot previous,
                                  const ScanCancellation &cancel)
{
    ScanResult result;
    QList<QUrl> normalized;
    if (!normalizeRoots(roots, normalized, result.error))
        return result;
    if (stopped(cancel)) {
        result.cancelled = true;
        return result;
    }
    LibrarySnapshot next = previous ? *previous : LibrarySnapshot{ };
    next.albums.clear();
    next.roots.clear();
    Walker walker(next, previous, cancel);
    for (const auto &url : normalized) {
        if (stopped(cancel))
            break;
        const QString root = url.toLocalFile();
        LibraryRoot entry;
        entry.path = url;
        if (previous)
            for (const auto &old : previous->roots)
                if (old.path == url)
                    entry.fields = old.fields;
        const auto start = next.albums.size();
        std::error_code ec;
        const auto status = fs::symlink_status(nativePath(root), ec);
        bool ok = !ec && fs::is_directory(status) && !fs::is_symlink(status);
        if (!ok)
            entry.error = ec ? fileError(root, ec) : QStringLiteral("根目录不是普通文件夹。");
        walker.frontier = { root };
        while (ok && !walker.frontier.isEmpty() && !stopped(cancel)) {
            const QString folder = walker.frontier.takeLast();
            ok = walker.walk(root, folder, entry.error);
        }
        entry.status = ok ? QStringLiteral("online") : QStringLiteral("offline");
        if (!ok) {
            // A partially unreadable root is transactional: no apparent deletions.
            next.albums.resize(start);
            if (previous)
                for (auto album : previous->albums)
                    if (within(album.folder.toLocalFile(), root)) {
                        album.offline = true;
                        album.fields.insert("_root", root);
                        next.albums.append(std::move(album));
                    }
        }
        next.roots.append(std::move(entry));
    }
    result.filesVisited = walker.filesVisited;
    result.cancelled = stopped(cancel);
    if (result.cancelled)
        return result;
    if (std::none_of(next.genres.cbegin(), next.genres.cend(), [](const Genre &genre) {
            return genre.id == QLatin1String("unclassified");
        })) {
        Genre genre;
        genre.id = QStringLiteral("unclassified");
        genre.name = QStringLiteral("未分类");
        next.genres.append(std::move(genre));
    }
    QHash<QString, int> counts;
    for (const auto &album : next.albums)
        ++counts[album.genreId];
    for (auto &genre : next.genres)
        genre.albumCount = counts.value(genre.id);
    next.scan.insert("running", false);
    next.scan.remove("error");
    next.scan.insert("finishedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    result.snapshot = std::make_shared<const LibrarySnapshot>(std::move(next));
    return result;
}
}
