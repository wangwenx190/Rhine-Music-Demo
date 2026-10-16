#pragma once

#include "DirectoryScanner.h"
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>

namespace yalahoo {
// One running job, one latest scan intent and one coalesced accepted save.
class LibraryStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged)
    Q_PROPERTY(QList<QUrl> roots READ roots NOTIFY statusChanged)
public:
    explicit LibraryStore(QString dataRoot, QObject *parent = nullptr);
    ~LibraryStore() override;
    void load();
    Q_INVOKABLE bool setRoots(const QList<QUrl> &roots);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void cancel();
    bool busy() const;
    bool scanning() const;
    QString error() const;
    QList<QUrl> roots() const;
    Snapshot snapshot() const;
signals:
    void statusChanged();
    void snapshotReady(yalahoo::Snapshot snapshot);
    void saved();
    void failed(const QString &error);
    void scanFinished(quint64 generation, quint64 filesVisited);

private:
    enum class Kind {
        None,
        Load,
        Scan,
        Save
    };
    struct JobResult
    {
        Snapshot snapshot;
        QString error;
        quint64 filesVisited = 0;
        bool cancelled = false;
    };
    static JobResult read(const QString &path);
    static JobResult write(const QString &path, Snapshot snapshot);
    void schedule();
    void complete();
    void report(const QString &error);
    QString m_path, m_error;
    QList<QUrl> m_roots;
    Snapshot m_snapshot, m_savePending;
    QThreadPool m_worker;
    QFutureWatcher<JobResult> m_job;
    ScanCancellation m_cancel;
    Kind m_active = Kind::None;
    quint64 m_generation = 0, m_activeGeneration = 0;
    bool m_loadPending = false, m_scanPending = false, m_writable = true, m_initialized = false;
};
}
