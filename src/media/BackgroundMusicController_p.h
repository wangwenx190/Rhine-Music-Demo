// Internal implementation detail. Include only from the owner, its collaborating peer or tests.
#pragma once
#include "BackgroundMusicController.h"
#include <memory>
#include "ProductDefaults.h"
#include <QAudioOutput>
#include <QAudioDevice>
#include <QMediaPlayer>
#include <QMediaDevices>
#include <QElapsedTimer>
#include <QTimer>
namespace yalahoo {
struct BackgroundMusicController::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(BackgroundMusicController *owner)
        : q(owner),
          m_devices(owner),
          m_outputRefresh(owner),
          m_output(owner),
          m_player(owner),
          m_fade(owner)
    {
    }
    BackgroundMusicController *const q;

    void reconcile();
    void advanceFade();
    void refreshOutput();
    QMediaDevices m_devices;
    QTimer m_outputRefresh;
    QAudioOutput m_output;
    QMediaPlayer m_player;
    QTimer m_fade;
    QElapsedTimer m_elapsed;
    QString m_error;
    SongActivity m_song = SongActivity::Stopped;
    double m_volume = yalahoo::productDefaults::DefaultBackgroundVolume, m_from = 0, m_target = 0;
    int m_duration = 0;
    bool m_enabled = true, m_active = false, m_available = false;
};
}
