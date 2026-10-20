#pragma once
#include <memory>
#include "DirectoryScanner.h"
#include <QMediaMetaData>
#include <QObject>
#include <functional>

namespace yalahoo {
struct MediaObservation final
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
    Q_DISABLE_COPY_MOVE(MetadataSource)
public:
    MetadataSource() = default;
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
    Q_DISABLE_COPY_MOVE(MediaMetadataProbe)
public:
    using Factory = std::function<MetadataSource *(QObject *)>;
    explicit MediaMetadataProbe(QObject *parent = nullptr, Factory factory = { });
    ~MediaMetadataProbe() override;
    void start(Snapshot snapshot, QString dataRoot, ScanCancellation cancel, quint64 generation);
    void cancel();
Q_SIGNALS:
    void finished(quint64 generation, yalahoo::Snapshot snapshot, bool cancelled);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
