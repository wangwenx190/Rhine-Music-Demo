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
    const auto info = QByteArray("INFO") + chunk("INAM", QByteArray("Anonymous Title\0", 16))
            + chunk("IART", QByteArray("Anonymous Artist\0", 17))
            + chunk("IPRD", QByteArray("Anonymous Album\0", 16));
    QByteArray artwork;
    if (!embedded.isNull()) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        embedded.save(&buffer, "PNG");
        QByteArray frame("APIC", 4);
        const auto payload = QByteArray("\0image/png\0\x03\0", 13) + png;
        QDataStream sizes(&frame, QIODevice::Append);
        sizes.setByteOrder(QDataStream::BigEndian);
        sizes << quint32(payload.size()) << quint16(0);
        frame += payload;
        QByteArray id3("ID3\x03\0\0", 6);
        for (int shift : { 21, 14, 7, 0 })
            id3.append(char((frame.size() >> shift) & 0x7f));
        id3 += frame;
        artwork = chunk("id3 ", id3);
    }
    const auto body = QByteArray("WAVE") + chunk("fmt ", format) + chunk("LIST", info) + artwork
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
                emit changed();
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
private slots:
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
    const auto root = temp.filePath("音乐 # %");
    QVERIFY(put(root + "/专辑/2.wav", wave()));
    QVERIFY(put(root + "/专辑/1.wav", wave()));
    QVERIFY(put(root + "/单曲.wav", wave()));
    QImage red(24, 16, QImage::Format_ARGB32);
    red.fill(Qt::red);
    QDir().mkpath(root + "/专辑");
    QVERIFY(red.save(root + "/专辑/cover.png"));
    QImage blue(16, 24, QImage::Format_ARGB32);
    blue.fill(Qt::blue);
    QVERIFY(blue.save(root + "/专辑/folder.png"));
    QVERIFY(red.save(root + "/单曲.png"));
    MediaObservation value;
    value.status = MediaObservation::Loaded;
    value.hasAudio = true;
    value.durationMs = 1234;
    value.metadata.insert(QMediaMetaData::Title, "标题 Unicode");
    value.metadata.insert(QMediaMetaData::AlbumTitle, "专辑标题");
    value.metadata.insert(QMediaMetaData::AlbumArtist, "专辑艺术家");
    value.metadata.insert(QMediaMetaData::ContributingArtist, QStringList{ "歌手甲", "歌手乙" });
    value.metadata.insert(QMediaMetaData::TrackNumber, 4);
    value.metadata.insert(QMediaMetaData::Date, QDate(2020, 2, 3));
    value.metadata.insert(QMediaMetaData::Genre, QStringList{ "Rock", "Pop" });
    value.metadata.insert(QMediaMetaData::CoverArtImage, blue);
    auto stats = std::make_shared<FakeStats>();
    MediaMetadataProbe probe(nullptr, factory(stats, value, 10));
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    auto next = *scanned(root);
    next.albums[0].localNote = "保留备注";
    next.albums[0].tracks[0].bitsPerSample = 24;
    next.albums[0].tracks[0].fields.insert("future", 9);
    probe.start(std::make_shared<const LibrarySnapshot>(next), temp.filePath("native"), { }, 1);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
    const auto result = resultOf(done);
    QVERIFY(result);
    QCOMPARE(stats->created, 2);
    QCOMPARE(stats->peak, 2);
    QVERIFY(stats->offGui);
    for (const auto &album : result->albums) {
        const auto &track = album.tracks.first();
        QCOMPARE(track.title, "标题 Unicode");
        QCOMPARE(track.duration, 1.234);
        QCOMPARE(track.trackNumber.value(), 4);
        QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
        QCOMPARE(album.year.value(), 2020);
        QCOMPARE(album.rawGenres, (QStringList{ "Rock", "Pop" }));
        const auto color = QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0);
        QCOMPARE(color,
                 album.fields.value("_singleFile").toBool() ? QColor(Qt::blue) : QColor(Qt::red));
    }
    QCOMPARE(result->albums[0].localNote, "保留备注");
    QCOMPARE(result->albums[0].tracks[0].bitsPerSample.value(), 24);
    QCOMPARE(result->albums[0].tracks[0].fields.value("future").toInt(), 9);
    const auto opens = stats->opens.load();
    probe.start(result, temp.filePath("native"), { }, 2);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 10000);
    QCOMPARE(stats->opens, opens);
    QCOMPARE(stats->created, 2);
    QCOMPARE(resultOf(done, 1)->albums[0].coverUrl, result->albums[0].coverUrl);
    QVERIFY(put(root + "/专辑/cover.png", "corrupt preferred image"));
    const auto changed =
            DirectoryScanner::scan({ QUrl::fromLocalFile(root) }, result, { }).snapshot;
    probe.start(changed, temp.filePath("native"), { }, 3);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 3, 10000);
    QCOMPARE(stats->opens, opens);
    for (const auto &album : resultOf(done, 2)->albums)
        QCOMPARE(QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0), QColor(Qt::blue));
    QVERIFY(QFile::remove(root + "/专辑/cover.png"));
    QVERIFY(QFile::remove(root + "/专辑/folder.png"));
    const auto withoutExternal =
            DirectoryScanner::scan({ QUrl::fromLocalFile(root) }, resultOf(done, 2), { }).snapshot;
    probe.start(withoutExternal, temp.filePath("native"), { }, 4);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 4, 10000);
    QCOMPARE(stats->opens, opens + 2);
    for (const auto &album : resultOf(done, 3)->albums)
        QCOMPARE(QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0), QColor(Qt::blue));
    Album removed;
    removed.fields.insert("coverUrl", "file:///old-cover.png");
    QVERIFY(!toJson(removed).contains("coverUrl"));
}
void MediaMetadataTest::realSilentAndInvalid()
{
    QTemporaryDir temp;
    const auto root = temp.filePath("曲库 Unicode");
    const auto wav = root + "/专辑/valid.wav";
    QVERIFY(put(wav, wave()));
    QVERIFY(put(root + "/专辑/broken.wav", "broken"));
    const auto original = get(wav);
    MediaMetadataProbe probe;
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    int ticks = 0;
    QTimer heartbeat;
    connect(&heartbeat, &QTimer::timeout, this, [&] {
        ++ticks;
    });
    heartbeat.start(1);
    probe.start(scanned(root), temp.filePath("native"), { }, 3);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    const auto result = resultOf(done);
    QCOMPARE(result->albums.size(), 1);
    QCOMPARE(result->albums.first().tracks.size(), 2);
    for (const auto &track : result->albums.first().tracks) {
        if (track.sourceUrl.toLocalFile().endsWith("valid.wav")) {
            QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
            QCOMPARE(track.title, "Anonymous Title");
            QVERIFY(qAbs(track.duration - .1) < .002);
        } else {
            QCOMPARE(track.mediaSupport, MediaSupport::Unsupported);
            QCOMPARE(track.title, "broken");
            QCOMPARE(track.fields.value("_probeStatus").toString(), "invalid");
        }
    }
    QVERIFY(ticks > 0);
    QCOMPARE(get(wav), original);
}
void MediaMetadataTest::realEmbeddedPriority()
{
    QTemporaryDir temp;
    const auto root = temp.filePath("music");
    QImage embedded(20, 12, QImage::Format_ARGB32);
    embedded.fill(Qt::blue);
    QImage directory(12, 20, QImage::Format_ARGB32);
    directory.fill(Qt::red);
    QVERIFY(put(root + "/single.wav", wave(embedded)));
    QVERIFY(put(root + "/album/song.wav", wave(embedded)));
    QVERIFY(put(root + "/fallback.wav", wave()));
    QVERIFY(directory.save(root + "/fallback.png"));
    QVERIFY(directory.save(root + "/single.png"));
    QVERIFY(directory.save(root + "/album/cover.png"));
    MediaMetadataProbe probe;
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    probe.start(scanned(root), temp.filePath("native"), { }, 7);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 15000);
    for (const auto &album : resultOf(done)->albums) {
        QVERIFY(!album.coverUrl.isEmpty());
        QCOMPARE(QImage(album.coverUrl.toLocalFile()).pixelColor(0, 0),
                 album.tracks.first().sourceUrl.toLocalFile().endsWith("/single.wav")
                         ? QColor(Qt::blue)
                         : QColor(Qt::red));
    }
}
void MediaMetadataTest::timeout()
{
    QTemporaryDir temp;
    QVERIFY(put(temp.filePath("root/hang.wav"), wave()));
    auto stats = std::make_shared<FakeStats>();
    MediaMetadataProbe probe(nullptr, factory(stats, { }, -1));
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    probe.start(scanned(temp.filePath("root")), temp.filePath("native"), { }, 4);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 11000);
    const auto track = resultOf(done)->albums.first().tracks.first();
    QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
    QCOMPARE(track.fields.value("_probeStatus").toString(), "timeout");
    QCOMPARE(track.title, "hang");
    QCOMPARE(stats->active, 0);
    QVERIFY(!track.fields.contains("_qtProbeVersion"));
}
void MediaMetadataTest::cancellationAndReuse()
{
    QTemporaryDir temp;
    for (int i = 0; i < 80; ++i)
        QVERIFY(put(temp.filePath("root/album/") + QString::number(i) + ".wav", wave()));
    auto stats = std::make_shared<FakeStats>();
    MediaObservation loaded;
    loaded.status = MediaObservation::Loaded;
    loaded.hasAudio = true;
    MediaMetadataProbe probe(nullptr, factory(stats, loaded, 100));
    QSignalSpy done(&probe, &MediaMetadataProbe::finished);
    const auto snapshot = scanned(temp.filePath("root"));
    auto cancel = std::make_shared<std::atomic_bool>(false);
    probe.start(snapshot, temp.filePath("native"), cancel, 10);
    QTRY_COMPARE(stats->active.load(), 2);
    cancel->store(true);
    probe.cancel();
    probe.start(snapshot, temp.filePath("native"), { }, 11);
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
    const auto native = temp.filePath("native");
    QImage large(2049, 2048, QImage::Format_RGB32);
    large.fill(Qt::red);
    const auto path = temp.filePath("large.png");
    QVERIFY(large.save(path));
    QString error;
    QVERIFY(ArtworkStore::copy(path, native, &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(ArtworkStore::save(large, native, &error).isEmpty());
    QVERIFY(put(temp.filePath("broken.png"), "not an image"));
    QVERIFY(ArtworkStore::copy(temp.filePath("broken.png"), native, &error).isEmpty());
    QFile oversized(temp.filePath("oversized.png"));
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
    QVERIFY(!ArtworkStore::save(small, temp.filePath("blocked/file"), &error).isEmpty());
    QVERIFY(put(temp.filePath("read-only-parent"), "file"));
    QVERIFY(ArtworkStore::save(small, temp.filePath("read-only-parent/native"), &error).isEmpty());
}
void MediaMetadataTest::artworkSymlink()
{
    QTemporaryDir temp;
    const auto source = temp.filePath("legacy-artwork");
    const auto native = temp.filePath("native");
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
    std::filesystem::create_directory_symlink(path(source), path(native + "/artwork"), error);
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
    const auto music = temp.filePath("音乐 # %");
    const auto source = temp.filePath("music-data-v3");
    const auto native = temp.filePath("native");
    QVERIFY(put(music + "/专辑/song.wav", wave()));
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QDir().mkpath(source + "/artwork");
    QVERIFY(image.save(source + "/artwork/old.png"));
    auto next = *scanned(music);
    auto &album = next.albums.first();
    album.localNote = "本地备注";
    album.descriptionSource.insert("url", "https://example.invalid/source");
    album.producers.append(
            { "制作人", "制作", "local", album.tracks.first().id, "歌曲", { }, { } });
    album.fields.insert("futureAlbum", QJsonObject{ { "unknown", 1 } });
    album.fields.insert(
            "_cover", QJsonObject{ { "path", source + "/artwork/old.png" }, { "embedded", true } });
    auto &track = album.tracks.first();
    track.fields.insert("_embeddedCover", album.fields.value("_cover"));
    track.fields.insert("_common", QJsonObject{ { "producer", "保留" } });
    track.fields.insert("futureTrack", 7);
    auto json = toJson(next);
    auto inputAlbums = json.value("albums").toArray();
    auto inputAlbum = inputAlbums.first().toObject();
    auto inputTracks = inputAlbum.value("tracks").toArray();
    auto inputTrack = inputTracks.first().toObject();
    inputTrack.insert("_metadataVersion", version);
    inputTrack.insert("_path", track.sourceUrl.toLocalFile());
    inputTracks[0] = inputTrack;
    inputAlbum.insert("tracks", inputTracks);
    inputAlbum.insert("_cover", album.fields.value("_cover"));
    inputAlbum.insert("_root", music);
    inputAlbums[0] = inputAlbum;
    json.insert("albums", inputAlbums);
    json.insert("version", 1);
    json.insert("futureIndex", 42);
    QVERIFY(put(source + "/library-index.json", QJsonDocument(json).toJson()));
    const auto before = get(source + "/library-index.json");
    QVERIFY(put(source + "/genre-rules.json",
                QJsonDocument(
                        QJsonObject{ { "genres",
                                       QJsonArray{ QJsonObject{ { "id", "manual" },
                                                                { "name", "Manual" } } } },
                                     { "albumOverrides", QJsonObject{ { album.id, "manual" } } } })
                        .toJson()));
    QVERIFY(put(source + "/config.json",
                QJsonDocument(QJsonObject{ { "version", 1 },
                                           { "roots", QJsonArray{ music } },
                                           { "onlineEnabled", true },
                                           { "futureConfig", 8 },
                                           { "musicBrainzContact", "project@example.invalid" } })
                        .toJson()));
    QVERIFY(put(source + "/pcm-cache/private", "DO NOT COPY"));
    const auto imported = LegacyImporter::read(source, native, { });
    QVERIFY2(imported.snapshot, qPrintable(imported.error));
    const auto &actual = imported.snapshot->albums.first();
    QCOMPARE(actual.id, album.id);
    QCOMPARE(actual.localNote, album.localNote);
    QCOMPARE(actual.descriptionSource, album.descriptionSource);
    QCOMPARE(actual.producers.first().trackId, track.id);
    QCOMPARE(actual.tracks.first().fields.value("futureTrack").toInt(), 7);
    QCOMPARE(actual.tracks.first().mediaSupport, MediaSupport::Unknown);
    QCOMPARE(actual.tracks.first().sourceUrl, track.sourceUrl);
    const auto configuration = imported.snapshot->fields.value("metadataConfiguration").toObject();
    QCOMPARE(configuration.value("futureConfig").toInt(), 8);
    QCOMPARE(configuration.value("musicBrainzContact").toString(),
             QString("project@example.invalid"));
    for (const auto &key : { "version", "roots" })
        QVERIFY(!configuration.contains(key));
    QVERIFY(actual.coverUrl.toLocalFile().startsWith(native + "/artwork/"));
    QVERIFY(!QFileInfo::exists(native + "/pcm-cache"));
    QVERIFY(!imported.snapshot->onlineEnabled);
    QCOMPARE(imported.snapshot->fields.value("futureIndex").toInt(), 42);
    QCOMPARE(get(source + "/library-index.json"), before);
    QCOMPARE(QImage(actual.coverUrl.toLocalFile()).pixelColor(0, 0), QColor(Qt::blue));
    // Rescan reuses imported fields and fingerprint; native probe fills only public metadata.
    const auto rescanned =
            DirectoryScanner::scan({ QUrl::fromLocalFile(music) }, imported.snapshot, { }).snapshot;
    QCOMPARE(rescanned->albums.first().tracks.first().fields.value("futureTrack").toInt(), 7);
}
void MediaMetadataTest::legacyErrorsAndCancellation()
{
    QTemporaryDir temp;
    const auto source = temp.filePath("legacy");
    const auto native = temp.filePath("native");
    QVERIFY(put(source + "/library-index.json", "broken"));
    QVERIFY(!LegacyImporter::read(source, native, { }).error.isEmpty());
    QVERIFY(put(source + "/library-index.json", "{\"version\":99,\"albums\":[]}"));
    QVERIFY(!LegacyImporter::read(source, native, { }).error.isEmpty());
    QVERIFY(put(source + "/library-index.json", "{\"version\":1,\"albums\":[]}"));
    QVERIFY(!LegacyImporter::read(source, source, { }).snapshot);
    QVERIFY(!LegacyImporter::read(source, source + "/nested", { }).snapshot);
    auto cancel = std::make_shared<std::atomic_bool>(true);
    const auto result = LegacyImporter::read(source, native, cancel);
    QVERIFY(result.cancelled);
    QVERIFY(!result.snapshot);
    QCOMPARE(get(source + "/library-index.json"), "{\"version\":1,\"albums\":[]}");
}
void MediaMetadataTest::storeImportAndPreferences()
{
    QTemporaryDir temp;
    const auto source = temp.filePath("legacy");
    const auto native = temp.filePath("native");
    QVERIFY(put(source + "/library-index.json", "{\"version\":1,\"albums\":[],\"future\":42}"));
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
    QCOMPARE(store.snapshot()->fields.value("future").toInt(), 42);
    const auto cache = QJsonDocument::fromJson(get(native + "/library-index.json")).object();
    QCOMPARE(cache.value("nativeSchemaVersion").toInt(), 1);
    PreferencesStore preferences(native);
    QSignalSpy imported(&preferences, &PreferencesStore::imported);
    QSignalSpy saved(&preferences, &PreferencesStore::saved);
    const auto exported = temp.filePath("browser-export.json");
    QVERIFY(put(
            exported,
            "{\"arrayMode\":\"realistic\",\"rememberColumnPosition\":false,\"future\":{\"x\":7}}"));
    const auto original = get(exported);
    QVERIFY(preferences.importJson(QUrl::fromLocalFile(exported)));
    QTRY_COMPARE(imported.size(), 1);
    QTRY_VERIFY(saved.size() > 0);
    QVERIFY(preferences.realistic());
    QVERIFY(!preferences.rememberColumnPosition());
    QCOMPARE(QJsonDocument::fromJson(get(native + "/preferences.json"))
                     .object()
                     .value("future")
                     .toObject()
                     .value("x")
                     .toInt(),
             7);
    QCOMPARE(get(exported), original);
    QSignalSpy errors(&preferences, &PreferencesStore::saveError);
    QVERIFY(put(exported, "[]"));
    QVERIFY(preferences.importJson(QUrl::fromLocalFile(exported)));
    QTRY_COMPARE(errors.size(), 1);
    QCOMPARE(imported.size(), 1);
    QVERIFY(put(exported, "{\"arrayMode\":\"realistic\"}"));
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
    const auto rootA = temp.filePath("first");
    const auto rootB = temp.filePath("last");
    for (int i = 0; i < 30; ++i)
        QVERIFY(put(rootA + "/album/" + QString::number(i) + ".wav", wave()));
    QVERIFY(put(rootB + "/single.wav", wave()));
    LibraryStore store(temp.filePath("native"));
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
             rootB + "/single.wav");
    QVERIFY(!store.probing());
}
QTEST_GUILESS_MAIN(MediaMetadataTest)
#include "MediaMetadataTest.moc"
