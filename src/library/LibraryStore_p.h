// Internal worker state shared only with the deterministic queued-completion regression.
// The public store header keeps worker, watcher and media dependencies private.
#pragma once
#include "LibraryStore.h"
#include "DirectoryScanner.h"
#include "IMetadataProvider.h"
#include "MediaMetadataProbe.h"
#include <QFutureWatcher>
#include <QHash>
#include <QThreadPool>

namespace yalahoo {
struct LibraryStore::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(LibraryStore *owner) : q(owner)
    {
    }
    LibraryStore *const q;

    enum class Kind {
        None,
        Load,
        Scan,
        Probe,
        Import,
        Save,
        Metadata,
    };
    struct JobResult final
    {
        Snapshot snapshot;
        QString error;
        quint64 filesVisited = 0;
        bool cancelled = false;
    };
    static JobResult read(const QString &path);
    static JobResult write(const QString &path, const Snapshot &snapshot);
    void schedule();
    void startMetadata();
    void startSave();
    void startLoad();
    void startImport();
    void startScan();

    void complete();
    void completeLoad(const JobResult &result, quint64 generation);
    void accept(const JobResult &result, Kind kind, quint64 generation);
    void report(const QString &error);
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
}
