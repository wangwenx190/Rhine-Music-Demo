#include <QCoreApplication>
#include "BackgroundMusicController.h"
#include "Logging.h"
#include <algorithm>
#include <cmath>

namespace {
// Timer ticks only wake the fade; actual progress uses monotonic elapsed milliseconds.
constexpr auto FadeTickMilliseconds = 24;
constexpr auto FadeInMilliseconds = 650;
constexpr auto FadeOutMilliseconds = 220;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
BackgroundMusicController::BackgroundMusicController(QObject *parent)
    : QObject(parent), m_devices(this), m_output(this), m_player(this), m_fade(this)
{
    m_output.setVolume(0);
    m_player.setAudioOutput(&m_output);
    m_player.setLoops(QMediaPlayer::Infinite);
    m_player.setPlaybackRate(1);
    m_fade.setInterval(FadeTickMilliseconds);
    m_fade.setTimerType(Qt::PreciseTimer);
    connect(&m_fade, &QTimer::timeout, this, &BackgroundMusicController::advanceFade);
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this] {
        Q_EMIT stateChanged();
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](auto error, const QString &reason) {
                if (error == QMediaPlayer::NoError || error != m_player.error())
                    return;
                qCWarning(logMedia) << "Background music error" << "source=" << m_player.source()
                                    << "code=" << int(error) << "reason=" << reason;
                m_error = reason.isEmpty()
                        ? QCoreApplication::translate("Yalahoo",
                                                      "Qt cannot play the background music.")
                        : reason;
                m_fade.stop();
                m_target = 0;
                m_output.setVolume(0);
                m_player.pause();
                Q_EMIT silenceReached();
                Q_EMIT stateChanged();
            });
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this,
            &BackgroundMusicController::scheduleOutputRefresh);
    refreshOutput();
}
void BackgroundMusicController::scheduleOutputRefresh()
{
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshPending = false;
        refreshOutput();
    });
}
BackgroundMusicController::~BackgroundMusicController()
{
    stop();
    m_player.setSource({ });
    m_player.setAudioOutput(nullptr);
}
void BackgroundMusicController::start()
{
    if (m_active)
        return;
    m_active = true;
    reconcile();
}
void BackgroundMusicController::stop()
{
    m_active = false;
    m_fade.stop();
    m_target = 0;
    m_output.setVolume(0);
    m_player.stop();
    Q_EMIT silenceReached();
}
void BackgroundMusicController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (enabled)
        m_error.clear();
    reconcile();
    Q_EMIT settingsChanged();
    Q_EMIT stateChanged();
}
void BackgroundMusicController::setVolume(double volume)
{
    if (!std::isfinite(volume))
        return;
    volume = std::clamp(volume, 0., 1.);
    if (m_volume == volume)
        return;
    m_volume = volume;
    reconcile();
    Q_EMIT settingsChanged();
}
void BackgroundMusicController::setSongActivity(SongActivity activity)
{
    if (m_song == activity)
        return;
    m_song = activity;
    reconcile();
}
void BackgroundMusicController::refreshOutput()
{
    const auto device = QMediaDevices::defaultAudioOutput();
    m_available = !device.isNull();
    if (m_available)
        m_output.setDevice(device);
    else {
        m_fade.stop();
        m_target = 0;
        m_output.setVolume(0);
        m_player.pause();
        Q_EMIT silenceReached();
    }
    reconcile();
}
void BackgroundMusicController::reconcile()
{
    const double target = m_active && m_enabled && m_available && m_error.isEmpty()
                    && m_song == SongActivity::Stopped
            ? m_volume
            : 0;
    if (target == m_target)
        return;
    m_target = target;
    m_from = m_output.volume();
    m_duration = target > 0 ? FadeInMilliseconds : FadeOutMilliseconds;
    m_elapsed.start();
    if (target > 0) {
        if (m_player.source().isEmpty() || m_player.error() != QMediaPlayer::NoError)
            m_player.setSource(QUrl(u"qrc:/assets/audio/atmosphere.ogg"_s));
        // A synchronous backend error may already have cancelled this transition.
        if (!m_error.isEmpty())
            return;
        m_player.play();
    }
    if (m_from == target) {
        advanceFade();
        return;
    }
    m_fade.start();
}
void BackgroundMusicController::advanceFade()
{
    const double progress = std::min(1., double(m_elapsed.elapsed()) / m_duration);
    m_output.setVolume(float(m_from + (m_target - m_from) * progress));
    if (progress < 1 && m_from != m_target)
        return;
    m_fade.stop();
    m_output.setVolume(float(m_target));
    if (m_target == 0) {
        m_player.pause();
        Q_EMIT silenceReached();
    }
}
}
