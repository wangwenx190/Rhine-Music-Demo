#include "LibraryStore.h"
#include "LegacyImporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QThread>
#include <QtConcurrentRun>
#include <utility>

namespace yalahoo {
namespace {
constexpr qint64 maximumCacheBytes = 256 * 1024 * 1024;
}
LibraryStore::LibraryStore(QString dataRoot, QObject *parent)
    : QObject(parent),
      m_path(QDir(dataRoot).filePath(QStringLiteral("library-index.json"))),
      m_snapshot(std::make_shared<const LibrarySnapshot>()),
      m_dataRoot(std::move(dataRoot))
{
    qRegisterMetaType<Snapshot>();
    connect(&m_probe, &MediaMetadataProbe::finished, this,
            [this](quint64 generation, Snapshot snapshot, bool cancelled) {
                if (m_active != Kind::Probe || generation != m_activeGeneration)
                    return;
                m_active = Kind::None;
                m_cancel.reset();
                accept({ snapshot, { }, m_filesVisited, cancelled }, Kind::Scan, generation);
                schedule();
            });
    m_worker.setMaxThreadCount(1);
    connect(&m_job, &QFutureWatcher<JobResult>::finished, this, &LibraryStore::complete);
}
LibraryStore::~LibraryStore()
{
    if (m_cancel)
        m_cancel->store(true, std::memory_order_relaxed);
    // Only teardown waits. A scan that never reached GUI acceptance is never saved.
    m_worker.waitForDone();
    if (m_savePending && m_writable)
        write(m_path, m_savePending);
}
bool LibraryStore::busy() const
{
    return m_active != Kind::None || m_scanPending || m_loadPending || !m_importPending.isEmpty()
            || bool(m_savePending);
}
bool LibraryStore::scanning() const
{
    return m_scanPending
            || ((m_active == Kind::Scan || m_active == Kind::Probe)
                && m_activeGeneration == m_generation);
}
bool LibraryStore::probing() const
{
    return m_active == Kind::Probe && m_activeGeneration == m_generation;
}
QString LibraryStore::error() const
{
    return m_error;
}
QList<QUrl> LibraryStore::roots() const
{
    return m_roots;
}
Snapshot LibraryStore::snapshot() const
{
    return m_snapshot;
}
void LibraryStore::report(const QString &error)
{
    m_error = error;
    emit statusChanged();
    if (!error.isEmpty())
        emit failed(error);
}
void LibraryStore::load()
{
    Q_ASSERT(QThread::currentThread() == thread());
    cancel();
    m_loadPending = true;
    schedule();
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
    m_roots = std::move(normalized);
    rescan();
    return true;
}
bool LibraryStore::importLegacy(const QUrl &directory)
{
    if (!directory.isLocalFile() || !QDir::isAbsolutePath(directory.toLocalFile())
        || directory.hasQuery() || directory.hasFragment()
        || directory.toLocalFile().contains(QChar::Null)) {
        report(QStringLiteral("旧数据导入需要本地绝对目录。"));
        return false;
    }
    cancel();
    m_importPending = directory.toLocalFile();
    schedule();
    return true;
}
void LibraryStore::rescan()
{
    Q_ASSERT(QThread::currentThread() == thread());
    cancel();
    m_scanPending = true;
    if (m_writable)
        m_error.clear();
    schedule();
}
void LibraryStore::cancel()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_generation;
    if (m_cancel)
        m_cancel->store(true, std::memory_order_relaxed);
    m_scanPending = false;
    m_loadPending = false;
    m_importPending.clear();
    if (m_active == Kind::Probe)
        m_probe.cancel();
    emit statusChanged();
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
        result.error = QStringLiteral("无法读取音乐库缓存或缓存超过 256MiB，保留原文件。");
        return result;
    }
    QJsonParseError parse;
    const auto bytes = file.read(maximumCacheBytes + 1);
    const auto document = bytes.size() <= maximumCacheBytes ? QJsonDocument::fromJson(bytes, &parse)
                                                            : QJsonDocument();
    LibrarySnapshot snapshot;
    if (file.error() != QFileDevice::NoError || parse.error != QJsonParseError::NoError
        || !document.isObject() || document.object().value("nativeSchemaVersion").toInt() != 1
        || !readLibrary(document.object(), snapshot, &result.error)) {
        result.error = QStringLiteral("音乐库缓存无效，保留原文件：") + result.error;
        return result;
    }
    QList<QUrl> roots, normalized;
    for (const auto &root : snapshot.roots)
        roots.append(root.path);
    if (!DirectoryScanner::normalizeRoots(roots, normalized, result.error) || roots != normalized) {
        result.error = QStringLiteral("音乐库缓存根目录无效，保留原文件。");
        return result;
    }
    result.snapshot = std::make_shared<const LibrarySnapshot>(std::move(snapshot));
    return result;
}
LibraryStore::JobResult LibraryStore::write(const QString &path, Snapshot snapshot)
{
    JobResult result;
    auto json = toJson(*snapshot);
    json.insert("nativeSchemaVersion", 1);
    const auto bytes = QJsonDocument(json).toJson();
    if (bytes.size() > maximumCacheBytes) {
        result.error = QStringLiteral("音乐库缓存超过 256MiB，保留原文件。");
        return result;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        result.error = QStringLiteral("无法创建音乐库缓存目录。");
        return result;
    }
    if (QFileInfo(path).isSymLink()) {
        result.error = QStringLiteral("原生音乐库缓存不能通过符号链接覆盖其他文件。");
        return result;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = QStringLiteral("无法保存音乐库缓存：") + file.errorString();
        return result;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        result.error = QStringLiteral("无法设置音乐库缓存权限：") + file.errorString();
        file.cancelWriting();
        return result;
    }
    if (file.write(bytes) != bytes.size() || !file.commit())
        result.error = QStringLiteral("无法原子保存音乐库缓存：") + file.errorString();
    return result;
}
void LibraryStore::schedule()
{
    if (m_active != Kind::None) {
        emit statusChanged();
        return;
    }
    m_activeGeneration = m_generation;
    if (m_savePending) {
        m_active = Kind::Save;
        m_job.setFuture(QtConcurrent::run(&m_worker, &LibraryStore::write, m_path,
                                          std::exchange(m_savePending, { })));
    } else if (m_loadPending || !m_initialized) {
        m_loadPending = false;
        m_active = Kind::Load;
        m_job.setFuture(QtConcurrent::run(&m_worker, &LibraryStore::read, m_path));
    } else if (!m_importPending.isEmpty()) {
        m_active = Kind::Import;
        m_cancel = std::make_shared<std::atomic_bool>(false);
        m_job.setFuture(QtConcurrent::run(&m_worker,
                                          [source = std::exchange(m_importPending, { }),
                                           root = m_dataRoot, cancel = m_cancel] {
                                              const auto imported =
                                                      LegacyImporter::read(source, root, cancel);
                                              return JobResult{ imported.snapshot, imported.error,
                                                                0, imported.cancelled };
                                          }));
    } else if (m_scanPending) {
        m_scanPending = false;
        m_active = Kind::Scan;
        m_cancel = std::make_shared<std::atomic_bool>(false);
        m_job.setFuture(QtConcurrent::run(
                &m_worker, [roots = m_roots, previous = m_snapshot, cancel = m_cancel] {
                    const auto scan = DirectoryScanner::scan(roots, previous, cancel);
                    return JobResult{ scan.snapshot, scan.error, scan.filesVisited,
                                      scan.cancelled };
                }));
    }
    emit statusChanged();
}
void LibraryStore::complete()
{
    const auto kind = std::exchange(m_active, Kind::None);
    const auto generation = m_activeGeneration;
    const auto result = m_job.result();
    if (kind == Kind::Scan && generation == m_generation && result.snapshot && !result.cancelled
        && result.error.isEmpty()) {
        m_active = Kind::Probe;
        m_filesVisited = result.filesVisited;
        m_probe.start(result.snapshot, m_dataRoot, m_cancel, generation);
        emit statusChanged();
        return;
    }
    m_cancel.reset();
    if (kind == Kind::Save) {
        if (result.error.isEmpty())
            emit saved();
        else
            report(result.error);
    } else if (kind == Kind::Load) {
        m_initialized = true;
        // A root change during loading invalidates display intent, not the cache's
        // safety check or the baseline needed to preserve metadata/offline roots.
        if (!result.error.isEmpty()) {
            m_writable = false;
            report(result.error);
        } else if (result.snapshot) {
            m_writable = true;
            m_error.clear();
            m_snapshot = result.snapshot;
            if (generation == m_generation && !m_scanPending && m_importPending.isEmpty()) {
                m_roots.clear();
                for (const auto &root : m_snapshot->roots)
                    m_roots.append(root.path);
                emit snapshotReady(m_snapshot);
            }
        }
    } else {
        accept(result, kind, generation);
    }
    schedule();
}
void LibraryStore::accept(JobResult result, Kind kind, quint64 generation)
{
    if (generation != m_generation || result.cancelled)
        return;
    if (!result.error.isEmpty()) {
        report(result.error);
    } else if (result.snapshot) {
        if (m_writable)
            m_error.clear();
        m_snapshot = result.snapshot;
        if (kind == Kind::Import) {
            m_roots.clear();
            for (const auto &root : m_snapshot->roots)
                m_roots.append(root.path);
        }
        if (m_writable)
            m_savePending = m_snapshot;
        emit snapshotReady(m_snapshot);
        if (kind == Kind::Scan)
            emit scanFinished(generation, result.filesVisited);
    }
}
}
