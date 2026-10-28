// Internal implementation detail. Include only from the owner, its collaborating peer or tests.
#pragma once
#include "PlaybackController.h"
#include <memory>
#include <QTimer>
#include "PlaybackQueue.h"
#include "BackgroundMusicController.h"
#include <QPointer>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
namespace yalahoo {
struct PlaybackController::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(PlaybackController *owner, LibrarySession *library)
        : q(owner),
          m_library(library),
          m_devices(owner),
          m_outputRefresh(owner),
          m_output(owner),
          m_player(owner)
    {
    }
    PlaybackController *const q;

    void restoreOutput(const QAudioDevice &selected);
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
    QTimer m_outputRefresh;
    QAudioOutput m_output;
    QMediaPlayer m_player;
    QList<QMetaObject::Connection> m_connections;
    PlaybackQueue m_queue;
    QList<QAudioDevice> m_outputs;
    QUrl m_source;
    QByteArray m_preferred;
    QString m_error, m_notice;
    Intent m_intent = Intent::Stopped;
    MediaSupport m_support = MediaSupport::Unknown;
    quint64 m_revision = 0, m_endedRevision = 0;
    qint64 m_position = 0, m_lastPosition = 0, m_duration = 0, m_restore = -1, m_preview = 0;
    bool m_seekable = false, m_dragging = false, m_unavailable = true;
};
}
