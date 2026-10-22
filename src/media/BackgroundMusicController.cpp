#include "BackgroundMusicController.h"
#include <memory>
#include "ProductDefaults.h"
#include <QAudioOutput>
#include <QAudioDevice>
#include <QMediaPlayer>
#include <QMediaDevices>
#include <QElapsedTimer>
#include <QTimer>
#include "BackgroundMusicController_p.h"
#include <utility>
#include <QCoreApplication>
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
    : QObject(parent), d(std::make_unique<Private>(this))
{
    d->m_output.setVolume(0);
    d->m_player.setAudioOutput(&d->m_output);
    d->m_player.setLoops(QMediaPlayer::Infinite);
    d->m_player.setPlaybackRate(1);
    d->m_fade.setInterval(FadeTickMilliseconds);
    d->m_fade.setTimerType(Qt::PreciseTimer);
    connect(&d->m_fade, &QTimer::timeout, this, [this] {
        d->advanceFade();
    });
    connect(&d->m_player, &QMediaPlayer::playbackStateChanged, this, [this] {
        Q_EMIT stateChanged();
    });
    connect(&d->m_player, &QMediaPlayer::errorOccurred, this,
            [this](auto error, const QString &reason) {
                if (error == QMediaPlayer::NoError || error != d->m_player.error())
                    return;
                qCWarning(logMedia) << "Background music error" << "source=" << d->m_player.source()
                                    << "code=" << int(error) << "reason=" << reason;
                d->m_error = reason.isEmpty()
                        ? QCoreApplication::translate("Yalahoo",
                                                      "Qt cannot play the background music.")
                        : reason;
                d->m_fade.stop();
                d->m_target = 0;
                d->m_output.setVolume(0);
                d->m_player.pause();
                Q_EMIT silenceReached();
                Q_EMIT stateChanged();
            });
    d->m_outputRefresh.setSingleShot(true);
    connect(&d->m_devices, &QMediaDevices::audioOutputsChanged, &d->m_outputRefresh,
            qOverload<>(&QTimer::start));
    connect(&d->m_outputRefresh, &QTimer::timeout, this, [this] {
        d->refreshOutput();
    });
    d->refreshOutput();
}

BackgroundMusicController::~BackgroundMusicController()
{
    stop();
    d->m_player.setSource({ });
    d->m_player.setAudioOutput(nullptr);
}
void BackgroundMusicController::start()
{
    if (d->m_active)
        return;
    d->m_active = true;
    d->reconcile();
}
void BackgroundMusicController::stop()
{
    d->m_active = false;
    d->m_fade.stop();
    d->m_target = 0;
    d->m_output.setVolume(0);
    d->m_player.stop();
    Q_EMIT silenceReached();
}
void BackgroundMusicController::setEnabled(bool enabled)
{
    if (d->m_enabled == enabled)
        return;
    d->m_enabled = enabled;
    if (enabled)
        d->m_error.clear();
    d->reconcile();
    Q_EMIT settingsChanged();
    Q_EMIT stateChanged();
}
void BackgroundMusicController::setVolume(double volume)
{
    if (!std::isfinite(volume))
        return;
    volume = std::clamp(volume, 0., 1.);
    if (d->m_volume == volume)
        return;
    d->m_volume = volume;
    d->reconcile();
    Q_EMIT settingsChanged();
}
void BackgroundMusicController::setSongActivity(SongActivity activity)
{
    if (d->m_song == activity)
        return;
    d->m_song = activity;
    d->reconcile();
}
void BackgroundMusicController::Private::refreshOutput()
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
        Q_EMIT q->silenceReached();
    }
    reconcile();
}
void BackgroundMusicController::Private::reconcile()
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
void BackgroundMusicController::Private::advanceFade()
{
    const double progress = std::min(1., double(m_elapsed.elapsed()) / m_duration);
    m_output.setVolume(float(m_from + (m_target - m_from) * progress));
    if (progress < 1 && m_from != m_target)
        return;
    m_fade.stop();
    m_output.setVolume(float(m_target));
    if (m_target == 0) {
        m_player.pause();
        Q_EMIT q->silenceReached();
    }
}
bool BackgroundMusicController::enabled() const
{
    return d->m_enabled;
}
double BackgroundMusicController::volume() const
{
    return d->m_volume;
}
bool BackgroundMusicController::playing() const
{
    return d->m_player.isPlaying();
}
QString BackgroundMusicController::error() const
{
    return d->m_error;
}
bool BackgroundMusicController::silent() const
{
    return d->m_output.volume() == 0;
}

}
