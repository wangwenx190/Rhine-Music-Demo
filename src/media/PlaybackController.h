// Owns song playback intent, queue position and output-device selection. Backend notifications
// are reconciled with the latest requested track and pause/stop intent.

#pragma once
#include "MusicTypes.h"
#include "BackgroundMusicController.h"
#include <QPointer>
#include <QHash>
#include <memory>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>

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
    QString error() const
    {
        return m_error;
    }
    QString notice() const
    {
        return m_notice;
    }
    qint64 position() const;
    qint64 duration() const
    {
        return m_duration;
    }
    bool seekable() const
    {
        return m_seekable && !m_source.isEmpty();
    }
    bool dragging() const
    {
        return m_dragging;
    }
    double volume() const
    {
        return m_output.volume();
    }
    void setVolume(double value);
    QVariantList devices() const;
    QVariantList deviceChoices() const;
    QString selectedDevice() const
    {
        return QString::fromUtf8(m_preferred.toBase64());
    }
    bool outputAvailable() const
    {
        return !m_unavailable;
    }
    bool canNext() const
    {
        return m_index >= 0 && m_index + 1 < m_queue.size();
    }
    bool canPrevious() const
    {
        return m_index >= 0;
    }
    QString outputName() const
    {
        return m_output.device().description();
    }
    QUrl source() const
    {
        return m_player.source();
    }
    bool isAvailable() const
    {
        return m_player.isAvailable();
    }
    quint64 sourceRevision() const
    {
        return m_revision;
    }
    MediaSupport mediaSupport() const
    {
        return m_support;
    }
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
    void restoreOutput(const QAudioDevice &selected);
    void scheduleOutputRefresh();
    friend class PlaybackTest;
    enum class Intent {
        Stopped,
        Paused,
        Playing
    };
    void setIntent(Intent intent);
    void clearConnections();
    void load(qint64 restore = 0);
    void ready();
    bool current(quint64 revision, const QUrl &source) const;
    void status(QMediaPlayer::MediaStatus status, quint64 revision);
    void refreshDevices();
    void applyDevices(const QList<QAudioDevice> &devices, const QAudioDevice &systemDefault);
    void start(int index);
    void setSupport(MediaSupport support);
    const Track *track() const;
    QPointer<BackgroundMusicController> m_background;
    QMetaObject::Connection m_silenceConnection;
    LibrarySession *m_library;
    QMediaDevices m_devices;
    QAudioOutput m_output;
    QMediaPlayer m_player;
    QList<QMetaObject::Connection> m_connections;
    QList<Track> m_queue;
    QHash<QString, int> m_queueIndices;
    std::weak_ptr<const LibrarySnapshot> m_queueSnapshot;
    QList<QAudioDevice> m_outputs;
    QUrl m_source;
    QByteArray m_preferred;
    QString m_error, m_notice;
    Intent m_intent = Intent::Stopped;
    MediaSupport m_support = MediaSupport::Unknown;
    quint64 m_revision = 0, m_endedRevision = 0;
    int m_index = -1;
    qint64 m_position = 0, m_lastPosition = 0, m_duration = 0, m_restore = -1, m_preview = 0;
    bool m_seekable = false, m_dragging = false, m_unavailable = true, m_refreshPending = false;
};
}
