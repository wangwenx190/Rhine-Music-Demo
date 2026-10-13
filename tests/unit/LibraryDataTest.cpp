#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include <QAbstractItemModelTester>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QSet>
#include <QTest>
#include <algorithm>

using namespace yalahoo;
class TestProvider final : public IMetadataProvider
{
public:
    using IMetadataProvider::IMetadataProvider;
    QList<RequestToken> requests, cancelled;
    QList<MetadataKind> kinds;
    ProviderCapabilities capabilities() const override
    {
        return { };
    }
    void requestIntroduction(AlbumQuery, RequestToken token) override
    {
        requests.append(token);
        kinds.append(MetadataKind::Introduction);
    }
    void requestCredits(AlbumQuery, RequestToken token) override
    {
        requests.append(token);
        kinds.append(MetadataKind::Credits);
    }
    void requestEnrichment(AlbumQuery, RequestToken token) override
    {
        requests.append(token);
        kinds.append(MetadataKind::Enrichment);
    }
    void cancel(RequestToken token) override
    {
        cancelled.append(token);
    }
    void deliver(int index, ProviderStatus status, quint64 generationOffset = 0,
                 QString albumOverride = { })
    {
        const auto token = requests[index];
        ProviderResult result;
        result.requestId = token.requestId;
        result.generation = token.generation + generationOffset;
        result.albumId = albumOverride.isEmpty() ? token.albumId : albumOverride;
        result.kind = kinds[index];
        result.status = status;
        result.payload = { { "test", true } };
        emit resultReady(result);
    }
};
class LibraryDataTest : public QObject
{
    Q_OBJECT
    QJsonObject fixtures, golden;
    LibrarySnapshot library(const QString &name = QStringLiteral("mixed")) const
    {
        LibrarySnapshot result;
        if (!readLibrary(fixtures.value(name).toObject(), result))
            qFatal("Invalid frozen fixture");
        return result;
    }
private slots:
    void browseSnapshotReplacementPreservesPlayback()
    {
        OfflineMetadataProvider provider;
        LibrarySession session(&provider);
        auto snapshot = library("mixed");
        QVERIFY(!snapshot.albums.isEmpty());
        const auto id = snapshot.albums.first().id;
        session.setSnapshot(snapshot);
        QVERIFY(session.navigate(id));
        QSignalSpy changes(&session, &LibrarySession::browseInfoChanged);
        snapshot.albums.first().title = QStringLiteral("替换后的收藏标题");
        session.setSnapshot(snapshot);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(session.browseInfo().value("title").toString(), snapshot.albums.first().title);
        QCOMPARE(session.navigationAlbumId(), id);
        QVERIFY(session.playbackTrackId().isEmpty());
        session.search(QStringLiteral("其他歌曲"));
        QCOMPARE(changes.size(), 1);
        session.setSnapshot({ });
        QVERIFY(session.browseInfo().isEmpty());
        QVERIFY(session.navigationAlbumId().isEmpty());
        QVERIFY(session.playbackTrackId().isEmpty());
    }
    void initTestCase()
    {
        QFile input(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/web-baseline/anonymous.json"));
        QVERIFY(input.open(QIODevice::ReadOnly));
        fixtures = QJsonDocument::fromJson(input.readAll()).object().value("libraries").toObject();
        QFile order(QStringLiteral(FIXTURE_ROOT "/docs/contracts/library-order.json"));
        QVERIFY(order.open(QIODevice::ReadOnly));
        golden = QJsonDocument::fromJson(order.readAll()).object().value("libraries").toObject();
    }
    void frozenColumnOrder()
    {
        for (auto it = fixtures.begin(); it != fixtures.end(); ++it) {
            const auto snapshot = library(it.key());
            const auto order = orderLibrary(snapshot, SortMode::Genre);
            const auto expected = golden.value(it.key()).toObject();
            QStringList ids;
            for (int index : order.indices)
                ids.append(snapshot.albums[index].id);
            QStringList expectedIds;
            for (const auto &id : expected.value("albumIds").toArray())
                expectedIds.append(id.toString());
            QCOMPARE(ids, expectedIds);
            QCOMPARE(order.slotStride, expected.value("slotStride").toInt());
            const auto columns = expected.value("columns").toArray();
            QCOMPARE(order.columns.size(), columns.size());
            for (int i = 0; i < columns.size(); ++i) {
                QCOMPARE(order.columns[i].label, columns[i].toObject().value("label").toString());
                QStringList expectedColumn;
                for (const auto &id : columns[i].toObject().value("albumIds").toArray())
                    expectedColumn.append(id.toString());
                QCOMPARE(order.columns[i].albumIds, expectedColumn);
            }
        }
    }
    void normalizedSortingAndGrouping()
    {
        auto snapshot = library(QStringLiteral("odd"));
        snapshot.albums[0].title = QStringLiteral(" Album 10 ");
        snapshot.albums[0].artist = QStringLiteral("Ａ  Artist");
        snapshot.albums[1].title = QStringLiteral("Ａlbum 2");
        snapshot.albums[1].artist = QStringLiteral("a Artist");
        snapshot.albums[2].title = QStringLiteral("Album 1");
        snapshot.albums[2].artist = QStringLiteral("B");
        const auto ordered = orderLibrary(snapshot, SortMode::Artist);
        QCOMPARE(ordered.indices, QList<int>({ 1, 0, 2 }));
        QCOMPARE(ordered.columns.size(), 2);
        QCOMPARE(ordered.columns[0].albumIds,
                 QStringList({ snapshot.albums[1].id, snapshot.albums[0].id }));
        QCOMPARE(orderLibrary(snapshot, SortMode::Album).indices, QList<int>({ 2, 1, 0 }));
        QCOMPARE(snapshot.albums[0].title, QStringLiteral(" Album 10 "));
        snapshot.albums[0].title = snapshot.albums[1].title;
        QCOMPARE(orderLibrary(snapshot, SortMode::Artist).indices.first(), 0);
        auto mixed = library();
        mixed.genres[1].name = mixed.genres[0].name;
        QCOMPARE(orderLibrary(mixed, SortMode::Genre).columns.size(), 3);
        auto albums = orderLibrary(mixed, SortMode::Album);
        QCOMPARE(albums.columns.size(), 2);
        QCOMPARE(albums.columns[0].label, QStringLiteral("01–12"));
        QCOMPARE(albums.columns[1].label, QStringLiteral("13–21"));
        // Equivalent locale keys stay contiguous through normalized-string fallback.
        snapshot.albums[0].artist = QStringLiteral("é");
        snapshot.albums[1].artist = QStringLiteral("e");
        snapshot.albums[2].artist = QStringLiteral("é");
        QCOMPARE(orderLibrary(snapshot, SortMode::Artist).columns.size(), 2);
    }
    void largeDistinctColumns()
    {
        LibrarySnapshot snapshot;
        for (int i = 1023; i >= 0; --i) {
            Album album;
            album.id = QStringLiteral("album-%1").arg(i);
            album.title = QStringLiteral(" Album %1 ").arg(i);
            album.artist = QStringLiteral(" Artist %1 ").arg(i);
            album.genreId = QStringLiteral("genre-%1").arg(i % 128);
            snapshot.albums.append(album);
        }
        snapshot.genres.append({ QStringLiteral("genre-7"), QStringLiteral("Known genre") });
        for (const auto mode : { SortMode::Artist, SortMode::Album, SortMode::Genre }) {
            const auto order = orderLibrary(snapshot, mode);
            QCOMPARE(order.indices.size(), snapshot.albums.size());
            QStringList members;
            for (const auto &column : order.columns)
                members.append(column.albumIds);
            QCOMPARE(members.size(), snapshot.albums.size());
            QCOMPARE(QSet<QString>(members.begin(), members.end()).size(), snapshot.albums.size());
            if (mode == SortMode::Genre) {
                QCOMPARE(order.columns.size(), 128);
                QCOMPARE(order.columns.first().key, QStringLiteral("genre-7"));
                QCOMPARE(order.columns.first().label, QStringLiteral("Known genre"));
                QCOMPARE(order.columns[1].key, QStringLiteral("genre-127"));
                QCOMPARE(order.indices.first(), 0);
            } else {
                QCOMPARE(snapshot.albums[order.indices.first()].id, QStringLiteral("album-0"));
                QCOMPARE(snapshot.albums[order.indices.last()].id, QStringLiteral("album-1023"));
                QCOMPARE(order.columns.size(), mode == SortMode::Artist ? 1024 : 86);
            }
        }
    }
    void searchRowsFollowSnapshot()
    {
        LibrarySnapshot snapshot;
        for (int i = 0; i < 2; ++i) {
            Album album;
            album.id = QStringLiteral("album-%1").arg(i);
            album.title = QStringLiteral("Collection %1").arg(i);
            Track track;
            track.id = QStringLiteral("same-local-id");
            track.albumId = album.id;
            track.title = QStringLiteral("Song %1").arg(i);
            track.artist = QStringLiteral("Artist %1").arg(i);
            album.tracks.append(track);
            snapshot.albums.append(album);
        }
        SearchListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto first = std::make_shared<const LibrarySnapshot>(snapshot);
        model.update(first, QStringLiteral("song"), { });
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), SearchListModel::Title), QVariant("Song 0"));
        QCOMPARE(model.data(model.index(1), SearchListModel::Title), QVariant("Song 1"));
        QCOMPARE(model.data(model.index(1), SearchListModel::Artist), QVariant("Artist 1"));
        QCOMPARE(model.data(model.index(1), SearchListModel::Kind), QVariant("track"));
        model.update(first, QStringLiteral("collection"), { });
        QCOMPARE(model.data(model.index(1), SearchListModel::Title), QVariant("Collection 1"));
        QCOMPARE(model.data(model.index(1), SearchListModel::TrackId), QVariant(QString()));
        snapshot.albums[1].tracks[0].title = QStringLiteral("Song replacement");
        model.update(std::make_shared<const LibrarySnapshot>(snapshot), QStringLiteral("song"),
                     { });
        QCOMPARE(model.data(model.index(1), SearchListModel::Title), QVariant("Song replacement"));
        model.update({ }, QString(), { });
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.data(model.index(0), SearchListModel::Title).isValid());
    }
    void frozenSearch()
    {
        QFile file(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/core/search-golden.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases =
                QJsonDocument::fromJson(file.readAll()).object().value("cases").toArray();
        for (const auto &entry : cases) {
            const auto object = entry.toObject();
            const auto actual = searchLibrary(library(object.value("library").toString()),
                                              object.value("query").toString(),
                                              object.value("genreId").toString());
            QJsonArray ids;
            for (const auto &hit : actual)
                ids.append(QJsonObject{ { "albumId", hit.albumId }, { "trackId", hit.trackId } });
            QCOMPARE(ids, object.value("results").toArray());
        }
    }
    void roundTripAndLocalUrls()
    {
        auto input = fixtures.value("single").toObject();
        auto album = input.value("albums").toArray().first().toObject();
        album.insert("customAlbum", QJsonObject{ { "nested", 42 } });
        album.insert("folder", QDir::tempPath() + QStringLiteral("/音乐 目录"));
        album.insert("descriptionSource",
                     QJsonObject{ { "name", "manual" },
                                  { "url", "https://example.invalid/info" },
                                  { "license", "test" },
                                  { "custom", true } });
        album.insert("introduction",
                     QJsonObject{ { "status", "uncertain" },
                                  { "candidateUrl", "https://example.invalid/candidate" } });
        album.insert("creditsLookup",
                     QJsonObject{ { "status", "partial" },
                                  { "source", "QQ Music" },
                                  { "matchedTracks", 1 },
                                  { "totalTracks", 3 } });
        album.insert("online",
                     QJsonObject{ { "status", "matched" },
                                  { "releaseId", "release" },
                                  { "descriptionStatus", "available" } });
        auto tracks = album.value("tracks").toArray();
        auto track = tracks.first().toObject();
        track.insert("customTrack", 7);
        track.insert("browserPlayable", true);
        track.insert("sourceUrl",
                     QUrl::fromLocalFile(QDir::tempPath() + QStringLiteral("/音乐 # 1.FLAC"))
                             .toString());
        track.insert("bitsPerSample", 24);
        track.insert("lossless", true);
        tracks[0] = track;
        album.insert("tracks", tracks);
        input.insert("albums", QJsonArray{ album });
        input.insert("customLibrary", true);
        LibrarySnapshot snapshot;
        QVERIFY(readLibrary(input, snapshot));
        QVERIFY(snapshot.albums.first().folder.isLocalFile());
        const auto &loaded = snapshot.albums.first().tracks.first();
        QVERIFY(loaded.sourceUrl.isLocalFile());
        QVERIFY(loaded.sourceUrl.toLocalFile().endsWith(QStringLiteral("音乐 # 1.FLAC")));
        QCOMPARE(loaded.mediaSupport, MediaSupport::Unknown);
        QCOMPARE(loaded.bitsPerSample, std::optional<int>(24));
        const auto saved = toJson(snapshot);
        QCOMPARE(saved.value("customLibrary"), input.value("customLibrary"));
        auto savedAlbum = saved.value("albums").toArray().first().toObject();
        QCOMPARE(savedAlbum.value("customAlbum"), album.value("customAlbum"));
        for (const auto &key : { "descriptionSource", "introduction", "creditsLookup", "online" })
            QCOMPARE(savedAlbum.value(key), album.value(key));
        QCOMPARE(savedAlbum.value("tracks").toArray().first().toObject().value("customTrack"),
                 QJsonValue(7));
        LibrarySnapshot again;
        QVERIFY(readLibrary(saved, again));
        QCOMPARE(toJson(again), saved);
        track.insert("sourceUrl", "https://example.invalid/song.flac");
        tracks[0] = track;
        album.insert("tracks", tracks);
        input.insert("albums", QJsonArray{ album });
        QVERIFY(readLibrary(input, snapshot));
        QVERIFY(snapshot.albums.first().tracks.first().sourceUrl.isEmpty());
        input.insert("albums", QJsonArray{ album, album });
        QString error;
        const auto previous = toJson(snapshot);
        QVERIFY(!readLibrary(input, snapshot, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(toJson(snapshot), previous);
        album.insert("tracks", true);
        input.insert("albums", QJsonArray{ album });
        QVERIFY(!readLibrary(input, snapshot, &error));
        QCOMPARE(toJson(snapshot), previous);
    }
    void modelNotificationsAndIdentity()
    {
        OfflineMetadataProvider provider;
        LibrarySession session(&provider);
        QAbstractItemModelTester albumTester(
                session.albums(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester searchTester(
                session.searchResults(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy resets(session.albums(), &QAbstractItemModel::modelReset);
        QSignalSpy counts(session.albums(), &AlbumListModel::countChanged);
        QSignalSpy playback(&session, &LibrarySession::playbackChanged);
        session.setSnapshot(library());
        QCOMPARE(resets.size(), 1);
        QCOMPARE(counts.size(), 1);
        QCOMPARE(session.albums()->rowCount(), 21);
        QCOMPARE(session.albums()->roleNames().value(AlbumListModel::AlbumId),
                 QByteArray("albumId"));
        const auto &a = session.snapshot()->albums[0];
        const auto albumId = a.id, trackId = a.tracks[0].id;
        QVERIFY(session.setPlaybackIds(albumId, trackId));
        playback.clear();
        session.sort("artist");
        session.search("Typography");
        QCOMPARE(session.searchResults()->rowCount(), 28);
        QVERIFY(session.navigate(session.snapshot()->albums[1].id));
        session.requestMetadata(albumId, "introduction");
        QTRY_COMPARE(session.metadata().size(), 1);
        QCOMPARE(playback.size(), 0);
        QCOMPARE(session.playbackAlbumId(), albumId);
        QCOMPARE(session.playbackTrackId(), trackId);
        QVERIFY(!session.navigate("missing"));
        QVERIFY(!session.setPlaybackIds(albumId, "missing"));
        session.setSnapshot(library("empty"));
        QCOMPARE(session.albums()->rowCount(), 0);
        QCOMPARE(session.searchResults()->rowCount(), 0);
        QVERIFY(session.navigationAlbumId().isEmpty());
        QCOMPARE(session.playbackTrackId(), trackId);
        QVERIFY(!session.albums()
                         ->data(session.albums()->index(0), AlbumListModel::Title)
                         .isValid());
    }
    void offlineCacheAndCancellation()
    {
        OfflineMetadataProvider provider;
        auto snapshot = library("single");
        auto &album = snapshot.albums[0];
        album.introduction = { { "status", "matched" }, { "checkedAt", "2026-10-01" } };
        album.creditsLookup = { { "status", "not-found" },
                                { "retryAt", "2026-11-01" },
                                { "source", "QQ Music" } };
        provider.setCache(std::make_shared<const LibrarySnapshot>(snapshot));
        QSignalSpy spy(&provider, &IMetadataProvider::resultReady);
        AlbumQuery query;
        query.albumId = album.id;
        query.force = true;
        provider.requestIntroduction(query, { 1, 2, album.id });
        QCOMPARE(spy.size(), 0);
        QTRY_COMPARE(spy.size(), 1);
        auto result = qvariant_cast<ProviderResult>(spy.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::Matched);
        QCOMPARE(result.checkedAt, QStringLiteral("2026-10-01"));
        QCOMPARE(result.payload.value("description").toString(), album.description);
        provider.requestCredits(query, { 2, 2, album.id });
        QTRY_COMPARE(spy.size(), 1);
        result = qvariant_cast<ProviderResult>(spy.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::NotFound);
        QCOMPARE(result.retryAt, QStringLiteral("2026-11-01"));
        provider.requestEnrichment(query, { 3, 2, album.id });
        QTRY_COMPARE(spy.size(), 1);
        result = qvariant_cast<ProviderResult>(spy.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::Deferred);
        QVERIFY(result.retryAt.isEmpty());
        QVERIFY(result.checkedAt.isEmpty());
        QVERIFY(result.payload.isEmpty());
        QCOMPARE(result.error, QStringLiteral("联网资料功能暂未启用"));
        provider.requestIntroduction(query, { 4, 2, album.id });
        provider.cancel({ 4, 2, album.id });
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(qvariant_cast<ProviderResult>(spy.takeFirst()[0]).status,
                 ProviderStatus::Cancelled);
        query.albumId = "missing";
        provider.requestCredits(query, { 5, 2, query.albumId });
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(qvariant_cast<ProviderResult>(spy.takeFirst()[0]).status,
                 ProviderStatus::Deferred);
        QVERIFY(!provider.capabilities().networkEnabled);
        auto *temporary = new OfflineMetadataProvider;
        connect(temporary, &IMetadataProvider::resultReady, this, [] {
            QFAIL("Provider emitted after destruction");
        });
        temporary->requestIntroduction(query, { 1, 0, query.albumId });
        delete temporary;
        QCoreApplication::processEvents();
    }
    void injectedProviderStaleResults()
    {
        TestProvider provider;
        LibrarySession session(&provider);
        session.setSnapshot(library("single"));
        const QString albumId = session.snapshot()->albums[0].id;
        QSignalSpy results(&session, &LibrarySession::metadataDelivered);
        session.requestMetadata(albumId, "introduction");
        session.requestMetadata(albumId, "introduction");
        QCOMPARE(provider.cancelled.size(), 1);
        provider.deliver(0, ProviderStatus::Matched);
        provider.deliver(1, ProviderStatus::Matched, 1);
        provider.deliver(1, ProviderStatus::Matched, 0, "wrong");
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 0);
        provider.deliver(1, ProviderStatus::Uncertain);
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(session.metadata()
                         .value(albumId + "/introduction")
                         .toMap()
                         .value("status")
                         .toString(),
                 QStringLiteral("Uncertain"));
        provider.deliver(1, ProviderStatus::Error);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 1);
        const auto cancelled = session.requestMetadata(albumId, "credits");
        session.cancelMetadata(cancelled);
        provider.deliver(2, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 1);
        session.requestMetadata(albumId, "enrichment");
        session.setSnapshot(library("single"));
        provider.deliver(3, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 1);
        session.requestMetadata(albumId, "credits");
        provider.deliver(4, ProviderStatus::Error);
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(session.metadata().value(albumId + "/credits").toMap().value("status").toString(),
                 QStringLiteral("Error"));
        session.requestMetadata(albumId, "enrichment");
        provider.deliver(5, ProviderStatus::Partial);
        QTRY_COMPARE(results.size(), 3);
        auto *destroyed = new LibrarySession(&provider);
        destroyed->setSnapshot(library("single"));
        destroyed->requestMetadata(albumId, "introduction");
        delete destroyed;
        provider.deliver(6, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        auto *shortLived = new TestProvider;
        LibrarySession survivor(shortLived);
        survivor.setSnapshot(library("single"));
        survivor.requestMetadata(albumId, "introduction");
        delete shortLived;
        QCOMPARE(survivor.requestMetadata(albumId, "credits"), quint64(0));
    }
};
QTEST_MAIN(LibraryDataTest)
#include "LibraryDataTest.moc"
