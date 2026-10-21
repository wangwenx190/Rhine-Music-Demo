#include "MusicTypes.h"
#include "Units.h"
#include <QCoreApplication>
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

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Stable identity and fingerprint encoding must match the frozen on-disk library protocol.
constexpr auto UnpreferredCoverRank = 3;
constexpr auto MaximumLibraryRoots = 64;
constexpr auto StableIdHexCharacters = 24;

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
    const QString prefix = parent.endsWith('/'_L1) ? parent : parent + '/'_L1;
    return path.compare(parent, pathCase) == 0 || path.startsWith(prefix, pathCase);
}
QString fileError(const QString &path, const std::error_code &error)
{
    return path + u": "_s + QString::fromStdString(error.message());
}
bool stopped(const ScanCancellation &cancel)
{
    return cancel && cancel->load(std::memory_order_relaxed);
}
// The extension index allocates Qt storage only when a scan actually needs it.
const QSet<QString> &audioExtensions()
{
    static const QSet<QString> value{ u"ape"_s,  u"wv"_s,   u"wma"_s, u"flac"_s, u"wav"_s, u"m4a"_s,
                                      u"mp4"_s,  u"alac"_s, u"dsf"_s, u"dff"_s,  u"mp3"_s, u"aac"_s,
                                      u"aiff"_s, u"aif"_s,  u"ogg"_s, u"opus"_s };
    return value;
}

struct Walker final
{
    LibrarySnapshot &next;
    const Snapshot &previous;
    const ScanCancellation &cancel;
    QHash<QString, const Album *> albums;
    QHash<QString, const Track *> tracks;
    QCollator collator{ QLocale(u"zh_CN"_s) };
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
                     bool single, const QStringList &images, QString &error)
    {
        const auto id = DirectoryScanner::stableId(u"album"_s, single ? files.first() : folder);
        const Album *old = albums.value(id);
        Album album = old ? *old : Album{ };
        album.id = id;
        album.folder = QUrl::fromLocalFile(folder);
        album.folderLabel = folder;
        album.fields.insert(u"_singleFile"_s, single);
        album.offline = false;
        QJsonArray candidates;
        for (const auto &image : images) {
            if (!single
                || QFileInfo(image).completeBaseName().compare(
                           QFileInfo(files.first()).completeBaseName(), Qt::CaseInsensitive)
                        == 0)
                candidates.append(image);
        }
        album.fields.insert(u"_coverCandidates"_s, candidates);
        album.tracks.clear();
        album.tracks.reserve(files.size());
        for (const auto &file : files) {
            if (stopped(cancel))
                return false;
            const auto stamp = DirectoryScanner::fingerprint(file, &error);
            if (!error.isEmpty())
                return false;
            const Track *oldTrack = tracks.value(file);
            const bool unchanged = oldTrack && oldTrack->fields.value(u"_fingerprint"_s) == stamp;
            Track track = unchanged ? *oldTrack : Track{ };
            if (oldTrack && !unchanged) {
                track.fields = oldTrack->fields;
                for (const auto &key : { u"_embeddedCover"_s, u"_common"_s, u"_qtProbeVersion"_s,
                                         u"_probeStatus"_s, u"_probeError"_s, u"_artworkError"_s })
                    track.fields.remove(key);
            }
            const QFileInfo info(file);
            track.id = DirectoryScanner::stableId(u"track"_s, file);
            track.albumId = id;
            track.sourceUrl = QUrl::fromLocalFile(file);
            track.relativePath = QDir(root).relativeFilePath(file);
            track.format = info.suffix().toUpper();
            track.fields.insert(u"_fingerprint"_s, stamp);
            if (!unchanged) {
                track.title = info.completeBaseName();
                track.artist.clear();
                static const QRegularExpression disc(u"^(\\d{1,2})[-_](\\d{1,3})(?:[\\s._-]|$)"_s);
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
            if (a.trackNumber.value_or(yalahoo::libraryDefaults::UnnumberedTrackSortKey)
                != b.trackNumber.value_or(yalahoo::libraryDefaults::UnnumberedTrackSortKey))
                return a.trackNumber.value_or(yalahoo::libraryDefaults::UnnumberedTrackSortKey)
                        < b.trackNumber.value_or(yalahoo::libraryDefaults::UnnumberedTrackSortKey);
            return less(a.relativePath, b.relativePath);
        });
        if (!old) {
            album.title = single ? album.tracks.first().title : QFileInfo(folder).fileName();
            album.artist.clear();
            album.genreId = u"unclassified"_s;
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
        QStringList files, directories, images;
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
            if (fs::is_directory(status) && !name.startsWith('.'_L1))
                directories.append(path);
            if (fs::is_regular_file(status)) {
                ++filesVisited;
                if (audioExtensions().contains(QFileInfo(name).suffix().toLower()))
                    files.append(path);
                const auto extension = QFileInfo(name).suffix().toLower();
                if (extension == "png"_L1 || extension == "jpg"_L1 || extension == "jpeg"_L1
                    || extension == "webp"_L1)
                    images.append(path);
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
        const QStringList preferred{ u"cover"_s, u"folder"_s, u"front"_s };
        QHash<QString, int> imageRanks;
        for (const auto &image : images) {
            const auto rank = preferred.indexOf(QFileInfo(image).completeBaseName().toLower());
            imageRanks.insert(image, rank < 0 ? UnpreferredCoverRank : int(rank));
        }
        std::sort(images.begin(), images.end(),
                  [this, &imageRanks](const QString &a, const QString &b) {
                      const int first = imageRanks.value(a), second = imageRanks.value(b);
                      return first == second ? less(a, b) : first < second;
                  });
        if (folder == root) {
            QHash<QString, QStringList> singleImages;
            for (const auto &image : images)
                singleImages[QFileInfo(image).completeBaseName().toCaseFolded()].append(image);
            for (const auto &file : files) {
                const auto matching =
                        singleImages.value(QFileInfo(file).completeBaseName().toCaseFolded());
                if (!appendAlbum(root, folder, { file }, true, matching, error))
                    return false;
            }
        } else if (!files.isEmpty() && !appendAlbum(root, folder, files, false, images, error))
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
    if (input.size() > MaximumLibraryRoots) {
        error = QCoreApplication::translate("Yalahoo", "At most 64 music directories are allowed.");
        return false;
    }
    QStringList paths;
    for (const auto &url : input) {
        const auto path = QDir::fromNativeSeparators(url.toLocalFile());
        if (!url.isLocalFile() || !url.query().isEmpty() || !url.fragment().isEmpty()
            || !QDir::isAbsolutePath(path) || path.contains(QChar::Null)) {
            error = QCoreApplication::translate("Yalahoo",
                                                "Music directories must be local absolute paths.");
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
    return prefix + '-'_L1
            + QString::fromUtf8(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                                        .toHex()
                                        .first(StableIdHexCharacters));
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
            const double ms =
                    static_cast<double>(seconds.count()) * yalahoo::units::MillisecondsPerSecond
                    + static_cast<double>(nanoseconds) / yalahoo::units::NanosecondsPerMillisecond;
            auto encoded = QJsonDocument(QJsonArray{ ms }).toJson(QJsonDocument::Compact);
            encoded = encoded.mid(1, encoded.size() - 2);
            if (error)
                error->clear();
            return QString::number(size) + ':'_L1 + QString::fromUtf8(encoded);
        }
    }
    if (error)
        *error = fileError(absolutePath, ec);
    return { };
}
// Build a candidate snapshot off-thread and roll back an unreadable root as a whole.
// Cancellation publishes no partial library; the owner validates the generation before acceptance.
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
            entry.error = ec ? fileError(root, ec)
                             : QCoreApplication::translate("Yalahoo",
                                                           "The root is not a regular directory.");
        walker.frontier = { root };
        while (ok && !walker.frontier.isEmpty() && !stopped(cancel)) {
            const QString folder = walker.frontier.takeLast();
            ok = walker.walk(root, folder, entry.error);
        }
        entry.status = ok ? u"online"_s : u"offline"_s;
        if (!ok) {
            // A partially unreadable root is transactional: no apparent deletions.
            next.albums.resize(start);
            if (previous)
                for (auto album : previous->albums)
                    if (within(album.folder.toLocalFile(), root)) {
                        album.offline = true;
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
            return genre.id == "unclassified"_L1;
        })) {
        Genre genre;
        genre.id = u"unclassified"_s;
        next.genres.append(std::move(genre));
    }
    QHash<QString, int> counts;
    for (const auto &album : next.albums)
        ++counts[album.genreId];
    for (auto &genre : next.genres)
        genre.albumCount = counts.value(genre.id);
    next.scan.insert(u"running"_s, false);
    next.scan.remove(u"error"_s);
    next.scan.insert(u"finishedAt"_s, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    result.snapshot = std::make_shared<const LibrarySnapshot>(std::move(next));
    return result;
}
}
