#pragma once
#include "LibrarySession.h"
#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>

namespace yalahoo {
class PlaybackTest;
// GUI-thread, ordinary local-file playback. No visual clock or PCM ownership.
class PlaybackController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY currentChanged)
    Q_PROPERTY(QString albumId READ albumId NOTIFY currentChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY currentChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool requested READ requested NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY devicesChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(bool dragging READ dragging NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString selectedDevice READ selectedDevice NOTIFY devicesChanged)
    Q_PROPERTY(bool outputAvailable READ outputAvailable NOTIFY devicesChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY currentChanged)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY currentChanged)
public:
    explicit PlaybackController(LibrarySession *library, QObject *parent = nullptr);
    ~PlaybackController() override;
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
    QString selectedDevice() const
    {
        return QString::fromLatin1(m_preferred.toBase64());
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
signals:
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
    enum class Intent {
        Stopped,
        Paused,
        Playing
    };
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
    LibrarySession *m_library;
    QMediaDevices m_devices;
    QAudioOutput m_output;
    QMediaPlayer m_player;
    QList<QMetaObject::Connection> m_connections;
    QList<Track> m_queue;
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
