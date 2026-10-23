// Uses an in-process HTTP fixture to verify provider pacing, cancellation, matching and cache
// provenance. Explicit fixture literals are independent inputs or expected results; timing
// bounds have named constants.

#include "OnlineMetadataProvider.h"
#include "LibrarySession.h"
#include "LibraryStore.h"
#include "DetailModel.h"
#include "metadata/MetadataHttp.h"
#include "metadata/MetadataWorker.h"
#include "metadata/MetadataCachePolicy.h"
#include "metadata/MetadataMerge.h"
#include "metadata/MusicBrainzSource.h"
#include "metadata/WikipediaSource.h"
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
#include <cstring>
#include <algorithm>
#include <memory>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int BackendDeadlineMilliseconds = 5000;
constexpr int QueuedEventObservationMilliseconds = 100;

const QString &releaseId()
{
    static const auto value = u"11111111-1111-4111-8111-111111111111"_s;
    return value;
}
const QString &groupId()
{
    static const auto value = u"22222222-2222-4222-8222-222222222222"_s;
    return value;
}
Album sampleAlbum()
{
    Album album;
    album.id = u"album-fixture"_s;
    album.title = u"Test Album"_s;
    album.artist = u"Test Artist"_s;
    album.year = 2001;
    album.genreId = u"manual"_s;
    album.online = { { u"releaseId"_s, releaseId() } };
    Track track;
    track.id = u"track-fixture"_s;
    track.albumId = album.id;
    track.title = u"Test Song"_s;
    track.artist = album.artist;
    track.duration = 200;
    track.trackNumber = 1;
    track.sourceUrl = QUrl::fromLocalFile(u"/private/fixture-song.wav"_s);
    album.tracks.append(track);
    Producer local;
    local.name = u"Local Person"_s;
    local.role = u"composer"_s;
    local.source = u"Local tags"_s;
    album.producers.append(local);
    return album;
}
QJsonObject relation(const QString &role, const QString &name)
{
    return { { u"type"_s, role },
             { u"target-credit"_s, u""_s },
             { u"target-type"_s, u"artist"_s },
             { u"artist"_s, QJsonObject{ { u"id"_s, releaseId() }, { u"name"_s, name } } } };
}
QJsonObject sampleRelease()
{
    const QJsonObject work{ { u"relations"_s,
                              QJsonArray{ relation(u"composer"_s, u"Composer"_s),
                                          relation(u"lyricist"_s, u"Lyricist"_s) } } };
    const QJsonObject recording{
        { u"title"_s, u"Test Song"_s },
        { u"relations"_s,
          QJsonArray{ relation(u"mix"_s, u"Mix Engineer"_s),
                      QJsonObject{ { u"type"_s, u"performance"_s }, { u"work"_s, work } } } }
    };
    const QJsonObject track{ { u"position"_s, 1 },
                             { u"title"_s, u"Test Song"_s },
                             { u"length"_s, 200000 },
                             { u"recording"_s, recording } };
    const QJsonObject medium{ { u"position"_s, 1 }, { u"tracks"_s, QJsonArray{ track } } };
    return { { u"id"_s, releaseId() },
             { u"title"_s, u"Test Album"_s },
             { u"date"_s, u"2001-01-01"_s },
             { u"artist-credit"_s, QJsonArray{ QJsonObject{ { u"name"_s, u"Test Artist"_s } } } },
             { u"release-group"_s, QJsonObject{ { u"id"_s, groupId() } } },
             { u"relations"_s, QJsonArray{ relation(u"producer"_s, u"Album Producer"_s) } },
             { u"media"_s, QJsonArray{ medium } } };
}
QJsonObject page()
{
    return { { u"pageid"_s, 1 },
             { u"title"_s, u"Test Album (album)"_s },
             { u"extract"_s, u"Test Album is an album by Test Artist, released in 2001."_s },
             { u"pageprops"_s, QJsonObject{ { u"wikibase_item"_s, u"Q1"_s } } },
             { u"revisions"_s, QJsonArray{ QJsonObject{ { u"revid"_s, 12345 } } } } };
}
QJsonObject claim(const QJsonObject &value)
{
    return { { u"mainsnak"_s,
               QJsonObject{ { u"datavalue"_s, QJsonObject{ { u"value"_s, value } } } } } };
}
QJsonObject sampleEntities()
{
    return {
        { u"Q1"_s,
          QJsonObject{ { u"claims"_s,
                         QJsonObject{ { u"P175"_s, QJsonArray{ claim({ { u"id"_s, u"Q2"_s } }) } },
                                      { u"P577"_s,
                                        QJsonArray{ claim(
                                                { { u"time"_s,
                                                    u"+2001-01-01T00:00:00Z"_s } }) } } } } } },
        { u"Q2"_s,
          QJsonObject{ { u"labels"_s,
                         QJsonObject{ { u"en"_s,
                                        QJsonObject{ { u"value"_s, u"Test Artist"_s } } } } } } }
    };
}
struct Response final
{
    QByteArray body;
    int status = 200, delay = 0;
    QByteArray retry;
};
struct Server final
{
    QMutex mutex;
    QList<QNetworkRequest> requests;
    QList<QNetworkProxy> proxies;
    std::function<Response(const QNetworkRequest &)> respond;
    int count()
    {
        QMutexLocker lock(&mutex);
        return int(requests.size());
    }
};
class Reply final : public QNetworkReply
{
    Q_DISABLE_COPY_MOVE(Reply)
public:
    Reply(const QNetworkRequest &request, Response response, QObject *parent)
        : QNetworkReply(parent), m_body(std::move(response.body))
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        if (!response.retry.isEmpty())
            setRawHeader("Retry-After"_ba, response.retry);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(response.delay, this, [this] {
            if (isFinished())
                return;
            Q_EMIT readyRead();
            if (!isFinished()) {
                setFinished(true);
                Q_EMIT finished();
            }
        });
    }
    void abort() override
    {
        if (isFinished())
            return;
        setError(QNetworkReply::OperationCanceledError, u"Cancelled"_s);
        setFinished(true);
        Q_EMIT finished();
    }
    qint64 bytesAvailable() const override
    {
        return m_body.size() - m_position + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const auto count = std::min(maximum, m_body.size() - m_position);
        if (count <= 0)
            return -1;
        std::memcpy(data, m_body.constData() + m_position, size_t(count));
        m_position += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_position = 0;
};
class Network final : public QNetworkAccessManager
{
    Q_DISABLE_COPY_MOVE(Network)
public:
    explicit Network(std::shared_ptr<Server> server) : m_server(std::move(server))
    {
    }

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        QMutexLocker lock(&m_server->mutex);
        m_server->requests.append(request);
        m_server->proxies.append(proxy());
        return new Reply(request, m_server->respond(request), this);
    }

private:
    std::shared_ptr<Server> m_server;
};
QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
AlbumQuery queryFor(const Album &album)
{
    AlbumQuery query;
    query.albumId = album.id;
    query.album = album;
    query.enabled = true;
    return query;
}
}
class OnlineMetadataTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OnlineMetadataTest)
public:
    OnlineMetadataTest() = default;

private Q_SLOTS:
    void workerCancellationRetainsRequestIdentity_data()
    {
        QTest::addColumn<MetadataKind>("kind");
        QTest::addColumn<bool>("active");
        for (const auto kind :
             { MetadataKind::Introduction, MetadataKind::Credits, MetadataKind::Enrichment }) {
            for (const bool active : { false, true })
                QTest::newRow(qPrintable(kindName(kind) + (active ? u"-active"_s : u"-waiting"_s)))
                        << kind << active;
        }
    }
    void workerCancellationRetainsRequestIdentity()
    {
        QFETCH(MetadataKind, kind);
        QFETCH(bool, active);
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &) {
            return Response{ json({ }), 200, 60000 };
        };
        MetadataWorker worker(new Network(server));
        QSignalSpy ready(&worker, &MetadataWorker::ready);
        const auto query = queryFor(sampleAlbum());
        const RequestToken token{ 17, 9, query.albumId };
        worker.enqueue(query, token, kind);
        if (active) {
            QTRY_VERIFY([&] {
                QMutexLocker lock(&server->mutex);
                return !server->requests.isEmpty();
            }());
        }
        worker.cancel(token);
        QCOMPARE(ready.size(), 1);
        const auto result = qvariant_cast<ProviderResult>(ready.first().first());
        QCOMPARE(result.requestId, token.requestId);
        QCOMPARE(result.generation, token.generation);
        QCOMPARE(result.albumId, token.albumId);
        QCOMPARE(result.kind, kind);
        QCOMPARE(result.status, ProviderStatus::Cancelled);
        worker.cancel(token);
        QCOMPARE(ready.size(), 1);
    }
    void cachePolicyUsesExpiryTransportAndCurrentProvenance()
    {
        AlbumQuery query;
        query.album = sampleAlbum();
        query.albumId = query.album.id;
        query.enabled = true;
        query.contact = u"fixture@example.invalid"_s;
        query.album.creditsLookup = { { u"sourceUrl"_s, u"https://old.example.invalid"_s } };
        const auto now = QDateTime::fromString(u"2026-10-22T00:00:00.000Z"_s, Qt::ISODateWithMs);
        const RequestToken token{ 7, 3, query.album.id };
        ProviderResult result;
        result.kind = MetadataKind::Credits;
        result.status = ProviderStatus::Matched;
        result.inputKey = metadataKey(query.album, result.kind);
        result.payload = musicBrainzMetadata(query.album, result.kind, sampleRelease()).payload;
        stampMetadata(result, query, releaseId(), now);
        const auto state = result.payload.value(u"creditsLookup"_s).toObject();
        QCOMPARE(state.value(u"expiresAt"_s).toString(), u"2026-11-21T00:00:00.000Z"_s);
        QVERIFY(!state.contains(u"sourceUrl"_s));
        query.album.creditsLookup = state;
        auto hit = reusableMetadata(query, token, result.kind, result.inputKey, now.addDays(29));
        QVERIFY(hit);
        QVERIFY(hit->fromCache);
        QCOMPARE(hit->requestId, quint64(7));
        QVERIFY(!reusableMetadata(query, token, result.kind, result.inputKey, now.addDays(30)));
        query.force = true;
        QVERIFY(!reusableMetadata(query, token, result.kind, result.inputKey, now));
        result.status = ProviderStatus::Error;
        result.retryAt.clear();
        stampMetadata(result, query, releaseId(), now);
        query.album.creditsLookup = result.payload.value(u"creditsLookup"_s).toObject();
        QVERIFY(reusableMetadata(query, token, result.kind, result.inputKey, now.addSecs(59)));
        QVERIFY(!reusableMetadata(query, token, result.kind, result.inputKey, now.addSecs(60)));
        query.contact = u"changed@example.invalid"_s;
        QVERIFY(!reusableMetadata(query, token, result.kind, result.inputKey, now));
        query.enabled = false;
        QVERIFY(reusableMetadata(query, token, result.kind, result.inputKey, now));
    }

    void failedReleaseAttemptsPreserveVerifiedIdentity()
    {
        AlbumQuery query;
        query.album = sampleAlbum();
        query.album.online = { };
        const auto now = QDateTime::fromString(u"2026-10-22T00:00:00.000Z"_s, Qt::ISODateWithMs);
        auto verified = musicBrainzMetadata(query.album, MetadataKind::Credits, sampleRelease());
        verified.albumId = query.album.id;
        stampMetadata(verified, query, releaseId(), now);
        QVERIFY(mergeMetadata(query.album, verified));
        QCOMPARE(preferredRelease(query), releaseId());
        const auto verifiedTime = query.album.creditsLookup[u"releaseVerifiedAt"_s];
        for (const auto kind : { MetadataKind::Credits, MetadataKind::Enrichment }) {
            for (const auto status : { ProviderStatus::Uncertain, ProviderStatus::Error }) {
                auto rejected = sampleRelease();
                rejected.insert(u"id"_s, groupId());
                rejected.insert(u"title"_s, u"Another Album"_s);
                auto result = musicBrainzMetadata(query.album, kind, rejected);
                QCOMPARE(result.status, ProviderStatus::Uncertain);
                result.status = status;
                result.albumId = query.album.id;
                query.releaseId = groupId();
                stampMetadata(result, query, groupId(), now.addSecs(10));
                QVERIFY(mergeMetadata(query.album, result));
                query.releaseId.clear();
                QCOMPARE(preferredRelease(query), releaseId());
                QCOMPARE(query.album.creditsLookup[u"releaseVerifiedAt"_s], verifiedTime);
                const auto state = metadataState(query.album, kind);
                QCOMPARE(state[u"lookupReleaseId"_s].toString(), groupId());
                QVERIFY(state[u"releaseId"_s].toString() != groupId());
            }
        }
        // A subsequent verified release supersedes the earlier one, even without credits.
        auto newer = sampleRelease();
        newer.insert(u"id"_s, groupId());
        newer.remove(u"relations"_s);
        auto result = musicBrainzMetadata(query.album, MetadataKind::Enrichment, newer);
        result.albumId = query.album.id;
        stampMetadata(result, query, groupId(), now.addSecs(20));
        QVERIFY(mergeMetadata(query.album, result));
        QCOMPARE(preferredRelease(query), groupId());
        ProviderResult failed;
        failed.albumId = query.album.id;
        failed.kind = MetadataKind::Credits;
        failed.status = ProviderStatus::Error;
        stampMetadata(failed, query, releaseId(), now.addSecs(30));
        QVERIFY(mergeMetadata(query.album, failed));
        QCOMPARE(preferredRelease(query), groupId());
        // Even a retained payload cannot turn an unsuccessful refresh into verification.
        result.kind = MetadataKind::Credits;
        result.status = ProviderStatus::Uncertain;
        stampMetadata(result, query, groupId(), now.addSecs(35));
        QCOMPARE(result.payload[u"creditsLookup"_s].toObject()[u"releaseId"_s].toString(),
                 releaseId());
        QCOMPARE(result.payload[u"creditsLookup"_s].toObject()[u"releaseVerifiedAt"_s],
                 verifiedTime);
        // Previously persisted unverified attempts must not steer new lookups either.
        query.album.creditsLookup = { { u"releaseId"_s, releaseId() },
                                      { u"status"_s, u"uncertain"_s },
                                      { u"checkedAt"_s,
                                        now.addSecs(40).toString(Qt::ISODateWithMs) } };
        QCOMPARE(preferredRelease(query), groupId());
    }
    void wikipediaTextRequiresPerformerAttribution_data()
    {
        QTest::addColumn<QString>("artist");
        QTest::addColumn<QString>("extract");
        QTest::addColumn<bool>("matches");
        QTest::newRow("substring")
                << u"Air"_s << u"Moon is an album by Chair, released in 1997."_s << false;
        QTest::newRow("longer-name")
                << u"Air"_s << u"Moon is an album by Airborne, released in 1997."_s << false;
        QTest::newRow("longer-performer")
                << u"Air"_s << u"Moon is an album by Air Supply, released in 1997."_s << false;
        QTest::newRow("mention-only")
                << u"Air"_s << u"Moon is an album by Chair, inspired by Air, released in 1997."_s
                << false;
        QTest::newRow("attribution")
                << u"Air"_s << u"Moon is an album by Air, released in 1997."_s << true;
        QTest::newRow("unicode-case")
                << u"Bj\u00f6rk"_s << u"Moon is an album by BJ\u00d6RK, released in 1997."_s
                << true;
        QTest::newRow("chinese-attribution")
                << u"\u738b\u83f2"_s
                << u"Moon\u662f\u7531\u738b\u83f2\u4e8e1997\u5e74\u53d1\u884c\u7684\u4e13\u8f91\u3002"_s
                << true;
        QTest::newRow("chinese-substring")
                << u"\u83f2"_s
                << u"Moon\u662f\u7531\u738b\u83f2\u4e8e1997\u5e74\u53d1\u884c\u7684\u4e13\u8f91\u3002"_s
                << false;
        QTest::newRow("punctuation")
                << u"AC/DC"_s << u"Moon is an album by AC/DC, released in 1997."_s << true;
    }
    void wikipediaTextRequiresPerformerAttribution()
    {
        QFETCH(QString, artist);
        QFETCH(QString, extract);
        QFETCH(bool, matches);
        Album album;
        album.title = u"Moon"_s;
        album.artist = artist;
        album.year = 1997;
        const QJsonObject candidate{ { u"title"_s, u"Moon (album)"_s }, { u"extract"_s, extract } };
        const auto result = wikipediaMetadata(album, QJsonArray{ candidate }, { }, u"en"_s);
        QCOMPARE(result.status, matches ? ProviderStatus::Matched : ProviderStatus::Uncertain);
        QCOMPARE(result.payload.isEmpty(), !matches);
    }
    void rejectedMetadataBatchDoesNotSaveOrNotify()
    {
        QTemporaryDir directory;
        LibrarySnapshot library;
        library.fields.insert(u"nativeSchemaVersion"_s, 1);
        library.albums.append(sampleAlbum());
        QFile cache(directory.filePath(u"library-index.json"_s));
        QVERIFY(cache.open(QIODevice::WriteOnly));
        const auto bytes = QJsonDocument(toJson(library)).toJson();
        QCOMPARE(cache.write(bytes), bytes.size());
        cache.close();
        LibraryStore store(directory.path());
        QSignalSpy loaded(&store, &LibraryStore::snapshotReady);
        store.load();
        QTRY_COMPARE(loaded.size(), 1);
        const auto original = store.snapshot();
        QSignalSpy committed(&store, &LibraryStore::metadataCommitted);
        QSignalSpy saved(&store, &LibraryStore::saved);
        ProviderResult result;
        result.albumId = library.albums.first().id;
        result.kind = MetadataKind::Introduction;
        result.status = ProviderStatus::Matched;
        result.checkedAt = u"2026-10-22T00:00:00Z"_s;
        result.inputKey = u"stale"_s;
        result.payload = { { u"introduction"_s, QJsonObject{ { u"status"_s, u"matched"_s } } } };
        store.stageMetadata(result);
        store.commitMetadata();
        QTRY_VERIFY(!store.busy());
        QCOMPARE(store.snapshot(), original);
        QCOMPARE(committed.size(), 0);
        QCOMPARE(saved.size(), 0);
        QVERIFY(cache.open(QIODevice::ReadOnly));
        QCOMPARE(cache.readAll(), bytes);
    }
    void metadataBatchRejectsStaleValuesWithoutCopyingSnapshot()
    {
        LibrarySnapshot library;
        library.albums.append(sampleAlbum());
        auto other = sampleAlbum();
        other.id = u"album-other"_s;
        other.fields.insert(u"extension"_s, 7);
        library.albums.append(other);
        const auto original = std::make_shared<const LibrarySnapshot>(library);
        ProviderResult introduction;
        introduction.albumId = library.albums.first().id;
        introduction.kind = MetadataKind::Introduction;
        introduction.status = ProviderStatus::Matched;
        introduction.inputKey = u"stale-key"_s;
        introduction.payload = { { u"introduction"_s,
                                   QJsonObject{ { u"status"_s, u"matched"_s } } },
                                 { u"description"_s, u"Accepted text"_s } };
        QHash<QString, ProviderResult> pending{ { u"introduction"_s, introduction } };
        QCOMPARE(mergeMetadataBatch(original, pending), original);
        introduction.inputKey = metadataKey(library.albums.first(), MetadataKind::Introduction);
        pending[u"introduction"_s] = introduction;
        auto credits = introduction;
        credits.kind = MetadataKind::Credits;
        credits.inputKey = metadataKey(library.albums.first(), MetadataKind::Credits);
        credits.payload = { { u"creditsLookup"_s, QJsonObject{ { u"status"_s, u"partial"_s } } } };
        credits.status = ProviderStatus::Partial;
        pending.insert(u"credits"_s, credits);
        auto missing = introduction;
        missing.albumId = u"missing"_s;
        pending.insert(u"missing"_s, missing);
        const auto updated = mergeMetadataBatch(original, pending);
        QVERIFY(updated != original);
        QCOMPARE(updated->albums.first().description, u"Accepted text"_s);
        QCOMPARE(updated->albums.first().creditsLookup.value(u"status"_s), u"partial"_s);
        QCOMPARE(updated->albums.first().producers.size(), 1);
        QCOMPARE(toJson(updated->albums.last()), toJson(other));
        QVERIFY(original->albums.first().description.isEmpty());
        QVERIFY(original->albums.first().creditsLookup.isEmpty());
        introduction.status = ProviderStatus::Cancelled;
        QCOMPARE(mergeMetadataBatch(original, { { u"cancelled"_s, introduction } }), original);
        introduction.status = ProviderStatus::Matched;
        introduction.payload = { };
        QCOMPARE(mergeMetadataBatch(original, { { u"empty"_s, introduction } }), original);
    }
    void repeatedTrackTitlesStillRequireUniqueMatches()
    {
        auto album = sampleAlbum();
        auto second = album.tracks.first();
        second.id = u"track-second"_s;
        second.trackNumber = 2;
        album.tracks.append(second);
        auto release = sampleRelease();
        auto medium = release.value(u"media"_s).toArray().first().toObject();
        const auto firstRemote = medium.value(u"tracks"_s).toArray().first().toObject();
        auto secondRemote = firstRemote;
        secondRemote.insert(u"position"_s, 2);
        // Distinct spelling normalizes to the same title bucket.
        secondRemote.insert(u"title"_s, u"TEST-SONG"_s);
        medium.insert(u"tracks"_s, QJsonArray{ firstRemote, secondRemote });
        release.insert(u"media"_s, QJsonArray{ medium });
        const auto matched = musicBrainzMetadata(album, MetadataKind::Credits, release);
        QCOMPARE(matched.status, ProviderStatus::Partial);
        QCOMPARE(matched.payload.value(u"creditsLookup"_s)
                         .toObject()
                         .value(u"matchedTracks"_s)
                         .toInt(),
                 2);
        album.tracks[0].trackNumber.reset();
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Uncertain);
        album.tracks[0].trackNumber = 2;
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Uncertain);
    }
    void repeatedTitlesMatchByDurationWithoutReusingRecordings()
    {
        auto album = sampleAlbum();
        auto release = sampleRelease();
        auto medium = release.value(u"media"_s).toArray().first().toObject();
        const auto original = medium.value(u"tracks"_s).toArray().first().toObject();
        album.tracks.clear();
        QJsonArray remoteTracks;
        // Reverse the duration order and omit local numbering: every title is identical,
        // but each duration admits exactly one recording. Check the actual credit mapping.
        for (int i = 0; i < 1800; ++i) {
            Track track;
            track.id = u"track-%1"_s.arg(i);
            track.albumId = album.id;
            track.title = u"Test Song"_s;
            track.duration = 20 + i * 8;
            album.tracks.append(track);
            auto remote = original;
            remote.insert(u"position"_s, 1800 - i);
            remote.insert(u"length"_s, (20 + (1799 - i) * 8) * 1000);
            remote.insert(u"recording"_s,
                          QJsonObject{ { u"relations"_s,
                                         QJsonArray{ relation(
                                                 u"mix"_s, u"Engineer-%1"_s.arg(1799 - i)) } } });
            remoteTracks.append(remote);
        }
        medium.insert(u"tracks"_s, remoteTracks);
        release.insert(u"media"_s, QJsonArray{ medium });
        const auto result = musicBrainzMetadata(album, MetadataKind::Credits, release);
        QCOMPARE(result.status, ProviderStatus::Partial);
        QCOMPARE(result.payload.value(u"creditsLookup"_s).toObject().value(u"matchedTracks"_s),
                 1800);
        const auto credits = result.payload.value(u"producers"_s).toArray();
        QCOMPARE(credits.size(), 1801);
        for (int i = 0; i < album.tracks.size(); ++i) {
            const auto credit = credits[i + 1].toObject();
            QCOMPARE(credit.value(u"trackId"_s), u"track-%1"_s.arg(i));
            QCOMPARE(credit.value(u"name"_s), u"Engineer-%1"_s.arg(i));
        }
        album.tracks[1].duration = album.tracks[0].duration;
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Uncertain);
    }
    void repeatedTitlesKeepAllOptionalProjectionsInSync()
    {
        auto album = sampleAlbum();
        auto release = sampleRelease();
        const auto originalMedium = release.value(u"media"_s).toArray().first().toObject();
        const auto originalTrack = originalMedium.value(u"tracks"_s).toArray().first().toObject();
        album.tracks.clear();
        QJsonArray media;
        for (int disc = 1; disc <= 2; ++disc) {
            auto medium = originalMedium;
            medium.insert(u"position"_s, disc);
            QJsonArray tracks;
            for (int position = 1; position <= 2; ++position) {
                auto remote = originalTrack;
                remote.insert(u"position"_s, position);
                remote.insert(u"length"_s, position == 1 ? 200000 : 210000);
                tracks.append(remote);
                Track local;
                local.id = u"%1-%2"_s.arg(disc).arg(position);
                local.title = u"Test Song"_s;
                local.discNumber = disc;
                local.trackNumber = position;
                local.duration = position == 1 ? 200 : 210;
                album.tracks.append(local);
            }
            medium.insert(u"tracks"_s, tracks);
            media.append(medium);
        }
        release.insert(u"media"_s, media);
        // Disc-only, position-only and fully unconstrained queries become unique as
        // preceding precise matches consume candidates through every other projection.
        album.tracks[1].trackNumber.reset();
        album.tracks[2].discNumber.reset();
        album.tracks[3].discNumber.reset();
        album.tracks[3].trackNumber.reset();
        album.tracks[3].duration = 0;
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Partial);
        // Without the first disc constraint, the first query is ambiguous immediately.
        album.tracks[0].discNumber.reset();
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Uncertain);
    }
    void durationBoundaryUsesTheOriginalSubtractionPredicate()
    {
        auto album = sampleAlbum();
        auto second = album.tracks.first();
        second.id = u"second"_s;
        second.trackNumber = 2;
        second.duration = 100;
        album.tracks[0].trackNumber.reset();
        album.tracks[0].duration = std::nextafter(3.0, 4.0);
        album.tracks.append(second);
        auto release = sampleRelease();
        auto medium = release.value(u"media"_s).toArray().first().toObject();
        auto firstRemote = medium.value(u"tracks"_s).toArray().first().toObject();
        auto secondRemote = firstRemote;
        firstRemote.insert(u"length"_s, std::ldexp(1., -52) * 1000);
        secondRemote.insert(u"position"_s, 2);
        secondRemote.insert(u"length"_s, 100000);
        medium.insert(u"tracks"_s, QJsonArray{ firstRemote, secondRemote });
        release.insert(u"media"_s, QJsonArray{ medium });
        // The original subtraction rounds to exactly three; a precomputed lower duration
        // bound rounds differently near zero and would exclude this valid candidate.
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Partial);
        firstRemote.insert(u"length"_s, std::ldexp(1., -54) * 1000);
        medium.insert(u"tracks"_s, QJsonArray{ firstRemote, secondRemote });
        release.insert(u"media"_s, QJsonArray{ medium });
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                 ProviderStatus::Uncertain);
    }
    void repeatedTitlesKeepUnknownRemoteDurationsAvailable()
    {
        for (const int unknownLength : { 0, -100000 }) {
            auto album = sampleAlbum();
            auto second = album.tracks.first();
            second.id = u"unknown-duration"_s;
            second.duration = 0;
            album.tracks[0].trackNumber.reset();
            album.tracks[0].duration = .5;
            album.tracks.append(second);
            auto release = sampleRelease();
            auto medium = release.value(u"media"_s).toArray().first().toObject();
            auto firstRemote = medium.value(u"tracks"_s).toArray().first().toObject();
            auto secondRemote = firstRemote;
            firstRemote.insert(u"length"_s, unknownLength);
            secondRemote.insert(u"position"_s, 2);
            secondRemote.insert(u"length"_s, 3500);
            medium.insert(u"tracks"_s, QJsonArray{ firstRemote, secondRemote });
            release.insert(u"media"_s, QJsonArray{ medium });
            QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, release).status,
                     ProviderStatus::Partial);
        }
    }
    void durationToleranceAndOptionalNumbering_data()
    {
        QTest::addColumn<double>("duration");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("unknown") << 0.0 << true;
        QTest::newRow("lower-inclusive") << 197.0 << true;
        QTest::newRow("upper-inclusive") << 203.0 << true;
        QTest::newRow("lower-outside") << std::nextafter(197.0, 0.0) << false;
        QTest::newRow("upper-outside") << std::nextafter(203.0, 300.0) << false;
    }
    void durationToleranceAndOptionalNumbering()
    {
        QFETCH(double, duration);
        QFETCH(bool, accepted);
        for (int mask = 0; mask < 4; ++mask) {
            auto album = sampleAlbum();
            auto &track = album.tracks[0];
            track.duration = duration;
            track.discNumber = mask & 1 ? std::optional(1) : std::nullopt;
            track.trackNumber = mask & 2 ? std::optional(1) : std::nullopt;
            const auto result = musicBrainzMetadata(album, MetadataKind::Credits, sampleRelease());
            QCOMPARE(result.status, accepted ? ProviderStatus::Partial : ProviderStatus::Uncertain);
        }
    }
    void creditsMatchTracksAndPreserveRoles()
    {
        auto album = sampleAlbum();
        const auto result = musicBrainzMetadata(album, MetadataKind::Credits, sampleRelease());
        QCOMPARE(result.status, ProviderStatus::Partial);
        const auto credits = result.payload[u"producers"_s].toArray();
        QCOMPARE(credits.size(), 4);
        QCOMPARE(credits[1].toObject()[u"role"_s].toString(), u"mix"_s);
        QCOMPARE(credits[2].toObject()[u"trackId"_s].toString(), album.tracks[0].id);
        album.tracks[0].duration = 250;
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, sampleRelease()).status,
                 ProviderStatus::Uncertain);
        album = sampleAlbum();
        album.tracks[0].title += u" (Live)"_s;
        QCOMPARE(musicBrainzMetadata(album, MetadataKind::Credits, sampleRelease()).status,
                 ProviderStatus::Uncertain);
        auto release = sampleRelease();
        release.insert(u"track-count"_s, 1);
        release.insert(u"score"_s, 100);
        QCOMPARE(musicBrainzRelease(sampleAlbum(), { { u"releases"_s, QJsonArray{ release } } }),
                 releaseId());
        auto other = release;
        other.insert(u"id"_s, groupId());
        QVERIFY(musicBrainzRelease(sampleAlbum(),
                                   { { u"releases"_s, QJsonArray{ release, other } } })
                        .isEmpty());
        album = sampleAlbum();
        album.tracks.clear();
        const auto albumOnly = musicBrainzMetadata(album, MetadataKind::Credits, sampleRelease());
        QCOMPARE(albumOnly.status, ProviderStatus::Partial);
        const auto albumCredits = albumOnly.payload.value(u"producers"_s).toArray();
        QCOMPARE(albumCredits.size(), 1);
        QCOMPARE(albumCredits.first().toObject().value(u"name"_s), u"Album Producer"_s);
    }
    void wikipediaChecksIdentityAndAttribution()
    {
        auto album = sampleAlbum();
        auto entities = sampleEntities();
        const auto result = wikipediaMetadata(album, QJsonArray{ page() }, entities, u"en"_s);
        QCOMPARE(result.status, ProviderStatus::Matched);
        QCOMPARE(result.source[u"revisionId"_s].toInt(), 12345);
        QVERIFY(result.source[u"licenseUrl"_s].toString().startsWith(
                u"https://creativecommons.org/"_s));
        album.year = 2002;
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ page() }, entities, u"en"_s).status,
                 ProviderStatus::Uncertain);
        album = sampleAlbum();
        auto artist = entities[u"Q2"_s].toObject();
        artist.insert(
                u"labels"_s,
                QJsonObject{ { u"en"_s, QJsonObject{ { u"value"_s, u"Another Artist"_s } } } });
        entities.insert(u"Q2"_s, artist);
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ page() }, entities, u"en"_s).status,
                 ProviderStatus::Uncertain);
        auto ambiguous = page();
        ambiguous.insert(u"pageprops"_s, QJsonObject{ { u"disambiguation"_s, u""_s } });
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ ambiguous }, { }, u"en"_s).status,
                 ProviderStatus::Uncertain);
        // A known structured contradiction cannot be rescued by matching article text.
        entities = sampleEntities();
        entities.remove(u"Q2"_s);
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ page() }, entities, u"en"_s).status,
                 ProviderStatus::Uncertain);
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ page() }, { }, u"en"_s).status,
                 ProviderStatus::Matched);
        album.year.reset();
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ page() }, { }, u"en"_s).status,
                 ProviderStatus::Uncertain);
        QCOMPARE(wikipediaMetadata(album, QJsonArray{ page() }, sampleEntities(), u"en"_s).status,
                 ProviderStatus::Matched);
    }
    void networkCreditsCacheAndCancellation()
    {
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &) {
            return Response{ json(sampleRelease()), 200, 50, { } };
        };
        OnlineMetadataProvider provider(nullptr, [server] {
            return new Network(server);
        });
        provider.configure({ { u"networkEnabled"_s, true } });
        QSignalSpy delivered(&provider, &IMetadataProvider::resultReady);
        auto album = sampleAlbum();
        QVERIFY(provider.requestCredits(queryFor(album), { 1, 1, album.id }));
        QTRY_COMPARE(delivered.size(), 1);
        auto result = qvariant_cast<ProviderResult>(delivered.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::Partial);
        QCOMPARE(server->count(), 1);
        QVERIFY(mergeMetadata(album, result));
        QCOMPARE(album.producers.first().name, u"Local Person"_s);
        QCOMPARE(album.producers.size(), 5);
        QVERIFY(provider.requestCredits(queryFor(album), { 2, 1, album.id }));
        QTRY_COMPARE(delivered.size(), 1);
        const auto cached = qvariant_cast<ProviderResult>(delivered.takeFirst()[0]);
        QVERIFY(cached.fromCache);
        QCOMPARE(cached.checkedAt, result.checkedAt);
        QCOMPARE(server->count(), 1);
        auto refresh = queryFor(album);
        refresh.force = true;
        QVERIFY(provider.requestCredits(refresh, { 3, 1, album.id }));
        provider.cancel({ 3, 1, album.id });
        QTest::qWait(QueuedEventObservationMilliseconds);
        QVERIFY(delivered.isEmpty());
        provider.configure({ { u"networkEnabled"_s, false } });
        QVERIFY(provider.requestCredits(queryFor(album), { 4, 1, album.id }));
        QTRY_COMPARE(delivered.size(), 1);
        QVERIFY(qvariant_cast<ProviderResult>(delivered.first()[0]).fromCache);
        QMutexLocker lock(&server->mutex);
        for (const auto &request : std::as_const(server->requests)) {
            QVERIFY(!request.url().toString().contains(u"private"_s));
            QVERIFY(!request.hasRawHeader("Cookie"_ba));
            QVERIFY(request.rawHeader("User-Agent"_ba).contains("yalahoo"_ba));
        }
    }

private:
    static auto releaseFixtureResponse(const QNetworkRequest &request)
    {
        if (request.url().path().endsWith(u"/release/"_s)) {
            auto first = sampleRelease();
            first.insert(u"score"_s, 100);
            first.insert(u"track-count"_s, 1);
            auto second = first;
            second.insert(u"id"_s, groupId());
            return Response{ json({ { u"count"_s, 2 },
                                    { u"releases"_s, QJsonArray{ first, second } } }),
                             200,
                             0,
                             { } };
        }
        if (request.url().path().contains(u"release-group"_s))
            return Response{
                json({ { u"id"_s, groupId() },
                       { u"genres"_s, QJsonArray{ QJsonObject{ { u"name"_s, u"rock"_s } } } } }),
                200,
                0,
                { }
            };
        auto release = sampleRelease();
        release.insert(u"id"_s, groupId());
        return Response{ json(release), 200, 0, { } };
    }
private Q_SLOTS:
    void releaseChoiceAndGenreEnrichment()
    {
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &request) {
            return releaseFixtureResponse(request);
        };
        OnlineMetadataProvider provider(nullptr, [server] {
            return new Network(server);
        });
        provider.configure({ { u"networkEnabled"_s, true } });
        auto album = sampleAlbum();
        album.online = { };
        LibrarySnapshot snapshot;
        snapshot.albums.append(album);
        LibrarySession session(&provider);
        session.setSnapshot(snapshot);
        QSignalSpy delivered(&session, &LibrarySession::metadataDelivered);
        QVERIFY(session.requestMetadata(album.id, u"credits"_s));
        QCOMPARE(session.metadataFor(album.id, u"credits"_s)[u"status"_s].toString(), u"Pending"_s);
        QTRY_COMPARE(delivered.size(), 1);
        const auto uncertain = qvariant_cast<ProviderResult>(delivered.takeFirst()[0]);
        QCOMPARE(uncertain.status, ProviderStatus::Uncertain);
        QCOMPARE(
                session.metadataFor(album.id, u"credits"_s)[u"releaseCandidates"_s].toList().size(),
                2);
        QVERIFY(session.requestMetadata(album.id, u"credits"_s, true, groupId()));
        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 1, BackendDeadlineMilliseconds);
        const auto matched = qvariant_cast<ProviderResult>(delivered.takeFirst()[0]);
        QCOMPARE(matched.status, ProviderStatus::Partial);
        QCOMPARE(session.resolvedAlbum(album.id).creditsLookup[u"releaseId"_s].toString(),
                 groupId());
        QVERIFY(session.metadataFor(album.id, u"credits"_s)[u"releaseCandidates"_s]
                        .toList()
                        .isEmpty());
        QVERIFY(session.requestMetadata(album.id, u"enrichment"_s));
        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 1, BackendDeadlineMilliseconds);
        const auto enriched = qvariant_cast<ProviderResult>(delivered.first()[0]);
        QCOMPARE(enriched.status, ProviderStatus::Matched);
        QCOMPARE(session.resolvedAlbum(album.id).genreId, u"manual"_s);
        QCOMPARE(session.resolvedAlbum(album.id)
                         .fields[u"_onlineGenres"_s]
                         .toArray()
                         .first()
                         .toString(),
                 u"rock"_s);
    }
    void failedRefreshPreservesContentAndUnknownFields()
    {
        auto album = sampleAlbum();
        album.description = u"Saved introduction"_s;
        album.introduction = { { u"future"_s, 17 } };
        ProviderResult failure;
        failure.albumId = album.id;
        failure.kind = MetadataKind::Introduction;
        failure.status = ProviderStatus::Error;
        failure.payload = { { u"introduction"_s,
                              QJsonObject{ { u"future"_s, 17 }, { u"status"_s, u"error"_s } } } };
        QVERIFY(mergeMetadata(album, failure));
        QCOMPARE(album.description, u"Saved introduction"_s);
        QCOMPARE(album.introduction[u"future"_s].toInt(), 17);
        failure.kind = MetadataKind::Credits;
        failure.payload = { { u"creditsLookup"_s, QJsonObject{ { u"status"_s, u"error"_s } } } };
        QVERIFY(mergeMetadata(album, failure));
        QCOMPARE(album.producers.size(), 1);
        QCOMPARE(album.producers.first().source, u"Local tags"_s);
    }
    void failedRefreshStillDisplaysSavedIntroduction()
    {
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &) {
            return Response{ "{}"_ba, 429, 0, "60"_ba };
        };
        OnlineMetadataProvider provider(nullptr, [server] {
            return new Network(server);
        });
        provider.configure({ { u"networkEnabled"_s, true } });
        auto album = sampleAlbum();
        album.description = u"Saved introduction"_s;
        LibrarySnapshot snapshot;
        snapshot.albums.append(album);
        LibrarySession session(&provider);
        session.setSnapshot(snapshot);
        QSignalSpy delivered(&session, &LibrarySession::metadataDelivered);
        QVERIFY(session.requestMetadata(album.id, u"introduction"_s, true));
        QTRY_COMPARE(delivered.size(), 1);
        const auto view = session.metadataFor(album.id, u"introduction"_s);
        QCOMPARE(view[u"status"_s].toString(), u"Error"_s);
        QCOMPARE(view[u"contentText"_s].toString(), u"Saved introduction"_s);
        QCOMPARE(server->count(), 1);
    }
    void changingProxyCanRetryConnectionErrors()
    {
        const auto server = std::make_shared<Server>();
        server->respond = [raw = server.get()](const QNetworkRequest &) {
            return raw->requests.size() == 1 ? Response{ "{}"_ba, 500, 0, { } }
                                             : Response{ json(sampleRelease()), 200, 0, { } };
        };
        OnlineMetadataProvider provider(nullptr, [server] {
            return new Network(server);
        });
        provider.configure({ { u"networkEnabled"_s, true } });
        auto album = sampleAlbum();
        QSignalSpy delivered(&provider, &IMetadataProvider::resultReady);
        QVERIFY(provider.requestCredits(queryFor(album), { 1, 1, album.id }));
        QTRY_COMPARE(delivered.size(), 1);
        const auto failed = qvariant_cast<ProviderResult>(delivered.takeFirst()[0]);
        QCOMPARE(failed.status, ProviderStatus::Error);
        QVERIFY(mergeMetadata(album, failed));
        QVERIFY(provider.requestCredits(queryFor(album), { 2, 1, album.id }));
        QTRY_COMPARE(delivered.size(), 1);
        QVERIFY(qvariant_cast<ProviderResult>(delivered.takeFirst()[0]).fromCache);
        QCOMPARE(server->count(), 1);
        auto corrected = queryFor(album);
        corrected.configuration.insert(u"proxyEnabled"_s, false);
        QVERIFY(provider.requestCredits(corrected, { 3, 1, album.id }));
        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 1, BackendDeadlineMilliseconds);
        QCOMPARE(qvariant_cast<ProviderResult>(delivered.first()[0]).status,
                 ProviderStatus::Partial);
        QCOMPARE(server->count(), 2);
    }
    void wikipediaPipeline()
    {
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &request) {
            if (request.url().host() == "www.wikidata.org"_L1)
                return Response{ json({ { u"entities"_s, sampleEntities() } }), 200, 0, { } };
            return Response{
                json({ { u"query"_s, QJsonObject{ { u"pages"_s, QJsonArray{ page() } } } } }),
                200,
                0,
                { }
            };
        };
        OnlineMetadataProvider provider(nullptr, [server] {
            return new Network(server);
        });
        provider.configure({ { u"networkEnabled"_s, true } });
        QSignalSpy delivered(&provider, &IMetadataProvider::resultReady);
        const auto album = sampleAlbum();
        QVERIFY(provider.requestIntroduction(queryFor(album), { 1, 1, album.id }));
        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 1, BackendDeadlineMilliseconds);
        const auto result = qvariant_cast<ProviderResult>(delivered.first()[0]);
        QCOMPARE(result.status, ProviderStatus::Matched);
        QCOMPARE(result.source[u"name"_s].toString(), u"Wikipedia"_s);
        QCOMPARE(server->count(), 2);
    }
    void httpFailureBoundaries_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("status");
        QTest::newRow("malformed") << "not json"_ba << 200;
        QTest::newRow("oversized") << QByteArray(2 * 1024 * 1024 + 1, 'x') << 200;
        QTest::newRow("redirect") << "{}"_ba << 302;
        QTest::newRow("rate-limit") << "{}"_ba << 429;
        QTest::newRow("maxlag") << "{\"error\":{\"code\":\"maxlag\"}}"_ba << 200;
    }
    void httpFailureBoundaries()
    {
        QFETCH(QByteArray, body);
        QFETCH(int, status);
        const auto server = std::make_shared<Server>();
        server->respond = [body, status](const QNetworkRequest &) {
            return Response{ body, status, 0, status == 429 ? "120"_ba : QByteArray() };
        };
        MetadataHttp http(nullptr, new Network(server));
        QList<MetadataResponse> responses;
        connect(&http, &MetadataHttp::completed, this, [&](const MetadataResponse &response) {
            responses.append(response);
        });
        const QUrl url(u"https://musicbrainz.org/ws/2/release/test"_s);
        http.get(url, { }, { });
        QTRY_COMPARE(responses.size(), 1);
        QVERIFY(!responses.first().error.isEmpty());
        QCOMPARE(server->count(), 1);
        if (status == 429 || body.contains("maxlag"_ba)) {
            QVERIFY(!responses.first().retryAt.isEmpty());
            http.get(url, { }, { });
            QTRY_COMPARE(responses.size(), 2);
            QCOMPARE(server->count(), 1);
        }
        http.get(QUrl(u"https://untrusted.invalid/"_s), { }, { });
        QTRY_VERIFY(responses.size() >= 2);
        QCOMPARE(server->count(), 1);
    }
    void proxySelection_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("url");
        QTest::addColumn<int>("expected");
        QTest::newRow("default-system")
                << true << u"system"_s << QString() << int(QNetworkProxy::DefaultProxy);
        QTest::newRow("system-ignores-stored-manual")
                << true << u"system"_s << u"http://127.0.0.1:7897"_s
                << int(QNetworkProxy::DefaultProxy);
        QTest::newRow("disabled-direct") << false << u"manual"_s << u"http://127.0.0.1:7897"_s
                                         << int(QNetworkProxy::NoProxy);
        QTest::newRow("manual") << true << u"manual"_s << u"http://127.0.0.1:7897"_s
                                << int(QNetworkProxy::HttpProxy);
        QTest::newRow("unknown-mode-fails")
                << true << u"unknown"_s << u"http://127.0.0.1:7897"_s << -1;
        QTest::newRow("manual-empty-fails") << true << u"manual"_s << QString() << -1;
    }
    void proxySelection()
    {
        QFETCH(bool, enabled);
        QFETCH(QString, mode);
        QFETCH(QString, url);
        QFETCH(int, expected);
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &) {
            return Response{ "{}"_ba, 200, 0, { } };
        };
        MetadataHttp http(nullptr, new Network(server));
        bool complete = false;
        MetadataResponse result;
        connect(&http, &MetadataHttp::completed, this, [&](const MetadataResponse &response) {
            complete = true;
            result = response;
        });
        const QJsonObject config{ { u"proxyEnabled"_s, enabled },
                                  { u"proxyMode"_s, mode },
                                  { u"proxyUrl"_s, url } };
        http.get(QUrl(u"https://musicbrainz.org/ws/2/release/test"_s), { }, config);
        QTRY_VERIFY(complete);
        if (expected < 0) {
            QCOMPARE(server->count(), 0);
            QVERIFY(!result.error.isEmpty());
        } else {
            QCOMPARE(server->count(), 1);
            QMutexLocker lock(&server->mutex);
            QCOMPARE(int(server->proxies.first().type()), expected);
            if (expected == QNetworkProxy::HttpProxy)
                QCOMPARE(server->proxies.first().port(), quint16(7897));
        }
    }
    void boundedQueueAndShutdown()
    {
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &) {
            return Response{ json(sampleRelease()), 200, 1000, { } };
        };
        auto provider = std::make_unique<OnlineMetadataProvider>(nullptr, [server] {
            return new Network(server);
        });
        provider->configure({ { u"networkEnabled"_s, true } });
        const auto album = sampleAlbum();
        for (quint64 id = 1; id <= 8; ++id)
            QVERIFY(provider->requestCredits(queryFor(album), { id, 1, album.id }));
        QVERIFY(!provider->requestCredits(queryFor(album), { 9, 1, album.id }));
        provider->cancel({ 5, 1, album.id });
        QTRY_VERIFY(provider->requestCredits(queryFor(album), { 9, 1, album.id }));
        provider.reset();
    }
    void batchPersistenceAndDetailRefresh()
    {
        QTemporaryDir directory;
        auto album = sampleAlbum();
        LibrarySnapshot initial;
        initial.albums.append(album);
        initial.genres.append({ u"manual"_s, u"Manual Genre"_s, { }, { }, { } });
        initial.fields.insert(u"metadataConfiguration"_s,
                              QJsonObject{ { u"networkEnabled"_s, true } });
        QFile file(directory.filePath(u"library-index.json"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        auto root = toJson(initial);
        root.insert(u"nativeSchemaVersion"_s, 1);
        file.write(json(root));
        file.close();
        LibraryStore store(directory.path());
        store.load();
        QTRY_VERIFY(!store.busy());
        const auto server = std::make_shared<Server>();
        server->respond = [](const QNetworkRequest &) {
            return Response{ json(sampleRelease()), 200, 0, { } };
        };
        OnlineMetadataProvider provider(nullptr, [server] {
            return new Network(server);
        });
        provider.configure({ { u"networkEnabled"_s, true } });
        LibrarySession session(&provider);
        session.setSnapshot(*store.snapshot());
        QVERIFY(session.navigate(album.id));
        QVERIFY(session.setPlaybackIds(album.id, album.tracks[0].id));
        DetailModel detail;
        detail.prepare(session);
        connect(&session, &LibrarySession::albumMetadataChanged, &detail, [&](const QString &) {
            detail.prepare(session, true);
        });
        connect(&session, &LibrarySession::metadataDelivered, &store, &LibraryStore::stageMetadata);
        connect(&session, &LibrarySession::metadataProgressChanged, &store, [&] {
            if (!session.metadataProgress()[u"active"_s].toBool())
                store.commitMetadata();
        });
        connect(&store, &LibraryStore::metadataCommitted, &session,
                &LibrarySession::adoptMetadataSnapshot);
        connect(&store, &LibraryStore::metadataConfigurationChanged, &session,
                &LibrarySession::adoptMetadataConfiguration);
        QSignalSpy reset(session.albums(), &QAbstractItemModel::modelReset);
        QSignalSpy detailReset(&detail, &QAbstractItemModel::modelReset);
        QSignalSpy saved(&store, &LibraryStore::saved);
        QVERIFY(session.requestMetadataBatch(u"credits"_s));
        QTRY_VERIFY(!session.metadataProgress()[u"active"_s].toBool());
        QTRY_VERIFY(!saved.isEmpty());
        QTRY_VERIFY(!store.busy());
        QVERIFY(reset.isEmpty());
        QVERIFY(detailReset.isEmpty());
        QCOMPARE(session.navigationAlbumId(), album.id);
        QCOMPARE(session.playbackTrackId(), album.tracks[0].id);
        QCOMPARE(session.resolvedAlbum(album.id).producers.size(), 5);
        const auto groups = detail.album()[u"producerGroups"_s].toList();
        QCOMPARE(groups.size(), 5);
        QCOMPARE(groups.first().toMap().value(u"name"_s), u"Local Person"_s);
        QCOMPARE(groups.last().toMap().value(u"tracks"_s).toStringList().size(), 1);
        QCOMPARE(store.snapshot()->albums[0].genreId, u"manual"_s);
        QVERIFY(store.setMetadataConfiguration({ { u"proxyEnabled"_s, false } }));
        QTRY_VERIFY(!store.busy());
        QVERIFY(reset.isEmpty());
        QCOMPARE(session.resolvedAlbum(album.id).producers.size(), 5);
        QCOMPARE(session.navigationAlbumId(), album.id);
        QVERIFY(!session.metadataConfiguration()[u"proxyEnabled"_s].toBool());
        LibraryStore restored(directory.path());
        restored.load();
        QTRY_VERIFY(!restored.busy());
        QCOMPARE(restored.snapshot()->albums[0].producers.size(), 5);
        QCOMPARE(restored.snapshot()->albums[0].tracks[0].sourceUrl, album.tracks[0].sourceUrl);
        QVERIFY(!restored.setMetadataConfiguration(
                { { u"proxyUrl"_s, u"http://user:pass@localhost"_s } }));
        QVERIFY(!restored.setMetadataConfiguration(
                { { u"musicBrainzContact"_s, u"bad\r\nheader"_s } }));
        QVERIFY(!restored.setMetadataConfiguration({ { u"proxyMode"_s, u"invalid"_s } }));
        QVERIFY(restored.setMetadataConfiguration(
                { { u"proxyEnabled"_s, true },
                  { u"proxyMode"_s, u"system"_s },
                  { u"proxyUrl"_s, u"http://127.0.0.1:7897"_s } }));
    }
};
QTEST_GUILESS_MAIN(OnlineMetadataTest)
#include "OnlineMetadataTest.moc"
