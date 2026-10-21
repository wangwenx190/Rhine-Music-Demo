#include "MetadataConfiguration.h"
#include "Units.h"
#include "IMetadataProvider.h"
#include <QCoreApplication>
#include "LibraryStore.h"
#include <QThreadPool>
#include <QFutureWatcher>
#include <QHash>
#include "MediaMetadataProbe.h"
#include "DirectoryScanner.h"
#include "Logging.h"
#include "LegacyImporter.h"
#include "metadata/MetadataMerge.h"
#include <QTimer>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QThread>
#include <QtConcurrentRun>
#include <utility>

namespace {
constexpr qint64 maximumCacheBytes =
        256LL * yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB;
// Bound metadata batches and validate persisted network configuration before committing it.
constexpr auto MetadataBatchMiB = 8;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
}
struct LibraryStore::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
    QString m_path, m_error;
    QList<QUrl> m_roots;
    Snapshot m_snapshot, m_savePending;
    QHash<QString, ProviderResult> m_metadataPending;
    bool m_metadataCommit = false;
    qsizetype m_metadataBytes = 0;
    QString m_dataRoot, m_importPending;
    quint64 m_filesVisited = 0;
    MediaMetadataProbe m_probe;
    QThreadPool m_worker;
    QFutureWatcher<JobResult> m_job;
    ScanCancellation m_cancel;
    Kind m_active = Kind::None;
    quint64 m_generation = 0, m_activeGeneration = 0;
    bool m_loadPending = false, m_scanPending = false, m_writable = true, m_initialized = false;
};
LibraryStore::LibraryStore(QString dataRoot, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->m_path = QDir(dataRoot).filePath(u"library-index.json"_s);
    d->m_snapshot = std::make_shared<const LibrarySnapshot>();
    d->m_dataRoot = std::move(dataRoot);
    qRegisterMetaType<Snapshot>();
    connect(&d->m_probe, &MediaMetadataProbe::finished, this,
            [this](quint64 generation, Snapshot snapshot, bool cancelled) {
                if (d->m_active != Kind::Probe || generation != d->m_activeGeneration)
                    return;
                d->m_active = Kind::None;
                d->m_cancel.reset();
                accept({ snapshot, { }, d->m_filesVisited, cancelled }, Kind::Scan, generation);
                schedule();
            });
    d->m_worker.setMaxThreadCount(1);
    connect(&d->m_job, &QFutureWatcher<JobResult>::finished, this, &LibraryStore::complete);
}
LibraryStore::~LibraryStore()
{
    if (d->m_cancel)
        d->m_cancel->store(true, std::memory_order_relaxed);
    // Only teardown waits. A scan that never reached GUI acceptance is never saved.
    d->m_worker.waitForDone();
    if (d->m_active == Kind::Metadata) {
        const auto result = d->m_job.result();
        if (result.snapshot)
            d->m_snapshot = result.snapshot;
        d->m_savePending = d->m_snapshot;
    }
    if (!d->m_metadataPending.isEmpty() && d->m_writable) {
        auto snapshot = *d->m_snapshot;
        for (auto &album : snapshot.albums)
            for (const auto kind :
                 { MetadataKind::Introduction, MetadataKind::Credits, MetadataKind::Enrichment }) {
                const auto it = d->m_metadataPending.constFind(album.id + '/'_L1 + kindName(kind));
                if (it != d->m_metadataPending.cend() && it->inputKey == metadataKey(album, kind))
                    mergeMetadata(album, *it);
            }
        d->m_savePending = std::make_shared<const LibrarySnapshot>(std::move(snapshot));
    }
    if (d->m_savePending && d->m_writable) {
        const auto result = write(d->m_path, d->m_savePending);
        if (!result.error.isEmpty())
            qCWarning(logLibrary) << "Library shutdown save failed" << "cache=" << d->m_path
                                  << "error=" << result.error;
    }
}
void LibraryStore::stageMetadata(const ProviderResult &result)
{
    if (!d->m_initialized || !d->m_writable || result.fromCache || result.inputKey.isEmpty()
        || result.checkedAt.isEmpty() || result.status == ProviderStatus::Deferred
        || result.status == ProviderStatus::Cancelled)
        return;
    const auto key = result.albumId + '/'_L1 + kindName(result.kind);
    d->m_metadataBytes -= d->m_metadataPending.value(key).payloadBytes;
    d->m_metadataPending.insert(key, result);
    d->m_metadataBytes += result.payloadBytes;
    if (d->m_metadataBytes
        >= MetadataBatchMiB * yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB)
        commitMetadata();
}
void LibraryStore::commitMetadata()
{
    if (d->m_metadataPending.isEmpty())
        return;
    d->m_metadataCommit = true;
    schedule();
}
bool LibraryStore::busy() const
{
    return d->m_active != Kind::None || d->m_scanPending || d->m_loadPending
            || !d->m_importPending.isEmpty() || bool(d->m_savePending);
}
bool LibraryStore::scanning() const
{
    return d->m_scanPending
            || ((d->m_active == Kind::Scan || d->m_active == Kind::Probe)
                && d->m_activeGeneration == d->m_generation);
}
bool LibraryStore::probing() const
{
    return d->m_active == Kind::Probe && d->m_activeGeneration == d->m_generation;
}
QString LibraryStore::error() const
{
    return d->m_error;
}
QList<QUrl> LibraryStore::roots() const
{
    return d->m_roots;
}
bool LibraryStore::setMetadataConfiguration(const QVariantMap &configuration)
{
    if (!d->m_initialized || busy() || !d->m_writable) {
        report(QCoreApplication::translate(
                "Yalahoo",
                "Wait until the music library is ready before saving metadata settings."));
        return false;
    }
    auto snapshot = *d->m_snapshot;
    auto object = snapshot.fields.value(u"metadataConfiguration"_s).toObject();
    const auto patch = QJsonObject::fromVariantMap(configuration);
    if ((patch.contains(u"musicBrainzContact"_s)
         && !patch.value(u"musicBrainzContact"_s).isString())
        || (patch.contains(u"onlineEnabled"_s) && !patch.value(u"onlineEnabled"_s).isBool())) {
        report(QCoreApplication::translate("Yalahoo",
                                           "Invalid metadata configuration field type."));
        return false;
    }
    if ((patch.contains(u"proxyEnabled"_s) && !patch.value(u"proxyEnabled"_s).isBool())
        || (patch.contains(u"proxyMode"_s)
            && (!patch.value(u"proxyMode"_s).isString()
                || !QStringList{ u"system"_s, u"manual"_s }.contains(
                        patch.value(u"proxyMode"_s).toString())))
        || (patch.contains(u"networkEnabled"_s) && !patch.value(u"networkEnabled"_s).isBool())
        || (patch.contains(u"proxyUrl"_s) && !patch.value(u"proxyUrl"_s).isString())) {
        report(QCoreApplication::translate("Yalahoo",
                                           "Invalid metadata configuration field type."));
        return false;
    }
    if (patch.contains(u"musicBrainzContact"_s)) {
        const auto contact = patch.value(u"musicBrainzContact"_s).toString();
        if (contact.size() > yalahoo::metadataConfiguration::MaximumContactCharacters
            || contact.contains(QChar::CarriageReturn) || contact.contains(QChar::LineFeed)) {
            report(QCoreApplication::translate(
                    "Yalahoo", "The metadata contact must be one line of at most 500 characters."));
            return false;
        }
    }
    if (patch.contains(u"proxyUrl"_s) && !patch.value(u"proxyUrl"_s).toString().isEmpty()) {
        const QUrl proxy(patch.value(u"proxyUrl"_s).toString());
        if (!metadataConfiguration::validProxy(proxy)) {
            report(QCoreApplication::translate("Yalahoo", "Enter a valid HTTP proxy URL."));
            return false;
        }
    }
    for (auto it = patch.begin(); it != patch.end(); ++it)
        object.insert(it.key(), it.value());
    if (object == snapshot.fields.value(u"metadataConfiguration"_s).toObject())
        return true;
    snapshot.fields.insert(u"metadataConfiguration"_s, object);
    d->m_snapshot = std::make_shared<const LibrarySnapshot>(std::move(snapshot));
    d->m_savePending = d->m_snapshot;
    Q_EMIT metadataConfigurationChanged(d->m_snapshot);
    if (!d->m_metadataPending.isEmpty())
        d->m_metadataCommit = true;
    schedule();
    return true;
}
Snapshot LibraryStore::snapshot() const
{
    return d->m_snapshot;
}
void LibraryStore::report(const QString &error)
{
    d->m_error = error;
    Q_EMIT statusChanged();
    if (!error.isEmpty()) {
        qCWarning(logLibrary) << "Library operation failed" << "kind=" << int(d->m_active)
                              << "generation=" << d->m_generation << "cache=" << d->m_path
                              << "error=" << error;
        Q_EMIT failed(error);
    }
}
void LibraryStore::load()
{
    Q_ASSERT(QThread::currentThread() == thread());
    cancel();
    d->m_loadPending = true;
    schedule();
}
QStringList LibraryStore::rootPaths() const
{
    QStringList result;
    for (const auto &root : d->m_roots)
        result.append(root.toLocalFile());
    return result;
}
bool LibraryStore::setRootPaths(const QString &paths)
{
    QList<QUrl> roots;
    for (const auto &line : paths.split('\n'_L1))
        if (!line.trimmed().isEmpty())
            roots.append(QUrl::fromLocalFile(line.trimmed()));
    return setRoots(roots);
}
bool LibraryStore::setRoots(const QList<QUrl> &roots)
{
    Q_ASSERT(QThread::currentThread() == thread());
    QList<QUrl> normalized;
    QString error;
    if (!DirectoryScanner::normalizeRoots(roots, normalized, error)) {
        report(error);
        return false;
    }
    d->m_roots = std::move(normalized);
    rescan();
    return true;
}
bool LibraryStore::importLegacy(const QUrl &directory)
{
    if (!directory.isLocalFile() || !QDir::isAbsolutePath(directory.toLocalFile())
        || directory.hasQuery() || directory.hasFragment()
        || directory.toLocalFile().contains(QChar::Null)) {
        report(QCoreApplication::translate("Yalahoo",
                                           "Legacy import requires a local absolute directory."));
        return false;
    }
    cancel();
    d->m_importPending = directory.toLocalFile();
    schedule();
    return true;
}
void LibraryStore::rescan()
{
    Q_ASSERT(QThread::currentThread() == thread());
    cancel();
    d->m_scanPending = true;
    if (d->m_writable)
        d->m_error.clear();
    schedule();
}
void LibraryStore::cancel()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++d->m_generation;
    if (d->m_cancel)
        d->m_cancel->store(true, std::memory_order_relaxed);
    d->m_scanPending = false;
    d->m_loadPending = false;
    d->m_importPending.clear();
    if (d->m_active == Kind::Probe)
        d->m_probe.cancel();
    Q_EMIT statusChanged();
}
LibraryStore::JobResult LibraryStore::read(const QString &path)
{
    JobResult result;
    QFile file(path);
    if (!file.exists()) {
        result.snapshot = std::make_shared<const LibrarySnapshot>();
        return result;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumCacheBytes) {
        result.error = QCoreApplication::translate(
                "Yalahoo",
                "Cannot read the library cache, or it exceeds 256 MiB. The original file was preserved.");
        return result;
    }
    QJsonParseError parse;
    const auto bytes = file.read(maximumCacheBytes + 1);
    const auto document = bytes.size() <= maximumCacheBytes ? QJsonDocument::fromJson(bytes, &parse)
                                                            : QJsonDocument();
    LibrarySnapshot snapshot;
    if (file.error() != QFileDevice::NoError || parse.error != QJsonParseError::NoError
        || !document.isObject() || document.object().value(u"nativeSchemaVersion"_s).toInt() != 1
        || !readLibrary(document.object(), snapshot, &result.error)) {
        result.error =
                QCoreApplication::translate(
                        "Yalahoo", "Invalid library cache. The original file was preserved: ")
                + result.error;
        return result;
    }
    QList<QUrl> roots, normalized;
    for (const auto &root : snapshot.roots)
        roots.append(root.path);
    if (!DirectoryScanner::normalizeRoots(roots, normalized, result.error) || roots != normalized) {
        result.error = QCoreApplication::translate(
                "Yalahoo", "Invalid library cache roots. The original file was preserved.");
        return result;
    }
    result.snapshot = std::make_shared<const LibrarySnapshot>(std::move(snapshot));
    return result;
}
LibraryStore::JobResult LibraryStore::write(const QString &path, Snapshot snapshot)
{
    JobResult result;
    auto json = toJson(*snapshot);
    json.insert(u"nativeSchemaVersion"_s, 1);
    const auto bytes = QJsonDocument(json).toJson();
    if (bytes.size() > maximumCacheBytes) {
        result.error = QCoreApplication::translate(
                "Yalahoo", "The library cache exceeds 256 MiB. The original file was preserved.");
        return result;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        result.error = QCoreApplication::translate("Yalahoo",
                                                   "Cannot create the library cache directory.");
        return result;
    }
    if (QFileInfo(path).isSymLink()) {
        result.error = QCoreApplication::translate(
                "Yalahoo",
                "The native library cache cannot overwrite other files through symbolic links.");
        return result;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = QCoreApplication::translate("Yalahoo", "Cannot save the library cache: ")
                + file.errorString();
        return result;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        result.error =
                QCoreApplication::translate("Yalahoo", "Cannot set library cache permissions: ")
                + file.errorString();
        file.cancelWriting();
        return result;
    }
    if (file.write(bytes) != bytes.size() || !file.commit())
        result.error =
                QCoreApplication::translate("Yalahoo", "Cannot atomically save the library cache: ")
                + file.errorString();
    return result;
}
// One worker owns all filesystem transactions. Prefer the latest scan intent and coalesce saves,
// so canceled work cannot create an unbounded queue or overwrite a newer accepted snapshot.
void LibraryStore::schedule()
{
    if (d->m_active != Kind::None) {
        Q_EMIT statusChanged();
        return;
    }
    d->m_activeGeneration = d->m_generation;
    if (d->m_savePending || d->m_loadPending || !d->m_initialized || !d->m_importPending.isEmpty()
        || d->m_scanPending)
        qCInfo(logLibrary) << "Library job scheduled" << "generation=" << d->m_generation
                           << "cache=" << d->m_path << "roots=" << d->m_roots
                           << "save=" << bool(d->m_savePending) << "scan=" << d->m_scanPending
                           << "import=" << d->m_importPending;
    if (d->m_metadataCommit && d->m_initialized && !d->m_loadPending) {
        d->m_metadataCommit = false;
        d->m_active = Kind::Metadata;
        auto pending = std::exchange(d->m_metadataPending, { });
        d->m_metadataBytes = 0;
        d->m_job.setFuture(QtConcurrent::run(
                &d->m_worker, [snapshot = d->m_snapshot, pending = std::move(pending)] {
                    auto updated = *snapshot;
                    for (auto &album : updated.albums)
                        for (const auto kind : { MetadataKind::Introduction, MetadataKind::Credits,
                                                 MetadataKind::Enrichment }) {
                            const auto it = pending.constFind(album.id + '/'_L1 + kindName(kind));
                            if (it != pending.cend() && it->inputKey == metadataKey(album, kind))
                                mergeMetadata(album, *it);
                        }
                    return JobResult{
                        std::make_shared<const LibrarySnapshot>(std::move(updated)), { }, 0, false
                    };
                }));
    } else if (d->m_savePending) {
        d->m_active = Kind::Save;
        d->m_job.setFuture(QtConcurrent::run(&d->m_worker, &LibraryStore::write, d->m_path,
                                             std::exchange(d->m_savePending, { })));
    } else if (d->m_loadPending || !d->m_initialized) {
        d->m_loadPending = false;
        d->m_active = Kind::Load;
        d->m_job.setFuture(QtConcurrent::run(&d->m_worker, &LibraryStore::read, d->m_path));
    } else if (!d->m_importPending.isEmpty()) {
        d->m_active = Kind::Import;
        d->m_cancel = std::make_shared<std::atomic_bool>(false);
        d->m_job.setFuture(QtConcurrent::run(
                &d->m_worker,
                [source = std::exchange(d->m_importPending, { }), root = d->m_dataRoot,
                 cancel = d->m_cancel] {
                    const auto imported = LegacyImporter::read(source, root, cancel);
                    return JobResult{ imported.snapshot, imported.error, 0, imported.cancelled };
                }));
    } else if (d->m_scanPending) {
        d->m_scanPending = false;
        d->m_active = Kind::Scan;
        d->m_cancel = std::make_shared<std::atomic_bool>(false);
        d->m_job.setFuture(QtConcurrent::run(
                &d->m_worker, [roots = d->m_roots, previous = d->m_snapshot, cancel = d->m_cancel] {
                    const auto scan = DirectoryScanner::scan(roots, previous, cancel);
                    return JobResult{ scan.snapshot, scan.error, scan.filesVisited,
                                      scan.cancelled };
                }));
    }
    Q_EMIT statusChanged();
}
// Completion is a publication boundary: accept only the job generation still owned by this store.
// Scheduling the next coalesced intent happens after the candidate has been reconciled.
void LibraryStore::complete()
{
    const auto kind = std::exchange(d->m_active, Kind::None);
    const auto generation = d->m_activeGeneration;
    const auto result = d->m_job.result();
    qCInfo(logLibrary) << "Library job completed" << "kind=" << int(kind)
                       << "generation=" << generation << "currentGeneration=" << d->m_generation
                       << "filesVisited=" << result.filesVisited << "cancelled=" << result.cancelled
                       << "error=" << result.error;
    if (kind == Kind::Scan && generation == d->m_generation && result.snapshot && !result.cancelled
        && result.error.isEmpty()) {
        d->m_active = Kind::Probe;
        d->m_filesVisited = result.filesVisited;
        d->m_probe.start(result.snapshot, d->m_dataRoot, d->m_cancel, generation);
        Q_EMIT statusChanged();
        return;
    }
    d->m_cancel.reset();
    if (kind == Kind::Metadata && result.snapshot) {
        d->m_snapshot = result.snapshot;
        d->m_savePending = d->m_snapshot;
        Q_EMIT metadataCommitted(d->m_snapshot);
    } else if (kind == Kind::Save) {
        if (result.error.isEmpty())
            Q_EMIT saved();
        else
            report(result.error);
    } else if (kind == Kind::Load) {
        d->m_initialized = true;
        // A root change during loading invalidates display intent, not the cache's
        // safety check or the baseline needed to preserve metadata/offline roots.
        if (!result.error.isEmpty()) {
            d->m_writable = false;
            report(result.error);
        } else if (result.snapshot) {
            d->m_writable = true;
            d->m_error.clear();
            d->m_snapshot = result.snapshot;
            if (generation == d->m_generation && !d->m_scanPending
                && d->m_importPending.isEmpty()) {
                d->m_roots.clear();
                for (const auto &root : d->m_snapshot->roots)
                    d->m_roots.append(root.path);
                Q_EMIT snapshotReady(d->m_snapshot);
            }
        }
    } else {
        accept(result, kind, generation);
    }
    schedule();
}
void LibraryStore::accept(JobResult result, Kind kind, quint64 generation)
{
    if (generation != d->m_generation || result.cancelled)
        return;
    if (!result.error.isEmpty()) {
        report(result.error);
    } else if (result.snapshot) {
        if (d->m_writable)
            d->m_error.clear();
        d->m_snapshot = result.snapshot;
        if (kind == Kind::Import) {
            d->m_roots.clear();
            for (const auto &root : d->m_snapshot->roots)
                d->m_roots.append(root.path);
        }
        if (d->m_writable)
            d->m_savePending = d->m_snapshot;
        Q_EMIT snapshotReady(d->m_snapshot);
        if (kind == Kind::Scan)
            Q_EMIT scanFinished(generation, result.filesVisited);
    }
}
}
