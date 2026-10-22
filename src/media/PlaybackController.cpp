#include "PlaybackController.h"
#include <memory>
#include "PlaybackQueue.h"
#include "BackgroundMusicController.h"
#include <QPointer>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include "LibrarySession_p.h"
#include "PlaybackController_p.h"
#include "ProductDefaults.h"
#include "LibrarySession.h"
#include <QCoreApplication>
#include "Logging.h"
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// Previous restarts the current track only after the authored playback-position threshold.
constexpr auto RestartPreviousThresholdMilliseconds = 3000;
}

using namespace Qt::StringLiterals;

namespace yalahoo {

PlaybackController::PlaybackController(LibrarySession *library, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, library))
{
    d->m_output.setVolume(yalahoo::productDefaults::DefaultSongVolume);
    d->m_player.setPlaybackRate(1);
    d->m_player.setAudioOutput(&d->m_output);
    d->m_outputRefresh.setSingleShot(true);
    connect(&d->m_devices, &QMediaDevices::audioOutputsChanged, &d->m_outputRefresh,
            qOverload<>(&QTimer::start));
    connect(&d->m_outputRefresh, &QTimer::timeout, this, [this] {
        d->refreshDevices();
    });
    connect(&d->m_output, &QAudioOutput::volumeChanged, this, &PlaybackController::volumeChanged);
    d->refreshDevices();
}

PlaybackController::~PlaybackController()
{
    ++d->m_revision;
    d->clearConnections();
    d->m_player.stop();
    d->m_player.setSource({ });
    d->m_player.setAudioOutput(nullptr);
}
void PlaybackController::setBackgroundMusic(BackgroundMusicController *background)
{
    disconnect(d->m_silenceConnection);
    d->m_background = background;
    if (background)
        d->m_silenceConnection =
                connect(background, &BackgroundMusicController::silenceReached, this, [this] {
                    d->ready();
                });
    d->setIntent(d->m_intent);
}
void PlaybackController::Private::setIntent(Private::Intent intent)
{
    if (m_intent != intent)
        qCInfo(logMedia) << "Playback intent changed" << "intent=" << int(intent)
                         << "albumId=" << q->albumId() << "trackId=" << q->trackId()
                         << "positionMs=" << q->position();
    m_intent = intent;
    if (m_background) {
        using Activity = BackgroundMusicController::SongActivity;
        m_background->setSongActivity(intent == Intent::Playing          ? Activity::Playing
                                              : intent == Intent::Paused ? Activity::Paused
                                                                         : Activity::Stopped);
    }
}
const Track *PlaybackController::Private::track() const
{
    return m_queue.current();
}
QString PlaybackController::title() const
{
    return d->track() ? d->track()->title : QString();
}
QString PlaybackController::albumId() const
{
    return d->track() ? d->track()->albumId : QString();
}
QString PlaybackController::trackId() const
{
    return d->track() ? d->track()->id : QString();
}
bool PlaybackController::requested() const
{
    return d->m_intent == Private::Intent::Playing;
}
bool PlaybackController::playing() const
{
    return requested() && !d->m_unavailable && d->m_player.isPlaying();
}
bool PlaybackController::loading() const
{
    return requested() && !d->m_unavailable
            && (d->m_player.mediaStatus() == QMediaPlayer::LoadingMedia
                || d->m_player.mediaStatus() == QMediaPlayer::BufferingMedia
                || d->m_player.source().isEmpty());
}
QString PlaybackController::state() const
{
    if (!d->m_error.isEmpty())
        return u"error"_s;
    if (requested() && d->m_unavailable)
        return u"waiting-output"_s;
    if (loading())
        return u"loading"_s;
    if (playing())
        return u"playing"_s;
    return d->m_intent == Private::Intent::Paused ? u"paused"_s : u"stopped"_s;
}
qint64 PlaybackController::position() const
{
    return d->m_dragging ? d->m_preview : d->m_restore >= 0 ? d->m_restore : d->m_position;
}
bool PlaybackController::playTrack(const QString &albumId, const QString &trackId)
{
    const auto *album = d->m_library->d->album(albumId);
    if (!album || album->offline) {
        Q_EMIT rejected(QCoreApplication::translate(
                "Yalahoo", "The album directory is unavailable. Rescan or choose another track."));
        return false;
    }
    const auto result = d->m_queue.select(d->m_library->snapshot(), *album, trackId);
    if (result != PlaybackQueue::Selection::Accepted) {
        Q_EMIT rejected(
                result == PlaybackQueue::Selection::MissingTrack
                        ? QCoreApplication::translate(
                                  "Yalahoo",
                                  "The track is no longer in the library. Choose another track or rescan.")
                        : QCoreApplication::translate(
                                  "Yalahoo", "Tracks require a local absolute file path."));
        return false;
    }
    d->start(d->m_queue.index());
    return true;
}
bool PlaybackController::playAlbum(const QString &albumId)
{
    const auto *album = d->m_library->d->album(albumId);
    return album && !album->tracks.isEmpty() && playTrack(albumId, album->tracks.first().id);
}
void PlaybackController::Private::start(int index)
{
    if (!m_queue.selectIndex(index)) {
        Q_EMIT q->rejected(QCoreApplication::translate(
                "Yalahoo", "Tracks require a local absolute file path."));
        return;
    }
    m_source = track()->sourceUrl;
    setIntent(Intent::Playing);
    m_error.clear();
    m_duration = m_position = m_lastPosition = 0;
    m_dragging = false;
    m_library->d->adoptPlaybackIds(q->albumId(), q->trackId());
    load();
    Q_EMIT q->currentChanged();
    Q_EMIT q->durationChanged();
    Q_EMIT q->positionChanged();
    Q_EMIT q->seekableChanged();
    Q_EMIT q->stateChanged();
}
void PlaybackController::Private::clearConnections()
{
    for (const auto &connection : std::exchange(m_connections, { }))
        QObject::disconnect(connection);
}
bool PlaybackController::Private::current(quint64 revision, const QUrl &source) const
{
    return revision == m_revision && !m_source.isEmpty() && source == m_source
            && m_player.source() == source;
}
void PlaybackController::Private::load(qint64 restore)
{
    const auto revision = ++m_revision;
    qCInfo(logMedia) << "Loading track" << "revision=" << revision << "albumId=" << q->albumId()
                     << "trackId=" << q->trackId() << "source=" << m_source
                     << "restoreMs=" << restore;
    setSupport(MediaSupport::Unknown);
    clearConnections();
    m_player.stop();
    m_player.setSource({ });
    m_seekable = m_dragging = false;
    Q_EMIT q->seekableChanged();
    m_restore = std::max<qint64>(0, restore);
    m_position = m_restore;
    Q_EMIT q->positionChanged();
    if (m_unavailable || m_source.isEmpty())
        return;
    if (!m_player.isAvailable()) {
        m_error = QCoreApplication::translate("Yalahoo",
                                              "The Qt media playback service is unavailable.");
        setIntent(Intent::Paused);
        Q_EMIT q->stateChanged();
        return;
    }
    const auto source = m_source;
    m_connections.append(QObject::connect(
            &m_player, &QMediaPlayer::mediaStatusChanged, q, [this, revision, source](auto value) {
                if (current(revision, source) && value == m_player.mediaStatus())
                    status(value, revision);
            }));
    m_connections.append(QObject::connect(
            &m_player, &QMediaPlayer::seekableChanged, q, [this, revision, source](bool value) {
                if (!current(revision, source) || value != m_player.isSeekable())
                    return;
                m_seekable = value;
                Q_EMIT q->seekableChanged();
                ready();
            }));
    m_connections.append(QObject::connect(
            &m_player, &QMediaPlayer::durationChanged, q, [this, revision, source](qint64 value) {
                if (!current(revision, source) || value != m_player.duration())
                    return;
                m_duration = std::max<qint64>(0, value);
                Q_EMIT q->durationChanged();
            }));
    m_connections.append(QObject::connect(
            &m_player, &QMediaPlayer::positionChanged, q, [this, revision, source](qint64) {
                if (!current(revision, source) || m_unavailable || m_restore >= 0)
                    return;
                const auto value = m_player.position();
                m_position = std::max<qint64>(0, value);
                if (value > 0) {
                    m_lastPosition = value;
                    if (q->playing() && m_player.hasAudio())
                        setSupport(MediaSupport::Supported);
                }
                if (!m_dragging)
                    Q_EMIT q->positionChanged();
            }));
    m_connections.append(QObject::connect(&m_player, &QMediaPlayer::playbackStateChanged, q,
                                          [this, revision, source](auto value) {
                                              if (!current(revision, source)
                                                  || value != m_player.playbackState())
                                                  return;
                                              if (value == QMediaPlayer::PlayingState) {
                                                  if (!q->requested() || m_unavailable)
                                                      m_player.pause();
                                              }
                                              Q_EMIT q->stateChanged();
                                          }));
    m_connections.append(QObject::connect(
            &m_player, &QMediaPlayer::errorOccurred, q,
            [this, revision, source](auto error, const QString &reason) {
                if (!current(revision, source) || error != m_player.error() || m_unavailable
                    || error == QMediaPlayer::NoError)
                    return;
                qCWarning(logMedia)
                        << "Playback error" << "revision=" << revision << "source=" << source
                        << "code=" << int(error) << "reason=" << reason
                        << "device=" << m_output.device().description();
                m_error = reason.isEmpty()
                        ? QCoreApplication::translate("Yalahoo", "Qt media error: %1")
                                  .arg(int(error))
                        : reason;
                setIntent(Intent::Paused);
                if (error == QMediaPlayer::FormatError)
                    setSupport(MediaSupport::Unsupported);
                Q_EMIT q->stateChanged();
            }));
    m_player.setPlaybackRate(1);
    m_player.setSource(source);
}
void PlaybackController::Private::ready()
{
    const auto value = m_player.mediaStatus();
    if (m_unavailable || m_source.isEmpty() || m_player.source() != m_source
        || (value != QMediaPlayer::LoadedMedia && value != QMediaPlayer::BufferedMedia))
        return;
    if (!m_player.hasAudio()) {
        m_error = QCoreApplication::translate("Yalahoo", "Qt detected no audio track.");
        setIntent(Intent::Paused);
        m_player.pause();
        setSupport(MediaSupport::Unsupported);
        Q_EMIT q->stateChanged();
        return;
    }
    m_duration = std::max<qint64>(0, m_player.duration());
    m_seekable = m_player.isSeekable();
    if (m_restore >= 0) {
        const auto target = m_seekable
                ? (m_duration > 0 ? std::clamp(m_restore, qint64(0), m_duration)
                                  : std::max<qint64>(0, m_restore))
                : 0;
        m_restore = -1;
        m_position = m_lastPosition = target;
        if (m_seekable)
            m_player.setPosition(target);
        Q_EMIT q->positionChanged();
    }
    if (q->requested() && m_background && !m_background->silent())
        return;
    if (q->requested() && !m_player.isPlaying())
        m_player.play();
    else if (m_intent == Intent::Paused)
        m_player.pause();
    Q_EMIT q->durationChanged();
    Q_EMIT q->seekableChanged();
    Q_EMIT q->stateChanged();
}
void PlaybackController::Private::status(QMediaPlayer::MediaStatus value, quint64 revision)
{
    if (revision != m_revision)
        return;
    qCInfo(logMedia) << "Media status changed" << "revision=" << revision << "source=" << m_source
                     << "status=" << int(value) << "error=" << m_player.errorString();
    if (value == QMediaPlayer::EndOfMedia && q->requested() && m_endedRevision != revision) {
        m_endedRevision = revision;
        // Never replace a source inside the backend's signal stack.
        QTimer::singleShot(0, q, [this, revision] {
            if (revision != m_revision || !q->requested())
                return;
            if (q->canNext())
                q->next();
            else {
                q->stop();
                Q_EMIT q->queueFinished();
            }
        });
    } else if (value == QMediaPlayer::InvalidMedia) {
        setSupport(MediaSupport::Unsupported);
        m_error = m_player.errorString().isEmpty()
                ? QCoreApplication::translate("Yalahoo", "Qt media status: InvalidMedia")
                : m_player.errorString();
        setIntent(Intent::Paused);
    } else if (value == QMediaPlayer::LoadedMedia || value == QMediaPlayer::BufferedMedia) {
        ready();
    }
    Q_EMIT q->stateChanged();
}
void PlaybackController::Private::setSupport(MediaSupport support)
{
    if (m_support == support)
        return;
    m_support = support;
    Q_EMIT q->supportChanged(support);
}
void PlaybackController::toggle()
{
    if (requested()) {
        pause();
        return;
    }
    if (!d->track()) {
        playAlbum(d->m_library->navigationAlbumId());
        return;
    }
    d->setIntent(Private::Intent::Playing);
    d->m_error.clear();
    if (d->m_source.isEmpty()) {
        d->m_source = d->track()->sourceUrl;
        d->load();
    } else if (d->m_player.mediaStatus() == QMediaPlayer::EndOfMedia)
        d->load();
    else if (d->m_player.error() != QMediaPlayer::NoError)
        d->load(position());
    else
        d->ready();
    Q_EMIT stateChanged();
}
void PlaybackController::pause()
{
    d->setIntent(Private::Intent::Paused);
    d->m_player.pause();
    Q_EMIT stateChanged();
}
void PlaybackController::stop()
{
    ++d->m_revision;
    d->clearConnections();
    d->m_player.stop();
    d->m_player.setSource({ });
    d->m_source.clear();
    d->setIntent(Private::Intent::Stopped);
    d->setSupport(MediaSupport::Unknown);
    d->m_error.clear();
    d->m_position = d->m_lastPosition = d->m_preview = 0;
    d->m_restore = -1;
    d->m_dragging = d->m_seekable = false;
    Q_EMIT positionChanged();
    Q_EMIT seekableChanged();
    Q_EMIT stateChanged();
}
void PlaybackController::next()
{
    if (canNext())
        d->start(d->m_queue.index() + 1);
    else
        stop();
}
void PlaybackController::previous()
{
    if (!d->track())
        return;
    if (position() > RestartPreviousThresholdMilliseconds) {
        seek(0);
        return;
    }
    if (d->m_queue.index() == 0) {
        if (seekable())
            seek(0);
        if (!requested())
            toggle();
    } else
        d->start(d->m_queue.index() - 1);
}
void PlaybackController::retry()
{
    if (!d->track())
        return;
    d->m_source = d->track()->sourceUrl;
    d->setIntent(Private::Intent::Playing);
    d->m_error.clear();
    d->load(position());
    Q_EMIT stateChanged();
}
void PlaybackController::seek(qint64 value)
{
    if (!seekable())
        return;
    d->m_position = d->m_lastPosition = d->m_duration > 0
            ? std::clamp(value, qint64(0), d->m_duration)
            : std::max<qint64>(0, value);
    d->m_restore = d->m_unavailable ? d->m_position : -1;
    d->m_player.setPosition(d->m_position);
    Q_EMIT positionChanged();
}
void PlaybackController::beginSeek()
{
    if (!seekable())
        return;
    d->m_preview = position();
    d->m_dragging = true;
    Q_EMIT positionChanged();
}
void PlaybackController::previewSeek(qint64 value)
{
    if (!d->m_dragging)
        return;
    d->m_preview = std::clamp(value, qint64(0), d->m_duration);
    Q_EMIT positionChanged();
}
void PlaybackController::commitSeek()
{
    if (!d->m_dragging)
        return;
    const auto target = d->m_preview;
    d->m_dragging = false;
    seek(target);
    Q_EMIT positionChanged();
}
void PlaybackController::cancelSeek()
{
    if (!d->m_dragging)
        return;
    d->m_dragging = false;
    Q_EMIT positionChanged();
}
void PlaybackController::setVolume(double value)
{
    if (std::isfinite(value))
        d->m_output.setVolume(float(std::clamp(value, 0., 1.)));
}
QVariantList PlaybackController::deviceChoices() const
{
    QVariantList choices;
    choices.reserve(d->m_outputs.size() + 1);
    choices.append(
            QVariantMap{ { u"label"_s, QCoreApplication::translate("Yalahoo", "System default") },
                         { u"value"_s, QString() } });
    for (const auto &device : d->m_outputs)
        choices.append(QVariantMap{ { u"label"_s, device.description() },
                                    { u"value"_s, QString::fromUtf8(device.id().toBase64()) } });
    return choices;
}
QVariantList PlaybackController::devices() const
{
    QVariantList result{ { QVariantMap{
            { u"id"_s, u""_s },
            { u"name"_s, QCoreApplication::translate("Yalahoo", "System default") } } } };
    for (const auto &device : d->m_outputs)
        result.append(QVariantMap{ { u"id"_s, QString::fromUtf8(device.id().toBase64()) },
                                   { u"name"_s, device.description() } });
    return result;
}
bool PlaybackController::selectDevice(const QString &id)
{
    const auto encoded = id.toUtf8();
    const auto decoded = QByteArray::fromBase64(encoded);
    if (!id.isEmpty() && decoded.toBase64() != encoded)
        return false;
    if (!id.isEmpty()
        && std::none_of(d->m_outputs.cbegin(), d->m_outputs.cend(), [&](const auto &device) {
               return device.id() == decoded;
           }))
        return false;
    d->m_preferred = decoded;
    d->m_notice.clear();
    d->refreshDevices();
    return true;
}
void PlaybackController::Private::restoreOutput(const QAudioDevice &selected)
{
    const bool recovering = m_unavailable;
    m_unavailable = false;
    const bool changed = m_output.device() != selected;
    if (recovering || changed)
        m_output.setDevice(selected);
    if (m_player.audioOutput() != &m_output)
        m_player.setAudioOutput(&m_output);
    if (!m_source.isEmpty()) {
        if (m_player.source() != m_source
            || (recovering && m_error.isEmpty() && m_player.error() != QMediaPlayer::NoError))
            load(std::max<qint64>(0, m_restore));
        else if ((recovering || changed) && m_error.isEmpty())
            ready();
    }
}
void PlaybackController::Private::refreshDevices()
{
    applyDevices(QMediaDevices::audioOutputs(), QMediaDevices::defaultAudioOutput());
}
// Choose a surviving explicit device first, then the system default. A route change preserves
// source identity, valid position and the newest pause/stop intent across backend reloads.
void PlaybackController::Private::applyDevices(const QList<QAudioDevice> &devices,
                                               const QAudioDevice &systemDefault)
{
    qCInfo(logMedia) << "Audio outputs refreshed" << "count=" << devices.size()
                     << "default=" << systemDefault.description()
                     << "preferredId=" << m_preferred.toBase64();
    m_outputs = devices;
    auto selected = systemDefault;
    if (!m_preferred.isEmpty()) {
        const auto it = std::find_if(devices.begin(), devices.end(), [&](const auto &device) {
            return device.id() == m_preferred;
        });
        if (it != devices.end())
            selected = *it;
        else {
            m_preferred.clear();
            m_notice = QCoreApplication::translate(
                    "Yalahoo",
                    "The selected output device was disconnected. Using the system default.");
        }
    }
    if (selected.isNull()) {
        if (!m_unavailable) {
            m_restore = std::max(m_lastPosition, m_player.position());
            m_unavailable = true;
            m_player.pause();
            m_player.setAudioOutput(nullptr);
        }
    } else {
        restoreOutput(selected);
    }
    Q_EMIT q->devicesChanged();
    Q_EMIT q->stateChanged();
}
QString PlaybackController::error() const
{
    return d->m_error;
}
QString PlaybackController::notice() const
{
    return d->m_notice;
}
qint64 PlaybackController::duration() const
{
    return d->m_duration;
}
bool PlaybackController::seekable() const
{
    return d->m_seekable && !d->m_source.isEmpty();
}
bool PlaybackController::dragging() const
{
    return d->m_dragging;
}
double PlaybackController::volume() const
{
    return d->m_output.volume();
}
QString PlaybackController::selectedDevice() const
{
    return QString::fromUtf8(d->m_preferred.toBase64());
}
bool PlaybackController::outputAvailable() const
{
    return !d->m_unavailable;
}
bool PlaybackController::canNext() const
{
    return d->m_queue.hasNext();
}
bool PlaybackController::canPrevious() const
{
    return d->m_queue.current() != nullptr;
}
QString PlaybackController::outputName() const
{
    return d->m_output.device().description();
}
QUrl PlaybackController::source() const
{
    return d->m_player.source();
}
bool PlaybackController::isAvailable() const
{
    return d->m_player.isAvailable();
}
quint64 PlaybackController::sourceRevision() const
{
    return d->m_revision;
}
MediaSupport PlaybackController::mediaSupport() const
{
    return d->m_support;
}

}
