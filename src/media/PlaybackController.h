// Owns song playback intent, queue position and output-device selection. Backend notifications
// are reconciled with the latest requested track and pause/stop intent.

#pragma once
#include "MusicTypes.h"
#include <QObject>
#include <QVariantList>
#include <memory>

namespace yalahoo {
class BackgroundMusicController;
}

namespace yalahoo {
class LibrarySession;
class PlaybackTest;
// GUI-thread, ordinary local-file playback. No visual clock or PCM ownership.
class PlaybackController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaybackController)
    Q_PROPERTY(QString title READ title NOTIFY currentChanged FINAL)
    Q_PROPERTY(QString albumId READ albumId NOTIFY currentChanged FINAL)
    Q_PROPERTY(QString trackId READ trackId NOTIFY currentChanged FINAL)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool requested READ requested NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString notice READ notice NOTIFY devicesChanged FINAL)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged FINAL)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged FINAL)
    Q_PROPERTY(bool dragging READ dragging NOTIFY positionChanged FINAL)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QVariantList deviceChoices READ deviceChoices NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QString selectedDevice READ selectedDevice NOTIFY devicesChanged FINAL)
    Q_PROPERTY(bool outputAvailable READ outputAvailable NOTIFY devicesChanged FINAL)
    Q_PROPERTY(bool canNext READ canNext NOTIFY currentChanged FINAL)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY currentChanged FINAL)
public:
    explicit PlaybackController(LibrarySession *library, QObject *parent = nullptr);
    ~PlaybackController() override;
    void setBackgroundMusic(BackgroundMusicController *background);
    Q_INVOKABLE bool playTrack(const QString &albumId, const QString &trackId);
    Q_INVOKABLE bool playAlbum(const QString &albumId);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void seek(qint64 position);
    Q_INVOKABLE void beginSeek();
    Q_INVOKABLE void previewSeek(qint64 position);
    Q_INVOKABLE void commitSeek();
    Q_INVOKABLE void cancelSeek();
    Q_INVOKABLE bool selectDevice(const QString &id);
    QString title() const;
    QString albumId() const;
    QString trackId() const;
    QString state() const;
    bool playing() const;
    bool loading() const;
    bool requested() const;
    QString error() const;
    QString notice() const;
    qint64 position() const;
    qint64 duration() const;
    bool seekable() const;
    bool dragging() const;
    double volume() const;
    void setVolume(double value);
    QVariantList devices() const;
    QVariantList deviceChoices() const;
    QString selectedDevice() const;
    bool outputAvailable() const;
    bool canNext() const;
    bool canPrevious() const;
    QString outputName() const;
    QUrl source() const;
    bool isAvailable() const;
    quint64 sourceRevision() const;
    MediaSupport mediaSupport() const;
Q_SIGNALS:
    void rejected(const QString &message);
    void currentChanged();
    void stateChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void volumeChanged();
    void devicesChanged();
    void supportChanged(yalahoo::MediaSupport support);
    void queueFinished();

private:
    friend class PlaybackTest;
    struct Private;
    std::unique_ptr<Private> d;
};
}
