#include "MediaMetadataProbe.h"
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

namespace yalahoo {
namespace {
class QtMetadataSource final : public MetadataSource
{
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
    static const QRegularExpression separators(QStringLiteral("[\\s\\p{P}\\p{S}]"));
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
    const auto overrides = snapshot.fields.value("genreOverrides").toObject();
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
                id = "source-"
                        + QString::fromLatin1(
                                QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256)
                                        .toHex()
                                        .first(24));
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
        track.duration = double(value.durationMs > 0 ? value.durationMs : duration) / 1000;
    const auto bitrate = metadata.value(QMediaMetaData::AudioBitRate).toInt();
    if (bitrate > 0)
        track.bitrate = bitrate;
    auto common = track.fields.value("_common").toObject();
    const auto albumTitle = text(metadata, QMediaMetaData::AlbumTitle);
    const auto albumArtist = text(metadata, QMediaMetaData::AlbumArtist);
    if (!albumTitle.isEmpty())
        common.insert("album", albumTitle);
    if (!albumArtist.isEmpty())
        common.insert("albumartist", albumArtist);
    const auto date = metadata.value(QMediaMetaData::Date).toDate();
    if (date.isValid())
        common.insert("year", date.year());
    const auto genres = metadata.value(QMediaMetaData::Genre).toStringList();
    if (!genres.isEmpty())
        common.insert("genres", QJsonArray::fromStringList(genres));
    track.fields.insert("_common", common);
    // Loading a container does not prove that its audio can actually be decoded.
    // Playback in R24 owns promotion to Supported; metadata remains independently useful.
    track.mediaSupport = !timeout
                    && (value.status == MediaObservation::Invalid
                        || (value.status == MediaObservation::Loaded && !value.hasAudio))
            ? MediaSupport::Unsupported
            : MediaSupport::Unknown;
    track.fields.insert("_probeStatus",
                        timeout ? "timeout"
                                : value.status == MediaObservation::Loaded ? "loaded"
                                                                           : "invalid");
    if (!value.error.isEmpty())
        track.fields.insert("_probeError", value.error);
    else
        track.fields.remove("_probeError");
    if (!timeout)
        track.fields.insert("_qtProbeVersion", QString::fromLatin1(qVersion()));
}
}
class ProbeWorker final : public QObject
{
    Q_OBJECT
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
        emit done(m_generation, { }, true);
    }
signals:
    void done(quint64 generation, yalahoo::Snapshot snapshot, bool cancelled);

private:
    struct Slot
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
        for (const auto &candidate : album.fields.value("_coverCandidates").toArray()) {
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
            while (!slot.busy && m_album < m_next.albums.size() && visited++ < 64) {
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
                bool coverKnown = track.fields.contains("_embeddedCover");
                const auto embedded = track.fields.value("_embeddedCover").toObject();
                if (!embedded.isEmpty() && !QFileInfo(embedded.value("path").toString()).isFile())
                    coverKnown = false;
                const bool needsCover =
                        (album.fields.value("_singleFile").toBool() || album.coverUrl.isEmpty())
                        && !coverKnown;
                if (!needsCover
                    && track.fields.value("_qtProbeVersion").toString()
                            == QString::fromLatin1(qVersion()))
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
                slot.timer->start(8000);
                slot.source->open(track.sourceUrl);
            }
        }
        if (m_album < m_next.albums.size()) {
            if (visited >= 64)
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
            if (album.fields.value("_singleFile").toBool() || album.coverUrl.isEmpty()) {
                for (const auto &track : album.tracks) {
                    const auto cached = track.fields.value("_embeddedCover")
                                                .toObject()
                                                .value("path")
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
                        return std::pair(a.discNumber.value_or(1), a.trackNumber.value_or(9999))
                                < std::pair(b.discNumber.value_or(1), b.trackNumber.value_or(9999));
                    });
            if (!album.tracks.isEmpty()) {
                const auto &first = album.tracks.first();
                const auto common = first.fields.value("_common").toObject();
                const bool single = album.fields.value("_singleFile").toBool();
                const auto title = common.value("album").toString();
                if (single || !title.isEmpty())
                    album.title = single ? first.title : title;
                const auto artist = common.value("albumartist").toString();
                if (!artist.isEmpty() || first.artist != QStringLiteral("未知艺术家"))
                    album.artist = !single && !artist.isEmpty() ? artist : first.artist;
                if (common.value("year").toInt() > 0)
                    album.year = common.value("year").toInt();
                const auto genres = common.value("genres").toArray();
                if (!genres.isEmpty()) {
                    album.rawGenres.clear();
                    for (const auto &genre : genres)
                        album.rawGenres.append(genre.toString());
                }
            }
        }
        resolveGenres(m_next);
        m_running = false;
        emit done(m_generation, std::make_shared<const LibrarySnapshot>(std::move(m_next)), false);
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
        if (album.fields.value("_singleFile").toBool() || album.coverUrl.isEmpty()) {
            const auto image =
                    observation.metadata.value(QMediaMetaData::CoverArtImage).value<QImage>();
            if (image.isNull()) {
                track.fields.insert("_embeddedCover", QJsonValue::Null);
            } else {
                QString error;
                const auto cover = ArtworkStore::save(image, m_root, &error);
                if (!cover.isEmpty())
                    track.fields.insert("_embeddedCover",
                                        QJsonObject{ { "path", cover.toLocalFile() } });
                if (!error.isEmpty())
                    track.fields.insert("_artworkError", error);
                else
                    track.fields.remove("_artworkError");
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
MediaMetadataProbe::MediaMetadataProbe(QObject *parent, Factory factory) : QObject(parent)
{
    qRegisterMetaType<Snapshot>();
    auto *worker = new ProbeWorker(std::move(factory));
    m_worker = worker;
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &ProbeWorker::done, this, &MediaMetadataProbe::finished, Qt::QueuedConnection);
    m_thread.setObjectName("media-metadata");
    m_thread.start();
}
MediaMetadataProbe::~MediaMetadataProbe()
{
    QMetaObject::invokeMethod(
            m_worker,
            [worker = static_cast<ProbeWorker *>(m_worker)] {
                worker->abort();
            },
            Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}
void MediaMetadataProbe::start(Snapshot snapshot, QString root, ScanCancellation cancel,
                               quint64 generation)
{
    QMetaObject::invokeMethod(
            m_worker,
            [worker = static_cast<ProbeWorker *>(m_worker), snapshot = std::move(snapshot),
             root = std::move(root), cancel = std::move(cancel), generation] {
                worker->begin(snapshot, root, cancel, generation);
            },
            Qt::QueuedConnection);
}
void MediaMetadataProbe::cancel()
{
    QMetaObject::invokeMethod(
            m_worker,
            [worker = static_cast<ProbeWorker *>(m_worker)] {
                worker->abort();
            },
            Qt::QueuedConnection);
}
}
#include "MediaMetadataProbe.moc"
