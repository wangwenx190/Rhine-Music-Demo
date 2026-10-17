#include "ArtworkStore.h"
#include "LegacyImporter.h"
#include "LibraryStore.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include "PreferencesStore.h"
#include "MediaMetadataProbe.h"
#include <QBuffer>
#include <QDataStream>
#include <QDate>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <atomic>
#include <filesystem>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
bool put(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray get(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray chunk(const char *name, QByteArray bytes)
{
    QByteArray result(name, 4);
    QDataStream stream(&result, QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(bytes.size());
    result += bytes;
    if (bytes.size() % 2)
        result += '\0';
    return result;
}
QByteArray wave(const QImage &embedded = { })
{
    // Anonymous silent test input, generated locally. No production PCM path.
    QByteArray format;
    QDataStream stream(&format, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2)
           << quint16(16);
    const auto info = "INFO"_ba + chunk("INAM", "Anonymous Title\0"_ba)
            + chunk("IART", "Anonymous Artist\0"_ba) + chunk("IPRD", "Anonymous Album\0"_ba);
    QByteArray artwork;
    if (!embedded.isNull()) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        embedded.save(&buffer, "PNG");
        QByteArray frame = "APIC"_ba;
        const auto payload = "\0image/png\0\x03\0"_ba + png;
        QDataStream sizes(&frame, QIODevice::Append);
        sizes.setByteOrder(QDataStream::BigEndian);
        sizes << quint32(payload.size()) << quint16(0);
        frame += payload;
        QByteArray id3 = "ID3\x03\0\0"_ba;
        for (int shift : { 21, 14, 7, 0 })
            id3.append(char((frame.size() >> shift) & 0x7f));
        id3 += frame;
        artwork = chunk("id3 ", id3);
    }
    const auto body = "WAVE"_ba + chunk("fmt ", format) + chunk("LIST", info) + artwork
            + chunk("data", QByteArray(1600, '\0'));
    return chunk("RIFF", body);
}
Snapshot scanned(const QString &root)
{
    return DirectoryScanner::scan({ QUrl::fromLocalFile(root) }, { }, { }).snapshot;
}
struct FakeStats
{
    std::atomic_int created = 0, active = 0, peak = 0, opens = 0, clears = 0;
    std::atomic_bool offGui = true;
};
class FakeSource final : public MetadataSource
{
public:
    FakeSource(QObject *parent, std::shared_ptr<FakeStats> stats, MediaObservation value, int delay)
        : MetadataSource(parent), m_stats(std::move(stats)), m_value(value), m_delay(delay)
    {
    }
    void open(const QUrl &) override
    {
        m_current = { };
        m_busy = true;
        ++m_stats->opens;
        const auto active = ++m_stats->active;
        m_stats->peak.store(qMax(m_stats->peak.load(), active));
        const auto serial = ++m_serial;
        if (m_delay >= 0)
            QTimer::singleShot(m_delay, this, [this, serial] {
                if (serial == m_serial)
                    m_current = m_value;
                // Deliberately deliver an obsolete notification after cancellation.
                Q_EMIT changed();
            });
    }
    void clear() override
    {
        ++m_serial;
        if (m_busy) {
            --m_stats->active;
            ++m_stats->clears;
            m_busy = false;
        }
        m_current = { };
    }
    MediaObservation observation() const override
    {
        return m_current;
    }

private:
    std::shared_ptr<FakeStats> m_stats;
    MediaObservation m_value, m_current;
    int m_delay;
    quint64 m_serial = 0;
    bool m_busy = false;
};
MediaMetadataProbe::Factory factory(std::shared_ptr<FakeStats> stats, MediaObservation value,
                                    int delay = 0)
{
    const auto gui = QThread::currentThread();
    return [stats, value, delay, gui](QObject *parent) {
        ++stats->created;
        if (QThread::currentThread() == gui)
            stats->offGui = false;
        return new FakeSource(parent, stats, value, delay);
    };
}
Snapshot resultOf(const QSignalSpy &spy, int index = 0)
{
    return qvariant_cast<Snapshot>(spy.at(index).at(1));
}
}
class MediaMetadataTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void publicMetadataAndPriority();
    void realSilentAndInvalid();
    void realEmbeddedPriority();
    void timeout();
    void cancellationAndReuse();
    void artworkGuards();
    void artworkSymlink();
    void legacy_data();
    void legacy();
    void legacyErrorsAndCancellation();
    void storeImportAndPreferences();
    void storeProbeReplacement();
};
void MediaMetadataTest::publicMetadataAndPriority()
{
    QTemporaryDir temp;
    const auto root = temp.filePath(u"\u97f3\u4e50 # %"_s);
    QVERIFY(put(root + u"/\u4e13\u8f91/2.wav"_s, wave()));
    QVERIFY(put(root + u"/\u4e13\u8f91/1.wav"_s, wave()));
    QVERIFY(put(root + u"/\u5355\u66f2.wav"_s, wave()));
    QImage red(24, 16, QImage::Format_ARGB32);
    red.fill(Qt::red);
    QDir().mkpath(root + u"/\u4e13\u8f91"_s);
    QVERIFY(red.save(root + u"/\u4e13\u8f91/cover.png"_s));
    QImage blue(16, 24, QImage::Format_ARGB32);
    blue.fill(Qt::blue);
    QVERIFY(blue.save(root + u"/\u4e13\u8f91/folder.png"_s));
    QVERIFY(red.save(root + u"/\u5355\u66f2.png"_s));
    MediaObservation value;
    value.status = MediaObservation::Loaded;
    value.hasAudio = true;
    value.durationMs = 1234;
    value.metadata.insert(QMediaMetaData::Title, u"\u6807\u9898 Unicode"_s);
    value.metadata.insert(QMediaMetaData::AlbumTitle, u"\u4e13\u8f91\u6807\u9898"_s);
    value.metadata.insert(QMediaMetaData::AlbumArtist, u"\u4e13\u8f91\u827a\u672f\u5bb6"_s);
    value.metadata.insert(QMediaMetaData::ContributingArtist,
                          QStringList{ u"\u6b4c\u624b\u7532"_s, u"\u6b4c\u624b\u4e59"_s });
    value.metadata.insert(QMediaMetaData::TrackNumber, 4);
    value.metadata.insert(QMediaMetaData::Date, QDate(2020, 2, 3));
    value.metadata.insert(QMediaMetaData::Genre, QStringList{ u"Rock"_s, u"Pop"_s });
    value.metadata.insert(QMediaMetaData::CoverArtImage, blue);
    auto stats = std::make_shared<FakeStats>();
    MediaMetadataProbe probe(nullptr, factory(stats, value, 10));
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    auto next = *scanned(root);
    next.albums[0].localNote = u"\u4fdd\u7559\u5907\u6ce8"_s;
    next.albums[0].tracks[0].bitsPerSample = 24;
    next.albums[0].tracks[0].fields.insert(u"future"_s, 9);
    probe.start(std::make_shared<const LibrarySnapshot>(next), temp.filePath(u"native"_s), { }, 1);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
    const auto result = resultOf(done);
    QVERIFY(result);
    QCOMPARE(stats->created, 2);
    QCOMPARE(stats->peak, 2);
    QVERIFY(stats->offGui);
    for (const auto &album : result->albums) {
        const auto &track = album.tracks.first();
        QCOMPARE(track.title, u"\u6807\u9898 Unicode"_s);
        QCOMPARE(track.duration, 1.234);
        QCOMPARE(track.trackNumber.value(), 4);
        QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
        QCOMPARE(album.year.value(), 2020);
        QCOMPARE(album.rawGenres, (QStringList{ u"Rock"_s, u"Pop"_s }));
        const auto color = QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0);
        QCOMPARE(color,
                 album.fields.value(u"_singleFile"_s).toBool() ? QColor(Qt::blue)
                                                               : QColor(Qt::red));
    }
    QCOMPARE(result->albums[0].localNote, u"\u4fdd\u7559\u5907\u6ce8"_s);
    QCOMPARE(result->albums[0].tracks[0].bitsPerSample.value(), 24);
    QCOMPARE(result->albums[0].tracks[0].fields.value(u"future"_s).toInt(), 9);
    const auto opens = stats->opens.load();
    probe.start(result, temp.filePath(u"native"_s), { }, 2);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 10000);
    QCOMPARE(stats->opens, opens);
    QCOMPARE(stats->created, 2);
    QCOMPARE(resultOf(done, 1)->albums[0].coverUrl, result->albums[0].coverUrl);
    QVERIFY(put(root + u"/\u4e13\u8f91/cover.png"_s, "corrupt preferred image"_ba));
    const auto changed =
            DirectoryScanner::scan({ QUrl::fromLocalFile(root) }, result, { }).snapshot;
    probe.start(changed, temp.filePath(u"native"_s), { }, 3);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 3, 10000);
    QCOMPARE(stats->opens, opens);
    for (const auto &album : resultOf(done, 2)->albums)
        QCOMPARE(QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0), QColor(Qt::blue));
    QVERIFY(QFile::remove(root + u"/\u4e13\u8f91/cover.png"_s));
    QVERIFY(QFile::remove(root + u"/\u4e13\u8f91/folder.png"_s));
    const auto withoutExternal =
            DirectoryScanner::scan({ QUrl::fromLocalFile(root) }, resultOf(done, 2), { }).snapshot;
    probe.start(withoutExternal, temp.filePath(u"native"_s), { }, 4);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 4, 10000);
    QCOMPARE(stats->opens, opens + 2);
    for (const auto &album : resultOf(done, 3)->albums)
        QCOMPARE(QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0), QColor(Qt::blue));
    Album removed;
    removed.fields.insert(u"coverUrl"_s, u"file:///old-cover.png"_s);
    QVERIFY(!toJson(removed).contains(u"coverUrl"_s));
}
void MediaMetadataTest::realSilentAndInvalid()
{
    QTemporaryDir temp;
    const auto root = temp.filePath(u"\u66f2\u5e93 Unicode"_s);
    const auto wav = root + u"/\u4e13\u8f91/valid.wav"_s;
    QVERIFY(put(wav, wave()));
    QVERIFY(put(root + u"/\u4e13\u8f91/broken.wav"_s, "broken"_ba));
    const auto original = get(wav);
    MediaMetadataProbe probe;
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    int ticks = 0;
    QTimer heartbeat;
    connect(&heartbeat, &QTimer::timeout, this, [&] {
        ++ticks;
    });
    heartbeat.start(1);
    probe.start(scanned(root), temp.filePath(u"native"_s), { }, 3);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    const auto result = resultOf(done);
    QCOMPARE(result->albums.size(), 1);
    QCOMPARE(result->albums.first().tracks.size(), 2);
    for (const auto &track : result->albums.first().tracks) {
        if (track.sourceUrl.toLocalFile().endsWith(u"valid.wav"_s)) {
            QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
            QCOMPARE(track.title, u"Anonymous Title"_s);
            QVERIFY(qAbs(track.duration - .1) < .002);
        } else {
            QCOMPARE(track.mediaSupport, MediaSupport::Unsupported);
            QCOMPARE(track.title, u"broken"_s);
            QCOMPARE(track.fields.value(u"_probeStatus"_s).toString(), u"invalid"_s);
        }
    }
    QVERIFY(ticks > 0);
    QCOMPARE(get(wav), original);
}
void MediaMetadataTest::realEmbeddedPriority()
{
    QTemporaryDir temp;
    const auto root = temp.filePath(u"music"_s);
    QImage embedded(20, 12, QImage::Format_ARGB32);
    embedded.fill(Qt::blue);
    QImage directory(12, 20, QImage::Format_ARGB32);
    directory.fill(Qt::red);
    QVERIFY(put(root + u"/single.wav"_s, wave(embedded)));
    QVERIFY(put(root + u"/album/song.wav"_s, wave(embedded)));
    QVERIFY(put(root + u"/fallback.wav"_s, wave()));
    QVERIFY(directory.save(root + u"/fallback.png"_s));
    QVERIFY(directory.save(root + u"/single.png"_s));
    QVERIFY(directory.save(root + u"/album/cover.png"_s));
    MediaMetadataProbe probe;
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    probe.start(scanned(root), temp.filePath(u"native"_s), { }, 7);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    for (const auto &album : resultOf(done)->albums) {
        QVERIFY(!album.coverUrl.isEmpty());
        QCOMPARE(QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0),
                 album.tracks.first().sourceUrl.toLocalFile().endsWith(u"/single.wav"_s)
                         ? QColor(Qt::blue)
                         : QColor(Qt::red));
    }
}
void MediaMetadataTest::timeout()
{
    QTemporaryDir temp;
    QVERIFY(put(temp.filePath(u"root/hang.wav"_s), wave()));
    auto stats = std::make_shared<FakeStats>();
    MediaMetadataProbe probe(nullptr, factory(stats, { }, -1));
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    probe.start(scanned(temp.filePath(u"root"_s)), temp.filePath(u"native"_s), { }, 4);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 11000);
    const auto track = resultOf(done)->albums.first().tracks.first();
    QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
    QCOMPARE(track.fields.value(u"_probeStatus"_s).toString(), u"timeout"_s);
    QCOMPARE(track.title, u"hang"_s);
    QCOMPARE(stats->active, 0);
    QVERIFY(!track.fields.contains(u"_qtProbeVersion"_s));
}
void MediaMetadataTest::cancellationAndReuse()
{
    QTemporaryDir temp;
    for (int i = 0; i < 80; ++i)
        QVERIFY(put(temp.filePath(u"root/album/"_s) + QString::number(i) + u".wav"_s, wave()));
    auto stats = std::make_shared<FakeStats>();
    MediaObservation loaded;
    loaded.status = MediaObservation::Loaded;
    loaded.hasAudio = true;
    MediaMetadataProbe probe(nullptr, factory(stats, loaded, 100));
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    const auto snapshot = scanned(temp.filePath(u"root"_s));
    auto cancel = std::make_shared<std::atomic_bool>(false);
    probe.start(snapshot, temp.filePath(u"native"_s), cancel, 10);
    QTRY_COMPARE(stats->active.load(), 2);
    cancel->store(true);
    probe.cancel();
    probe.start(snapshot, temp.filePath(u"native"_s), { }, 11);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 10000);
    QVERIFY(done[0][2].toBool());
    QVERIFY(!resultOf(done));
    QCOMPARE(done[1][0].toULongLong(), 11);
    QVERIFY(resultOf(done, 1));
    QCOMPARE(stats->created, 2);
    QCOMPARE(stats->peak, 2);
    QCOMPARE(stats->active, 0);
    QCOMPARE(resultOf(done, 1)->albums.first().tracks.size(), 80);
}
void MediaMetadataTest::artworkGuards()
{
    QTemporaryDir temp;
    const auto native = temp.filePath(u"native"_s);
    QImage large(2049, 2048, QImage::Format_RGB32);
    large.fill(Qt::red);
    const auto path = temp.filePath(u"large.png"_s);
    QVERIFY(large.save(path));
    QString error;
    QVERIFY(ArtworkStore::copy(path, native, &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(ArtworkStore::save(large, native, &error).isEmpty());
    QVERIFY(put(temp.filePath(u"broken.png"_s), "not an image"_ba));
    QVERIFY(ArtworkStore::copy(temp.filePath(u"broken.png"_s), native, &error).isEmpty());
    QFile oversized(temp.filePath(u"oversized.png"_s));
    QVERIFY(oversized.open(QIODevice::WriteOnly));
    QVERIFY(oversized.resize(ArtworkStore::maximumBytes + 1));
    oversized.close();
    QVERIFY(ArtworkStore::copy(oversized.fileName(), native, &error).isEmpty());
    QImage small(32, 16, QImage::Format_ARGB32);
    small.fill(Qt::transparent);
    const auto a = ArtworkStore::save(small, native, &error);
    QVERIFY(!a.isEmpty());
    const auto before = get(a.toLocalFile());
    QCOMPARE(ArtworkStore::save(small, native), a);
    QCOMPARE(get(a.toLocalFile()), before);
    small.fill(Qt::blue);
    QVERIFY(ArtworkStore::save(small, native) != a);
    QVERIFY(!ArtworkStore::save(small, temp.filePath(u"blocked/file"_s), &error).isEmpty());
    QVERIFY(put(temp.filePath(u"read-only-parent"_s), "file"_ba));
    QVERIFY(ArtworkStore::save(small, temp.filePath(u"read-only-parent/native"_s), &error)
                    .isEmpty());
}
void MediaMetadataTest::artworkSymlink()
{
    QTemporaryDir temp;
    const auto source = temp.filePath(u"legacy-artwork"_s);
    const auto native = temp.filePath(u"native"_s);
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(native));
    const auto path = [](const QString &value) {
#ifdef Q_OS_WIN
        return std::filesystem::path(value.toStdWString());
#else
        return std::filesystem::path(value.toUtf8().toStdString());
#endif
    };
    std::error_code error;
    std::filesystem::create_directory_symlink(path(source), path(native + "/artwork"_L1), error);
#ifdef Q_OS_WIN
    if (error)
        QSKIP("Native symlink permission unavailable; target coverage remains R22-P.");
#endif
    QVERIFY2(!error, error.message().c_str());
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QString failure;
    QVERIFY(ArtworkStore::save(image, native, &failure).isEmpty());
    QVERIFY(!failure.isEmpty());
    QCOMPARE(QDir(source).entryList(QDir::Files).size(), 0);
}
void MediaMetadataTest::legacy_data()
{
    QTest::addColumn<int>("version");
    QTest::newRow("metadata-v3") << 3;
    QTest::newRow("metadata-v4") << 4;
}
void MediaMetadataTest::legacy()
{
    QFETCH(int, version);
    QTemporaryDir temp;
    const auto music = temp.filePath(u"\u97f3\u4e50 # %"_s);
    const auto source = temp.filePath(u"music-data-v3"_s);
    const auto native = temp.filePath(u"native"_s);
    QVERIFY(put(music + u"/\u4e13\u8f91/song.wav"_s, wave()));
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QDir().mkpath(source + "/artwork"_L1);
    QVERIFY(image.save(source + u"/artwork/old.png"_s));
    auto next = *scanned(music);
    auto &album = next.albums.first();
    album.localNote = u"\u672c\u5730\u5907\u6ce8"_s;
    album.descriptionSource.insert(u"url"_s, u"https://example.invalid/source"_s);
    album.producers.append({ u"\u5236\u4f5c\u4eba"_s,
                             u"\u5236\u4f5c"_s,
                             u"local"_s,
                             album.tracks.first().id,
                             u"\u6b4c\u66f2"_s,
                             { },
                             { } });
    album.fields.insert(u"futureAlbum"_s, QJsonObject{ { u"unknown"_s, 1 } });
    album.fields.insert(
            u"_cover"_s,
            QJsonObject{ { u"path"_s, source + "/artwork/old.png"_L1 }, { u"embedded"_s, true } });
    auto &track = album.tracks.first();
    track.fields.insert(u"_embeddedCover"_s, album.fields.value(u"_cover"_s));
    track.fields.insert(u"_common"_s, QJsonObject{ { u"producer"_s, u"\u4fdd\u7559"_s } });
    track.fields.insert(u"futureTrack"_s, 7);
    auto json = toJson(next);
    auto inputAlbums = json.value(u"albums"_s).toArray();
    auto inputAlbum = inputAlbums.first().toObject();
    auto inputTracks = inputAlbum.value(u"tracks"_s).toArray();
    auto inputTrack = inputTracks.first().toObject();
    inputTrack.insert(u"_metadataVersion"_s, version);
    inputTrack.insert(u"_path"_s, track.sourceUrl.toLocalFile());
    inputTracks[0] = inputTrack;
    inputAlbum.insert(u"tracks"_s, inputTracks);
    inputAlbum.insert(u"_cover"_s, album.fields.value(u"_cover"_s));
    inputAlbum.insert(u"_root"_s, music);
    inputAlbums[0] = inputAlbum;
    json.insert(u"albums"_s, inputAlbums);
    json.insert(u"version"_s, 1);
    json.insert(u"futureIndex"_s, 42);
    QVERIFY(put(source + u"/library-index.json"_s, QJsonDocument(json).toJson()));
    const auto before = get(source + "/library-index.json"_L1);
    QVERIFY(put(
            source + u"/genre-rules.json"_s,
            QJsonDocument(QJsonObject{ { u"genres"_s,
                                         QJsonArray{ QJsonObject{ { u"id"_s, u"manual"_s },
                                                                  { u"name"_s, u"Manual"_s } } } },
                                       { u"albumOverrides"_s,
                                         QJsonObject{ { album.id, u"manual"_s } } } })
                    .toJson()));
    QVERIFY(put(
            source + u"/config.json"_s,
            QJsonDocument(QJsonObject{ { u"version"_s, 1 },
                                       { u"roots"_s, QJsonArray{ music } },
                                       { u"onlineEnabled"_s, true },
                                       { u"futureConfig"_s, 8 },
                                       { u"musicBrainzContact"_s, u"project@example.invalid"_s } })
                    .toJson()));
    QVERIFY(put(source + u"/pcm-cache/private"_s, "DO NOT COPY"_ba));
    const auto imported = LegacyImporter::read(source, native, { });
    QVERIFY2(imported.snapshot, qPrintable(imported.error));
    const auto &actual = imported.snapshot->albums.first();
    QCOMPARE(actual.id, album.id);
    QCOMPARE(actual.localNote, album.localNote);
    QCOMPARE(actual.descriptionSource, album.descriptionSource);
    QCOMPARE(actual.producers.first().trackId, track.id);
    QCOMPARE(actual.tracks.first().fields.value(u"futureTrack"_s).toInt(), 7);
    QCOMPARE(actual.tracks.first().mediaSupport, MediaSupport::Unknown);
    QCOMPARE(actual.tracks.first().sourceUrl, track.sourceUrl);
    const auto configuration =
            imported.snapshot->fields.value(u"metadataConfiguration"_s).toObject();
    QCOMPARE(configuration.value(u"futureConfig"_s).toInt(), 8);
    QCOMPARE(configuration.value(u"musicBrainzContact"_s).toString(), u"project@example.invalid"_s);
    for (const auto &key : { u"version"_s, u"roots"_s })
        QVERIFY(!configuration.contains(key));
    QVERIFY(actual.coverUrl.toLocalFile().startsWith(native + u"/artwork/"_s));
    QVERIFY(!QFileInfo::exists(native + u"/pcm-cache"_s));
    QVERIFY(!imported.snapshot->onlineEnabled);
    QCOMPARE(imported.snapshot->fields.value(u"futureIndex"_s).toInt(), 42);
    QCOMPARE(get(source + u"/library-index.json"_s), before);
    QCOMPARE(QImage(actual.coverUrl.toLocalFile()).pixelColor(0, 0), QColor(Qt::blue));
    // Rescan reuses imported fields and fingerprint; native probe fills only public metadata.
    const auto rescanned =
            DirectoryScanner::scan({ QUrl::fromLocalFile(music) }, imported.snapshot, { }).snapshot;
    QCOMPARE(rescanned->albums.first().tracks.first().fields.value(u"futureTrack"_s).toInt(), 7);
}
void MediaMetadataTest::legacyErrorsAndCancellation()
{
    QTemporaryDir temp;
    const auto source = temp.filePath(u"legacy"_s);
    const auto native = temp.filePath(u"native"_s);
    QVERIFY(put(source + u"/library-index.json"_s, "broken"_ba));
    QVERIFY(!LegacyImporter::read(source, native, { }).error.isEmpty());
    QVERIFY(put(source + u"/library-index.json"_s, "{\"version\":99,\"albums\":[]}"_ba));
    QVERIFY(!LegacyImporter::read(source, native, { }).error.isEmpty());
    QVERIFY(put(source + u"/library-index.json"_s, "{\"version\":1,\"albums\":[]}"_ba));
    QVERIFY(!LegacyImporter::read(source, source, { }).snapshot);
    QVERIFY(!LegacyImporter::read(source, source + u"/nested"_s, { }).snapshot);
    auto cancel = std::make_shared<std::atomic_bool>(true);
    const auto result = LegacyImporter::read(source, native, cancel);
    QVERIFY(result.cancelled);
    QVERIFY(!result.snapshot);
    QCOMPARE(get(source + u"/library-index.json"_s), "{\"version\":1,\"albums\":[]}"_ba);
}
void MediaMetadataTest::storeImportAndPreferences()
{
    QTemporaryDir temp;
    const auto source = temp.filePath(u"legacy"_s);
    const auto native = temp.filePath(u"native"_s);
    QVERIFY(put(source + u"/library-index.json"_s,
                "{\"version\":1,\"albums\":[],\"future\":42}"_ba));
    LibraryStore store(native);
    OfflineMetadataProvider provider;
    LibrarySession session(&provider);
    QSignalSpy ready(&store, &LibraryStore::snapshotReady);
    QSignalSpy resets(session.albums(), &QAbstractItemModel::modelReset);
    connect(&store, &LibraryStore::snapshotReady, &session, [&](Snapshot snapshot) {
        session.setSnapshot(*snapshot);
    });
    QVERIFY(store.importLegacy(QUrl::fromLocalFile(source)));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
    QCOMPARE(ready.size(), 1);
    QCOMPARE(resets.size(), 1);
    QCOMPARE(store.snapshot()->fields.value(u"future"_s).toInt(), 42);
    const auto cache = QJsonDocument::fromJson(get(native + "/library-index.json"_L1)).object();
    QCOMPARE(cache.value(u"nativeSchemaVersion"_s).toInt(), 1);
    PreferencesStore preferences(native);
    QSignalSpy imported(&preferences, &PreferencesStore::imported);
    QSignalSpy saved(&preferences, &PreferencesStore::saved);
    const auto exported = temp.filePath(u"browser-export.json"_s);
    QVERIFY(put(
            exported,
            "{\"arrayMode\":\"realistic\",\"rememberColumnPosition\":false,\"future\":{\"x\":7}}"_ba));
    const auto original = get(exported);
    QVERIFY(preferences.importJson(QUrl::fromLocalFile(exported)));
    QTRY_COMPARE(imported.size(), 1);
    QTRY_VERIFY(saved.size() > 0);
    QVERIFY(preferences.realistic());
    QVERIFY(!preferences.rememberColumnPosition());
    QCOMPARE(QJsonDocument::fromJson(get(native + u"/preferences.json"_s))
                     .object()
                     .value(u"future"_s)
                     .toObject()
                     .value(u"x"_s)
                     .toInt(),
             7);
    QCOMPARE(get(exported), original);
    QSignalSpy errors(&preferences, &PreferencesStore::saveError);
    QVERIFY(put(exported, "[]"_ba));
    QVERIFY(preferences.importJson(QUrl::fromLocalFile(exported)));
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(imported.size(), 1);
    QVERIFY(put(exported, "{\"arrayMode\":\"realistic\"}"_ba));
    const auto saves = saved.size();
    QVERIFY(preferences.importJson(QUrl::fromLocalFile(exported)));
    preferences.setNavigation(true, false);
    QTRY_VERIFY(saved.size() > saves);
    QCOMPARE(imported.size(), 1);
    QVERIFY(!preferences.realistic());
}
void MediaMetadataTest::storeProbeReplacement()
{
    QTemporaryDir temp;
    const auto rootA = temp.filePath(u"first"_s);
    const auto rootB = temp.filePath(u"last"_s);
    for (int i = 0; i < 30; ++i)
        QVERIFY(put(rootA + u"/album/"_s + QString::number(i) + u".wav"_s, wave()));
    QVERIFY(put(rootB + u"/single.wav"_s, wave()));
    LibraryStore store(temp.filePath(u"native"_s));
    QSignalSpy ready(&store, &LibraryStore::snapshotReady);
    bool replaced = false;
    connect(&store, &LibraryStore::statusChanged, this, [&] {
        if (!replaced && store.probing()) {
            replaced = true;
            store.setRoots({ QUrl::fromLocalFile(rootB) });
        }
    });
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(rootA) }));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 15000);
    QVERIFY(replaced);
    QCOMPARE(ready.size(), 1);
    QCOMPARE(store.snapshot()->albums.size(), 1);
    QCOMPARE(store.snapshot()->albums.first().tracks.first().sourceUrl.toLocalFile(),
             rootB + u"/single.wav"_s);
    QVERIFY(!store.probing());
}
QTEST_GUILESS_MAIN(MediaMetadataTest)
#include "MediaMetadataTest.moc"
