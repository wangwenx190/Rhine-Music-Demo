// Serializes scanning, metadata probing and atomic persistence on one worker. Generations reject
// stale results while pending intents are coalesced to bound work.

#pragma once
#include <memory>

#include "MusicTypes.h"
#include <QObject>

namespace yalahoo {
struct ProviderResult;
// One running job, one latest scan intent and one coalesced accepted save.
class LibraryStore final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LibraryStore)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged FINAL)
    Q_PROPERTY(bool scanning READ scanning NOTIFY statusChanged FINAL)
    Q_PROPERTY(bool probing READ probing NOTIFY statusChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged FINAL)
    Q_PROPERTY(QList<QUrl> roots READ roots NOTIFY statusChanged FINAL)
public:
    explicit LibraryStore(QString dataRoot, QObject *parent = nullptr);
    ~LibraryStore() override;
    void load();
    void stageMetadata(const ProviderResult &result);
    void commitMetadata();
    Q_INVOKABLE bool setMetadataConfiguration(const QVariantMap &configuration);
    Q_INVOKABLE bool setRootPaths(const QString &paths);
    Q_INVOKABLE QStringList rootPaths() const;
    Q_INVOKABLE QString localPath(const QUrl &url) const
    {
        return url.toLocalFile();
    }
    Q_INVOKABLE bool setRoots(const QList<QUrl> &roots);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE bool importLegacy(const QUrl &directory);
    Q_INVOKABLE void cancel();
    bool busy() const;
    bool scanning() const;
    bool probing() const;
    QString error() const;
    QList<QUrl> roots() const;
    Snapshot snapshot() const;
Q_SIGNALS:
    void statusChanged();
    void snapshotReady(yalahoo::Snapshot snapshot);
    void metadataCommitted(yalahoo::Snapshot snapshot);
    void metadataConfigurationChanged(yalahoo::Snapshot snapshot);
    void saved();
    void failed(const QString &error);
    void scanFinished(quint64 generation, quint64 filesVisited);

private:
    enum class Kind {
        None,
        Load,
        Scan,
        Probe,
        Import,
        Save,
        Metadata
    };
    struct JobResult final
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
    void accept(JobResult result, Kind kind, quint64 generation);
    void report(const QString &error);
    struct Private;
    std::unique_ptr<Private> d;
};
}
