#include "MediaMetadataProbe.h"
#include <QThread>
#include "ArtworkStore.h"
#include <QDate>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QMediaPlayer>
#include <QTimer>
#include <algorithm>
#include <array>

namespace {
// Bound work per event-loop turn and each backend probe without starting playback.
constexpr auto ArtworkIdHexCharacters = 24;
constexpr auto MillisecondsPerSecond = 1000;
constexpr auto MaximumTracksPerPump = 64;
constexpr auto ProbeTimeoutMilliseconds = 8000;
constexpr auto UnnumberedTrackSortKey = 9999;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
class QtMetadataSource final : public MetadataSource
{
    Q_DISABLE_COPY_MOVE(QtMetadataSource)
public:
    explicit QtMetadataSource(QObject *parent) : MetadataSource(parent), m_player(this)
    {
        // No audio/video output, and never play: setSource only prepares local metadata.
        connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, &MetadataSource::changed);
        connect(&m_player, &QMediaPlayer::metaDataChanged, this, &MetadataSource::changed);
        connect(&m_player, &QMediaPlayer::durationChanged, this, &MetadataSource::changed);
        connect(&m_player, &QMediaPlayer::errorOccurred, this, &MetadataSource::changed);
    }
    void open(const QUrl &url) override
    {
        m_player.setSource(url);
    }
    void clear() override
    {
        m_player.setSource({ });
    }
    MediaObservation observation() const override
    {
        MediaObservation result;
        if (m_player.mediaStatus() == QMediaPlayer::LoadedMedia)
            result.status = MediaObservation::Loaded;
        else if (m_player.mediaStatus() == QMediaPlayer::InvalidMedia
                 || m_player.error() != QMediaPlayer::NoError)
            result.status = MediaObservation::Invalid;
        result.metadata = m_player.metaData();
        result.durationMs = m_player.duration();
        result.hasAudio = m_player.hasAudio();
        result.error = m_player.errorString();
        return result;
    }

private:
    QMediaPlayer m_player;
};
QString text(const QMediaMetaData &metadata, QMediaMetaData::Key key)
{
    return metadata.stringValue(key).trimmed();
}
QString genreKey(QString value)
{
    static const QRegularExpression separators(u"[\\s\\p{P}\\p{S}]"_s);
    return value.trimmed().normalized(QString::NormalizationForm_KC).toLower().remove(separators);
}
void resolveGenres(LibrarySnapshot &snapshot)
{
    QHash<QString, QString> names;
    QSet<QString> ids;
    for (const auto &genre : snapshot.genres) {
        ids.insert(genre.id);
        auto aliases = genre.aliases;
        aliases.prepend(genre.id);
        aliases.prepend(genre.name);
        for (const auto &name : aliases) {
            const auto key = genreKey(name);
            if (!key.isEmpty() && !names.contains(key))
                names.insert(key, genre.id);
        }
    }
    const auto overrides = snapshot.fields.value(u"genreOverrides"_s).toObject();
    QHash<QString, int> counts;
    for (auto &album : snapshot.albums) {
        const auto overrideId = overrides.value(album.id).toString();
        if (ids.contains(overrideId)) {
            album.genreId = overrideId;
        } else if (!album.rawGenres.isEmpty()) {
            QString id;
            for (const auto &raw : album.rawGenres) {
                id = names.value(genreKey(raw));
                if (!id.isEmpty())
                    break;
            }
            if (id.isEmpty()) {
                const auto name = album.rawGenres.first();
                const auto key = genreKey(name);
                id = "source-"_L1
                        + QString::fromUtf8(
                                QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256)
                                        .toHex()
                                        .first(ArtworkIdHexCharacters));
                if (!ids.contains(id)) {
                    Genre genre;
                    genre.id = id;
                    genre.name = name;
                    snapshot.genres.append(genre);
                    names.insert(key, id);
                    ids.insert(id);
                }
            }
            album.genreId = id;
        }
        ++counts[album.genreId];
    }
    for (auto &genre : snapshot.genres)
        genre.albumCount = counts.value(genre.id);
}

void apply(Track &track, const MediaObservation &value, bool timeout)
{
    const auto &metadata = value.metadata;
    const auto title = text(metadata, QMediaMetaData::Title);
    if (!title.isEmpty())
        track.title = title;
    auto artist = text(metadata, QMediaMetaData::ContributingArtist);
    if (artist.isEmpty())
        artist = text(metadata, QMediaMetaData::AlbumArtist);
    if (!artist.isEmpty())
        track.artist = artist;
    const auto number = metadata.value(QMediaMetaData::TrackNumber).toInt();
    if (number > 0)
        track.trackNumber = number;
    const auto duration = metadata.value(QMediaMetaData::Duration).toLongLong();
    if (value.durationMs > 0 || duration > 0)
        track.duration =
                double(value.durationMs > 0 ? value.durationMs : duration) / MillisecondsPerSecond;
    const auto bitrate = metadata.value(QMediaMetaData::AudioBitRate).toInt();
    if (bitrate > 0)
        track.bitrate = bitrate;
    auto common = track.fields.value(u"_common"_s).toObject();
    const auto albumTitle = text(metadata, QMediaMetaData::AlbumTitle);
    const auto albumArtist = text(metadata, QMediaMetaData::AlbumArtist);
    if (!albumTitle.isEmpty())
        common.insert(u"album"_s, albumTitle);
    if (!albumArtist.isEmpty())
        common.insert(u"albumartist"_s, albumArtist);
    const auto date = metadata.value(QMediaMetaData::Date).toDate();
    if (date.isValid())
        common.insert(u"year"_s, date.year());
    const auto genres = metadata.value(QMediaMetaData::Genre).toStringList();
    if (!genres.isEmpty())
        common.insert(u"genres"_s, QJsonArray::fromStringList(genres));
    track.fields.insert(u"_common"_s, common);
    // Loading a container does not prove that its audio can actually be decoded.
    // Playback in R24 owns promotion to Supported; metadata remains independently useful.
    track.mediaSupport = !timeout
                    && (value.status == MediaObservation::Invalid
                        || (value.status == MediaObservation::Loaded && !value.hasAudio))
            ? MediaSupport::Unsupported
            : MediaSupport::Unknown;
    track.fields.insert(u"_probeStatus"_s,
                        timeout ? u"timeout"_s
                                : value.status == MediaObservation::Loaded ? u"loaded"_s
                                                                           : u"invalid"_s);
    if (!value.error.isEmpty())
        track.fields.insert(u"_probeError"_s, value.error);
    else
        track.fields.remove(u"_probeError"_s);
    if (!timeout)
        track.fields.insert(u"_qtProbeVersion"_s, QString::fromUtf8(qVersion()));
}
}
class ProbeWorker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProbeWorker)
public:
    explicit ProbeWorker(MediaMetadataProbe::Factory factory) : m_factory(std::move(factory))
    {
    }
    void begin(Snapshot snapshot, QString root, ScanCancellation cancel, quint64 generation)
    {
        abort();
        m_next = snapshot ? *snapshot : LibrarySnapshot{ };
        m_root = std::move(root);
        m_cancel = std::move(cancel);
        m_generation = generation;
        m_album = m_track = 0;
        m_running = true;
        for (auto &slot : m_slots) {
            if (!slot.source) {
                slot.source = m_factory ? m_factory(this) : new QtMetadataSource(this);
                slot.timer = new QTimer(this);
                slot.timer->setSingleShot(true);
                connect(slot.timer, &QTimer::timeout, this, [this, &slot] {
                    complete(slot, true);
                });
            }
        }
        pump();
    }
    void abort()
    {
        if (!m_running)
            return;
        for (auto &slot : m_slots) {
            disconnect(slot.connection);
            slot.timer->stop();
            slot.busy = false;
            ++slot.serial;
            slot.source->clear();
        }
        m_running = false;
        m_next = { };
        Q_EMIT done(m_generation, { }, true);
    }
Q_SIGNALS:
    void done(quint64 generation, yalahoo::Snapshot snapshot, bool cancelled);

private:
    struct Slot final
    {
        MetadataSource *source = nullptr;
        QTimer *timer = nullptr;
        QMetaObject::Connection connection;
        qsizetype album = 0, track = 0;
        quint64 serial = 0;
        bool busy = false, queued = false;
    };
    bool stopped() const
    {
        return m_cancel && m_cancel->load(std::memory_order_relaxed);
    }
    void prepareAlbum(Album &album)
    {
        // Candidates are sorted by the scanner; reject invalid images before fallback.
        album.coverUrl = QUrl();
        for (const auto &candidate : album.fields.value(u"_coverCandidates"_s).toArray()) {
            if (stopped())
                return;
            album.coverUrl = ArtworkStore::copy(candidate.toString(), m_root);
            if (!album.coverUrl.isEmpty())
                break;
        }
    }
    void pump()
    {
        if (!m_running)
            return;
        if (stopped()) {
            abort();
            return;
        }
        // Admit a bounded slice per event turn, even when a large cached library needs no IO.
        int visited = 0;
        for (auto &slot : m_slots) {
            while (!slot.busy && m_album < m_next.albums.size()
                   && visited++ < MaximumTracksPerPump) {
                auto &album = m_next.albums[m_album];
                if (m_track == 0 && !album.offline)
                    prepareAlbum(album);
                if (album.offline || m_track >= album.tracks.size()) {
                    ++m_album;
                    m_track = 0;
                    continue;
                }
                const auto index = m_track++;
                const auto &track = album.tracks[index];
                bool coverKnown = track.fields.contains(u"_embeddedCover"_s);
                const auto embedded = track.fields.value(u"_embeddedCover"_s).toObject();
                if (!embedded.isEmpty()
                    && !QFileInfo(embedded.value(u"path"_s).toString()).isFile())
                    coverKnown = false;
                const bool needsCover =
                        (album.fields.value(u"_singleFile"_s).toBool() || album.coverUrl.isEmpty())
                        && !coverKnown;
                if (!needsCover
                    && track.fields.value(u"_qtProbeVersion"_s).toString()
                            == QString::fromUtf8(qVersion()))
                    continue;
                if (!track.sourceUrl.isLocalFile())
                    continue;
                slot.album = m_album;
                slot.track = index;
                slot.busy = true;
                slot.queued = false;
                const auto serial = ++slot.serial;
                slot.connection =
                        connect(slot.source, &MetadataSource::changed, this, [this, &slot, serial] {
                            if (!slot.busy || slot.serial != serial || slot.queued
                                || slot.source->observation().status == MediaObservation::Loading)
                                return;
                            slot.queued = true;
                            QMetaObject::invokeMethod(
                                    this,
                                    [this, &slot, serial] {
                                        if (slot.busy && slot.serial == serial)
                                            complete(slot, false);
                                    },
                                    Qt::QueuedConnection);
                        });
                slot.timer->start(ProbeTimeoutMilliseconds);
                slot.source->open(track.sourceUrl);
            }
        }
        if (m_album < m_next.albums.size()) {
            if (visited >= MaximumTracksPerPump)
                QMetaObject::invokeMethod(this, &ProbeWorker::pump, Qt::QueuedConnection);
            return;
        }
        if (std::any_of(m_slots.begin(), m_slots.end(), [](const Slot &slot) {
                return slot.busy;
            }))
            return;
        for (auto &album : m_next.albums) {
            if (stopped()) {
                abort();
                return;
            }
            if (album.offline)
                continue;
            if (album.fields.value(u"_singleFile"_s).toBool() || album.coverUrl.isEmpty()) {
                for (const auto &track : album.tracks) {
                    const auto cached = track.fields.value(u"_embeddedCover"_s)
                                                .toObject()
                                                .value(u"path"_s)
                                                .toString();
                    if (!cached.isEmpty()) {
                        const auto embedded = ArtworkStore::copy(cached, m_root);
                        if (!embedded.isEmpty()) {
                            album.coverUrl = embedded;
                            break;
                        }
                    }
                }
            }
            std::stable_sort(
                    album.tracks.begin(), album.tracks.end(), [](const Track &a, const Track &b) {
                        return std::pair(a.discNumber.value_or(1),
                                         a.trackNumber.value_or(UnnumberedTrackSortKey))
                                < std::pair(b.discNumber.value_or(1),
                                            b.trackNumber.value_or(UnnumberedTrackSortKey));
                    });
            if (!album.tracks.isEmpty()) {
                const auto &first = album.tracks.first();
                const auto common = first.fields.value(u"_common"_s).toObject();
                const bool single = album.fields.value(u"_singleFile"_s).toBool();
                const auto title = common.value(u"album"_s).toString();
                if (single || !title.isEmpty())
                    album.title = single ? first.title : title;
                const auto artist = common.value(u"albumartist"_s).toString();
                if (!artist.isEmpty() || !first.artist.isEmpty())
                    album.artist = !single && !artist.isEmpty() ? artist : first.artist;
                if (common.value(u"year"_s).toInt() > 0)
                    album.year = common.value(u"year"_s).toInt();
                const auto genres = common.value(u"genres"_s).toArray();
                if (!genres.isEmpty()) {
                    album.rawGenres.clear();
                    for (const auto &genre : genres)
                        album.rawGenres.append(genre.toString());
                }
            }
        }
        resolveGenres(m_next);
        m_running = false;
        Q_EMIT done(m_generation, std::make_shared<const LibrarySnapshot>(std::move(m_next)),
                    false);
    }
    void complete(Slot &slot, bool timeout)
    {
        if (!slot.busy || !m_running)
            return;
        if (stopped()) {
            abort();
            return;
        }
        const auto observation = slot.source->observation();
        auto &album = m_next.albums[slot.album];
        auto &track = album.tracks[slot.track];
        apply(track, observation, timeout);
        if (album.fields.value(u"_singleFile"_s).toBool() || album.coverUrl.isEmpty()) {
            const auto image =
                    observation.metadata.value(QMediaMetaData::CoverArtImage).value<QImage>();
            if (image.isNull()) {
                track.fields.insert(u"_embeddedCover"_s, QJsonValue::Null);
            } else {
                QString error;
                const auto cover = ArtworkStore::save(image, m_root, &error);
                if (!cover.isEmpty())
                    track.fields.insert(u"_embeddedCover"_s,
                                        QJsonObject{ { u"path"_s, cover.toLocalFile() } });
                if (!error.isEmpty())
                    track.fields.insert(u"_artworkError"_s, error);
                else
                    track.fields.remove(u"_artworkError"_s);
            }
        }
        disconnect(slot.connection);
        slot.timer->stop();
        slot.busy = false;
        ++slot.serial;
        slot.source->clear();
        QMetaObject::invokeMethod(this, &ProbeWorker::pump, Qt::QueuedConnection);
    }
    MediaMetadataProbe::Factory m_factory;
    std::array<Slot, 2> m_slots;
    LibrarySnapshot m_next;
    QString m_root;
    ScanCancellation m_cancel;
    quint64 m_generation = 0;
    qsizetype m_album = 0, m_track = 0;
    bool m_running = false;
};
struct MediaMetadataProbe::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
    QThread m_thread;
    QObject *m_worker = nullptr;
};
MediaMetadataProbe::MediaMetadataProbe(QObject *parent, Factory factory)
    : QObject(parent), d(std::make_unique<Private>())
{
    qRegisterMetaType<Snapshot>();
    auto *worker = new ProbeWorker(std::move(factory));
    d->m_worker = worker;
    worker->moveToThread(&d->m_thread);
    connect(&d->m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &ProbeWorker::done, this, &MediaMetadataProbe::finished, Qt::QueuedConnection);
    d->m_thread.setObjectName("media-metadata");
    d->m_thread.start();
}
MediaMetadataProbe::~MediaMetadataProbe()
{
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = static_cast<ProbeWorker *>(d->m_worker)] {
                worker->abort();
            },
            Qt::BlockingQueuedConnection);
    d->m_thread.quit();
    d->m_thread.wait();
}
void MediaMetadataProbe::start(Snapshot snapshot, QString root, ScanCancellation cancel,
                               quint64 generation)
{
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = static_cast<ProbeWorker *>(d->m_worker), snapshot = std::move(snapshot),
             root = std::move(root), cancel = std::move(cancel), generation] {
                worker->begin(snapshot, root, cancel, generation);
            },
            Qt::QueuedConnection);
}
// Invalidate acceptance before clearing backend sources: clear() may emit synchronous status changes.
// No completion from the canceled generation may replace the caller snapshot.
void MediaMetadataProbe::cancel()
{
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = static_cast<ProbeWorker *>(d->m_worker)] {
                worker->abort();
            },
            Qt::QueuedConnection);
}
}
#include "MediaMetadataProbe.moc"
