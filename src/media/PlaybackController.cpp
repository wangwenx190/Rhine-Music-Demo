#include "PlaybackController.h"
#include <QDir>
#include <QSet>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <utility>

namespace yalahoo {
PlaybackController::PlaybackController(LibrarySession *library, QObject *parent)
    : QObject(parent), m_library(library), m_devices(this), m_output(this), m_player(this)
{
    m_output.setVolume(.65f);
    m_player.setPlaybackRate(1);
    m_player.setAudioOutput(&m_output);
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        if (m_refreshPending)
            return;
        m_refreshPending = true;
        QTimer::singleShot(0, this, [this] {
            m_refreshPending = false;
            refreshDevices();
        });
    });
    connect(&m_output, &QAudioOutput::volumeChanged, this, &PlaybackController::volumeChanged);
    refreshDevices();
}
PlaybackController::~PlaybackController()
{
    ++m_revision;
    clearConnections();
    m_player.stop();
    m_player.setSource({ });
    m_player.setAudioOutput(nullptr);
}
const Track *PlaybackController::track() const
{
    return m_index >= 0 && m_index < m_queue.size() ? &m_queue[m_index] : nullptr;
}
QString PlaybackController::title() const
{
    return track() ? track()->title : QString();
}
QString PlaybackController::albumId() const
{
    return track() ? track()->albumId : QString();
}
QString PlaybackController::trackId() const
{
    return track() ? track()->id : QString();
}
bool PlaybackController::requested() const
{
    return m_intent == Intent::Playing;
}
bool PlaybackController::playing() const
{
    return requested() && !m_unavailable && m_player.isPlaying();
}
bool PlaybackController::loading() const
{
    return requested() && !m_unavailable
            && (m_player.mediaStatus() == QMediaPlayer::LoadingMedia
                || m_player.mediaStatus() == QMediaPlayer::BufferingMedia
                || m_player.source().isEmpty());
}
QString PlaybackController::state() const
{
    if (!m_error.isEmpty())
        return "error";
    if (requested() && m_unavailable)
        return "waiting-output";
    if (loading())
        return "loading";
    if (playing())
        return "playing";
    return m_intent == Intent::Paused ? "paused" : "stopped";
}
qint64 PlaybackController::position() const
{
    return m_dragging ? m_preview : m_restore >= 0 ? m_restore : m_position;
}
bool PlaybackController::playTrack(const QString &albumId, const QString &trackId)
{
    const auto *album = m_library->album(albumId);
    if (!album || album->offline) {
        emit rejected(QStringLiteral("专辑目录暂不可用，请重新扫描或换曲。"));
        return false;
    }
    int target = -1;
    QList<Track> queue;
    QSet<QString> ids;
    for (const auto &entry : album->tracks) {
        if (ids.contains(entry.id))
            continue;
        ids.insert(entry.id);
        if (entry.id == trackId)
            target = int(queue.size());
        queue.append(entry);
    }
    if (target < 0) {
        emit rejected(QStringLiteral("曲目已不在当前音乐库中，请换曲或重新扫描。"));
        return false;
    }
    const auto &url = queue[target].sourceUrl;
    if (!url.isLocalFile() || !QDir::isAbsolutePath(url.toLocalFile()) || url.hasQuery()
        || url.hasFragment() || url.toLocalFile().contains(QChar::Null)) {
        emit rejected(QStringLiteral("歌曲需要本地绝对文件路径。"));
        return false;
    }
    m_queue = std::move(queue);
    start(target);
    return true;
}
bool PlaybackController::playAlbum(const QString &albumId)
{
    const auto *album = m_library->album(albumId);
    return album && !album->tracks.isEmpty() && playTrack(albumId, album->tracks.first().id);
}
void PlaybackController::start(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;
    const auto url = m_queue[index].sourceUrl;
    if (!url.isLocalFile() || !QDir::isAbsolutePath(url.toLocalFile()) || url.hasQuery()
        || url.hasFragment() || url.toLocalFile().contains(QChar::Null)) {
        emit rejected(QStringLiteral("歌曲需要本地绝对文件路径。"));
        return;
    }
    m_index = index;
    m_source = track()->sourceUrl;
    m_intent = Intent::Playing;
    m_error.clear();
    m_duration = m_position = m_lastPosition = 0;
    m_dragging = false;
    m_library->adoptPlaybackIds(albumId(), trackId());
    load();
    emit currentChanged();
    emit durationChanged();
    emit positionChanged();
    emit seekableChanged();
    emit stateChanged();
}
void PlaybackController::clearConnections()
{
    for (const auto &connection : std::exchange(m_connections, { }))
        disconnect(connection);
}
bool PlaybackController::current(quint64 revision, const QUrl &source) const
{
    return revision == m_revision && !m_source.isEmpty() && source == m_source
            && m_player.source() == source;
}
void PlaybackController::load(qint64 restore)
{
    const auto revision = ++m_revision;
    setSupport(MediaSupport::Unknown);
    clearConnections();
    m_player.stop();
    m_player.setSource({ });
    m_seekable = m_dragging = false;
    emit seekableChanged();
    m_restore = std::max<qint64>(0, restore);
    m_position = m_restore;
    emit positionChanged();
    if (m_unavailable || m_source.isEmpty())
        return;
    if (!m_player.isAvailable()) {
        m_error = QStringLiteral("Qt 媒体播放服务不可用。");
        m_intent = Intent::Paused;
        emit stateChanged();
        return;
    }
    const auto source = m_source;
    m_connections.append(connect(&m_player, &QMediaPlayer::mediaStatusChanged, this,
                                 [this, revision, source](auto value) {
                                     if (current(revision, source)
                                         && value == m_player.mediaStatus())
                                         status(value, revision);
                                 }));
    m_connections.append(connect(
            &m_player, &QMediaPlayer::seekableChanged, this, [this, revision, source](bool value) {
                if (!current(revision, source) || value != m_player.isSeekable())
                    return;
                m_seekable = value;
                emit seekableChanged();
                ready();
            }));
    m_connections.append(connect(&m_player, &QMediaPlayer::durationChanged, this,
                                 [this, revision, source](qint64 value) {
                                     if (!current(revision, source) || value != m_player.duration())
                                         return;
                                     m_duration = std::max<qint64>(0, value);
                                     emit durationChanged();
                                 }));
    m_connections.append(connect(
            &m_player, &QMediaPlayer::positionChanged, this, [this, revision, source](qint64) {
                if (!current(revision, source) || m_unavailable || m_restore >= 0)
                    return;
                const auto value = m_player.position();
                m_position = std::max<qint64>(0, value);
                if (value > 0) {
                    m_lastPosition = value;
                    if (playing() && m_player.hasAudio())
                        setSupport(MediaSupport::Supported);
                }
                if (!m_dragging)
                    emit positionChanged();
            }));
    m_connections.append(connect(&m_player, &QMediaPlayer::playbackStateChanged, this,
                                 [this, revision, source](auto value) {
                                     if (!current(revision, source)
                                         || value != m_player.playbackState())
                                         return;
                                     if (value == QMediaPlayer::PlayingState) {
                                         if (!requested() || m_unavailable)
                                             m_player.pause();
                                     }
                                     emit stateChanged();
                                 }));
    m_connections.append(connect(&m_player, &QMediaPlayer::errorOccurred, this,
                                 [this, revision, source](auto error, const QString &reason) {
                                     if (!current(revision, source) || error != m_player.error()
                                         || m_unavailable || error == QMediaPlayer::NoError)
                                         return;
                                     m_error = reason.isEmpty()
                                             ? QStringLiteral("Qt 媒体错误：%1").arg(int(error))
                                             : reason;
                                     m_intent = Intent::Paused;
                                     if (error == QMediaPlayer::FormatError)
                                         setSupport(MediaSupport::Unsupported);
                                     emit stateChanged();
                                 }));
    m_player.setPlaybackRate(1);
    m_player.setSource(source);
}
void PlaybackController::ready()
{
    const auto value = m_player.mediaStatus();
    if (m_unavailable || m_source.isEmpty() || m_player.source() != m_source
        || (value != QMediaPlayer::LoadedMedia && value != QMediaPlayer::BufferedMedia))
        return;
    if (!m_player.hasAudio()) {
        m_error = QStringLiteral("Qt 未检测到音轨。");
        m_intent = Intent::Paused;
        m_player.pause();
        setSupport(MediaSupport::Unsupported);
        emit stateChanged();
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
        emit positionChanged();
    }
    if (requested() && !m_player.isPlaying())
        m_player.play();
    else if (m_intent == Intent::Paused)
        m_player.pause();
    emit durationChanged();
    emit seekableChanged();
    emit stateChanged();
}
void PlaybackController::status(QMediaPlayer::MediaStatus value, quint64 revision)
{
    if (revision != m_revision)
        return;
    if (value == QMediaPlayer::EndOfMedia && requested() && m_endedRevision != revision) {
        m_endedRevision = revision;
        // Never replace a source inside the backend's signal stack.
        QTimer::singleShot(0, this, [this, revision] {
            if (revision != m_revision || !requested())
                return;
            if (canNext())
                next();
            else {
                stop();
                emit queueFinished();
            }
        });
    } else if (value == QMediaPlayer::InvalidMedia) {
        setSupport(MediaSupport::Unsupported);
        m_error = m_player.errorString().isEmpty() ? QStringLiteral("Qt 媒体状态：InvalidMedia")
                                                   : m_player.errorString();
        m_intent = Intent::Paused;
    } else if (value == QMediaPlayer::LoadedMedia || value == QMediaPlayer::BufferedMedia) {
        ready();
    }
    emit stateChanged();
}
void PlaybackController::setSupport(MediaSupport support)
{
    if (m_support == support)
        return;
    m_support = support;
    emit supportChanged(support);
}
void PlaybackController::toggle()
{
    if (requested()) {
        pause();
        return;
    }
    if (!track()) {
        playAlbum(m_library->navigationAlbumId());
        return;
    }
    m_intent = Intent::Playing;
    m_error.clear();
    if (m_source.isEmpty()) {
        m_source = track()->sourceUrl;
        load();
    } else if (m_player.mediaStatus() == QMediaPlayer::EndOfMedia)
        load();
    else if (m_player.error() != QMediaPlayer::NoError)
        load(position());
    else
        ready();
    emit stateChanged();
}
void PlaybackController::pause()
{
    m_intent = Intent::Paused;
    m_player.pause();
    emit stateChanged();
}
void PlaybackController::stop()
{
    m_intent = Intent::Stopped;
    ++m_revision;
    clearConnections();
    m_player.stop();
    m_player.setSource({ });
    m_source.clear();
    setSupport(MediaSupport::Unknown);
    m_error.clear();
    m_position = m_lastPosition = m_preview = 0;
    m_restore = -1;
    m_dragging = m_seekable = false;
    emit positionChanged();
    emit seekableChanged();
    emit stateChanged();
}
void PlaybackController::next()
{
    if (canNext())
        start(m_index + 1);
    else
        stop();
}
void PlaybackController::previous()
{
    if (!track())
        return;
    if (position() > 3000) {
        seek(0);
        return;
    }
    if (m_index == 0) {
        if (seekable())
            seek(0);
        if (!requested())
            toggle();
    } else
        start(m_index - 1);
}
void PlaybackController::retry()
{
    if (!track())
        return;
    m_source = track()->sourceUrl;
    m_intent = Intent::Playing;
    m_error.clear();
    load(position());
    emit stateChanged();
}
void PlaybackController::seek(qint64 value)
{
    if (!seekable())
        return;
    m_position = m_lastPosition =
            m_duration > 0 ? std::clamp(value, qint64(0), m_duration) : std::max<qint64>(0, value);
    m_restore = m_unavailable ? m_position : -1;
    m_player.setPosition(m_position);
    emit positionChanged();
}
void PlaybackController::beginSeek()
{
    if (!seekable())
        return;
    m_preview = position();
    m_dragging = true;
    emit positionChanged();
}
void PlaybackController::previewSeek(qint64 value)
{
    if (!m_dragging)
        return;
    m_preview = std::clamp(value, qint64(0), m_duration);
    emit positionChanged();
}
void PlaybackController::commitSeek()
{
    if (!m_dragging)
        return;
    const auto target = m_preview;
    m_dragging = false;
    seek(target);
    emit positionChanged();
}
void PlaybackController::cancelSeek()
{
    if (!m_dragging)
        return;
    m_dragging = false;
    emit positionChanged();
}
void PlaybackController::setVolume(double value)
{
    if (std::isfinite(value))
        m_output.setVolume(float(std::clamp(value, 0., 1.)));
}
QVariantList PlaybackController::devices() const
{
    QVariantList result{ { QVariantMap{ { "id", "" }, { "name", "系统默认" } } } };
    for (const auto &device : m_outputs)
        result.append(QVariantMap{ { "id", QString::fromLatin1(device.id().toBase64()) },
                                   { "name", device.description() } });
    return result;
}
bool PlaybackController::selectDevice(const QString &id)
{
    const auto decoded = QByteArray::fromBase64(id.toLatin1());
    if (!id.isEmpty() && decoded.toBase64() != id.toLatin1())
        return false;
    if (!id.isEmpty() && std::none_of(m_outputs.begin(), m_outputs.end(), [&](const auto &device) {
            return device.id() == decoded;
        }))
        return false;
    m_preferred = decoded;
    m_notice.clear();
    refreshDevices();
    return true;
}
void PlaybackController::refreshDevices()
{
    applyDevices(QMediaDevices::audioOutputs(), QMediaDevices::defaultAudioOutput());
}
void PlaybackController::applyDevices(const QList<QAudioDevice> &devices,
                                      const QAudioDevice &systemDefault)
{
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
            m_notice = QStringLiteral("所选输出设备已断开，已切换系统默认。");
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
    emit devicesChanged();
    emit stateChanged();
}
}
