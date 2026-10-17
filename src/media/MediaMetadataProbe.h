#pragma once
#include "DirectoryScanner.h"
#include <QMediaMetaData>
#include <QObject>
#include <QThread>
#include <functional>

namespace yalahoo {
struct MediaObservation
{
    enum Status {
        Loading,
        Loaded,
        Invalid
    };
    Status status = Loading;
    QMediaMetaData metadata;
    qint64 durationMs = 0;
    bool hasAudio = false;
    QString error;
};
// Test injection shares the production scheduler, timeout and cancellation path.
class MetadataSource : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void open(const QUrl &url) = 0;
    virtual void clear() = 0;
    virtual MediaObservation observation() const = 0;
Q_SIGNALS:
    void changed();
};
class MediaMetadataProbe final : public QObject
{
    Q_OBJECT
public:
    using Factory = std::function<MetadataSource *(QObject *)>;
    explicit MediaMetadataProbe(QObject *parent = nullptr, Factory factory = { });
    ~MediaMetadataProbe() override;
    void start(Snapshot snapshot, QString dataRoot, ScanCancellation cancel, quint64 generation);
    void cancel();
Q_SIGNALS:
    void finished(quint64 generation, yalahoo::Snapshot snapshot, bool cancelled);

private:
    QThread m_thread;
    QObject *m_worker = nullptr;
};
}
