// Validates typed library parsing and atomic persistence while retaining unknown document
// fields. Explicit fixture literals are independent inputs or expected results; timing bounds
// have named constants.

#include "Localization.h"
#include "LibrarySearchOracle.h"
#include "LibrarySession.h"
#include <QScopeGuard>
#include "LibraryIndex.h"
#include "GenreRowsModel.h"
#include "OptionMetrics.h"
#include <QFontMetricsF>
#include "MetadataPresentation.h"
#include <limits>
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
#include <functional>
#include <utility>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using yalahoo::test::searchLibrary;
class TestProvider final : public IMetadataProvider
{
    Q_DISABLE_COPY_MOVE(TestProvider)
public:
    TestProvider() = default;
    using IMetadataProvider::IMetadataProvider;
    QList<RequestToken> requests, cancelled;
    QList<MetadataKind> kinds;
    QList<AlbumQuery> queries;
    std::function<void()> onCancel;
    ProviderCapabilities capabilities() const override
    {
        return { };
    }
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind) override
    {
        queries.append(query);
        requests.append(token);
        kinds.append(kind);
        return true;
    }
    void cancel(RequestToken token) override
    {
        cancelled.append(token);
        if (onCancel)
            onCancel();
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
        result.payload = { { u"test"_s, true } };
        Q_EMIT resultReady(result);
    }
};
class LibraryDataTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LibraryDataTest)
public:
    LibraryDataTest() = default;

private:
    yalahoo::Localization localization{ QLocale(QLocale::Chinese, QLocale::China) };
    QJsonObject fixtures, golden;
    LibrarySnapshot library(const QString &name = u"mixed"_s) const
    {
        LibrarySnapshot result;
        if (!readLibrary(fixtures.value(name).toObject(), result))
            qFatal("Invalid frozen fixture");
        return result;
    }
private Q_SLOTS:
    void metadataQueuedDuringResetCannotRevertLibrary()
    {
        for (bool queueThird : { false, true }) {
            AlbumListModel model;
            QAbstractItemModelTester tester(&model,
                                            QAbstractItemModelTester::FailureReportingMode::QtTest);
            LibrarySnapshot a, b, c;
            Album album;
            album.id = u"A"_s;
            a.albums.append(album);
            album.id = u"B"_s;
            b.albums.append(album);
            album.id = u"C"_s;
            c.albums.append(album);
            auto enriched = a;
            enriched.albums[0].description = u"stale"_s;
            model.setSnapshot(std::make_shared<const LibrarySnapshot>(a));
            bool once = false;
            QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
            QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
            connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] {
                if (std::exchange(once, true))
                    return;
                if (queueThird)
                    model.setSnapshot(std::make_shared<const LibrarySnapshot>(c));
                model.updateMetadataSnapshot(std::make_shared<const LibrarySnapshot>(enriched));
            });
            model.setSnapshot(std::make_shared<const LibrarySnapshot>(b));
            QCOMPARE(model.snapshot()->albums[0].id, queueThird ? u"C"_s : u"B"_s);
            QCOMPARE(resets.size(), queueThird ? 2 : 1);
            QCOMPARE(changes.size(), 0);
        }
        AlbumListModel model;
        LibrarySnapshot b;
        Album album;
        album.id = u"B"_s;
        b.albums.append(album);
        auto enriched = b;
        enriched.albums[0].description = u"current"_s;
        connect(&model, &QAbstractItemModel::modelReset, &model, [&] {
            model.updateMetadataSnapshot(std::make_shared<const LibrarySnapshot>(enriched));
        });
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        model.setSnapshot(std::make_shared<const LibrarySnapshot>(b));
        QCOMPARE(resets.size(), 1);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(model.snapshot()->albums[0].description, u"current"_s);
    }
    void trackIdsAreUniqueWithinTheirAlbum()
    {
        LibrarySnapshot library;
        for (const auto &id : { u"A"_s, u"B"_s }) {
            Album album;
            album.id = id;
            Track track;
            track.id = u"shared"_s;
            track.albumId = id;
            album.tracks.append(track);
            library.albums.append(album);
        }
        LibrarySnapshot parsed;
        const auto encoded = toJson(library);
        QVERIFY(readLibrary(encoded, parsed));
        QCOMPARE(toJson(parsed), encoded);
        library.albums[0].tracks.append(library.albums[0].tracks[0]);
        QVERIFY(!readLibrary(toJson(library), parsed));
    }
    void resetsDrainLatestSnapshotAndSortWithoutNesting()
    {
        AlbumListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        LibrarySnapshot initial, replacement;
        for (int i = 0; i < 3; ++i) {
            Album album;
            album.id = QString::number(i);
            album.title = QString::number(i);
            album.artist = QString::number(2 - i);
            initial.albums.append(album);
        }
        replacement = initial;
        replacement.albums.removeLast();
        const auto latest = std::make_shared<const LibrarySnapshot>(replacement);
        model.setSnapshot(std::make_shared<const LibrarySnapshot>(initial));
        int depth = 0, maximumDepth = 0;
        bool once = false;
        connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] {
            maximumDepth = std::max(maximumDepth, ++depth);
            if (!std::exchange(once, true)) {
                model.setSnapshot(latest);
                model.setSortMode(SortMode::Artist);
            }
        });
        connect(&model, &QAbstractItemModel::modelReset, &model, [&] {
            --depth;
        });
        model.setSortMode(SortMode::Album);
        QCOMPARE(maximumDepth, 1);
        QCOMPARE(depth, 0);
        QCOMPARE(model.snapshot(), latest);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), AlbumListModel::AlbumId).toString(), u"1"_s);
    }

    void metadataNotificationStopsAfterReentrantSort()
    {
        LibrarySnapshot data;
        for (int row = 0; row < 3; ++row) {
            Album album;
            album.id = QString::number(row);
            album.title = album.id;
            album.artist = QString::number(2 - row);
            data.albums.append(album);
        }
        AlbumListModel model;
        model.setSortMode(SortMode::Album);
        model.setSnapshot(std::make_shared<const LibrarySnapshot>(data));
        for (auto &album : data.albums)
            album.description = u"Updated metadata"_s;
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        connect(&model, &QAbstractItemModel::dataChanged, &model, [&] {
            model.setSortMode(SortMode::Artist);
        });
        model.updateMetadataSnapshot(std::make_shared<const LibrarySnapshot>(data));
        QCOMPARE(reset.size(), 1);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(model.data(model.index(0), AlbumListModel::AlbumId).toString(), u"2"_s);
        for (int row = 0; row < model.rowCount(); ++row)
            QCOMPARE(model.data(model.index(row), AlbumListModel::Description).toString(),
                     u"Updated metadata"_s);
    }
    void typedPayloadsDoNotRetainObsoleteJsonCopies()
    {
        LibrarySnapshot source;
        const QJsonObject extension{ { u"future"_s,
                                       QJsonObject{ { u"keep"_s, QJsonValue::Null } } } };
        source.fields = extension;
        source.scan = { { u"status"_s, u"done"_s } };
        source.enrich = source.introductions = source.credits = source.scan;
        Genre genre;
        genre.id = u"genre"_s;
        genre.name = u"Genre"_s;
        genre.aliases = { u"Alias"_s };
        genre.fields = extension;
        source.genres.append(genre);
        Album album;
        album.id = u"album"_s;
        album.genreId = genre.id;
        album.title = u"Collection"_s;
        album.description = QString(65536, u'x');
        album.localNote = u"Local note"_s;
        album.descriptionSource = album.introduction = album.creditsLookup = album.online =
                QJsonObject{ { u"payload"_s, QString(8192, u'y') } };
        album.rawGenres = { u"Raw genre"_s };
        album.fields = extension;
        Track track;
        track.id = u"track"_s;
        track.albumId = album.id;
        track.title = u"Song"_s;
        track.fields = extension;
        album.tracks.append(track);
        Producer producer;
        producer.name = u"Person"_s;
        producer.role = u"Writer"_s;
        producer.fields = extension;
        album.producers.append(producer);
        source.albums.append(album);
        const auto encoded = toJson(source);
        LibrarySnapshot parsed;
        QVERIFY(readLibrary(encoded, parsed));
        QCOMPARE(toJson(parsed), encoded);
        QCOMPARE(parsed.fields, extension);
        QCOMPARE(parsed.genres.first().fields, extension);
        QCOMPARE(parsed.albums.first().fields, extension);
        QCOMPARE(parsed.albums.first().tracks.first().fields, extension);
        QCOMPARE(parsed.albums.first().producers.first().fields, extension);
        auto &updated = parsed.albums.first();
        updated.description = u"Refreshed"_s;
        updated.introduction = { { u"status"_s, u"matched"_s } };
        const auto refreshed = toJson(parsed).value(u"albums"_s).toArray().first().toObject();
        QCOMPARE(refreshed.value(u"description"_s).toString(), u"Refreshed"_s);
        QCOMPARE(refreshed.value(u"introduction"_s).toObject(), updated.introduction);
        QCOMPARE(refreshed.value(u"future"_s), extension.value(u"future"_s));
        QCOMPARE(encoded.value(u"albums"_s)
                         .toArray()
                         .first()
                         .toObject()
                         .value(u"description"_s)
                         .toString(),
                 album.description);
    }
    void boundedPrefixesKeepCompleteGraphemes()
    {
        const QString prefix(255, u'a');
        QCOMPARE(graphemePrefix(prefix + u"\U0001f3b5"_s, 256), prefix);
        QCOMPARE(graphemePrefix(prefix + u"e\u0301"_s, 256), prefix);
        QCOMPARE(graphemePrefix(u"\U0001f468\u200d\U0001f469\u200d\U0001f467"_s, 4), QString());
        QCOMPARE(graphemePrefix(u"AB"_s + QString(10000, u'\u0301'), 256), u"A"_s);
        QCOMPARE(graphemePrefix(u"\U0001f3b5"_s, 2), u"\U0001f3b5"_s);
        QCOMPARE(graphemePrefix(u"title"_s, 0), QString());
    }
    void malformedKnownFieldsPreserveSnapshot()
    {
        const auto original = toJson(library(u"single"_s));
        auto output = library(u"mixed"_s);
        const auto previous = toJson(output);
        const auto rejected = [&](const QJsonObject &input) {
            QString error;
            QVERIFY(!readLibrary(input, output, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(toJson(output), previous);
        };
        for (const auto key :
             { "scan"_L1, "enrich"_L1, "introductions"_L1, "credits"_L1, "onlineEnabled"_L1 }) {
            auto input = original;
            input.insert(key, 17);
            rejected(input);
        }
        for (const auto key :
             { "title"_L1, "artist"_L1, "descriptionSource"_L1, "introduction"_L1,
               "creditsLookup"_L1, "online"_L1, "offline"_L1, "rawGenres"_L1, "producers"_L1 }) {
            auto input = original;
            auto albums = input.value("albums"_L1).toArray();
            auto album = albums.first().toObject();
            album.insert(key, 17);
            albums[0] = album;
            input.insert("albums"_L1, albums);
            rejected(input);
        }
        for (const auto key : { "title"_L1, "sourceUrl"_L1, "mediaSupport"_L1, "lossless"_L1 }) {
            auto input = original;
            auto albums = input.value("albums"_L1).toArray();
            auto album = albums.first().toObject();
            auto tracks = album.value("tracks"_L1).toArray();
            auto track = tracks.first().toObject();
            track.insert(key, 17);
            tracks[0] = track;
            album.insert("tracks"_L1, tracks);
            albums[0] = album;
            input.insert("albums"_L1, albums);
            rejected(input);
        }
        for (const auto &malformed :
             { QJsonValue(17), QJsonValue(QJsonArray{ 17 }),
               QJsonValue(QJsonArray{ QJsonObject{ { u"name"_s, 17 } } }) }) {
            auto input = original;
            auto albums = input.value("albums"_L1).toArray();
            auto album = albums.first().toObject();
            album.insert("producers"_L1, malformed);
            albums[0] = album;
            input.insert("albums"_L1, albums);
            rejected(input);
        }
        auto input = original;
        input.insert("roots"_L1, QJsonArray{ 17 });
        rejected(input);
        input = original;
        input.insert("genres"_L1,
                     QJsonArray{ QJsonObject{ { u"id"_s, u"genre"_s },
                                              { u"aliases"_s, QJsonArray{ 17 } } } });
        rejected(input);
    }
    void missingArtistIsPresentationRatherThanStoredMetadata()
    {
        LibrarySession session(nullptr);
        auto snapshot = library(u"single"_s);
        auto &album = snapshot.albums.first();
        album.artist.clear();
        album.tracks.first().artist.clear();
        snapshot.genres.clear();
        Genre genre;
        genre.id = album.genreId;
        snapshot.genres.append(genre);
        const auto albumId = album.id;
        session.setSnapshot(snapshot);
        QVERIFY(session.navigate(albumId));
        QCOMPARE(session.albums()
                         ->data(session.albums()->index(0), AlbumListModel::Artist)
                         .toString(),
                 artistDisplayName({ }));
        QCOMPARE(session.browseInfo().value(u"artist"_s).toString(), artistDisplayName({ }));
        QVERIFY(session.snapshot()->albums.first().artist.isEmpty());
        QVERIFY(session.snapshot()->genres.first().name.isEmpty());
        QCOMPARE(session.genres().first().toMap().value("name"_L1).toString(),
                 genreDisplayName(genre));
        QCOMPARE(session.albums()->columns().first().toMap().value("label"_L1).toString(),
                 genreDisplayName(genre));
        QVERIFY(toJson(*session.snapshot())
                        .value(u"albums"_s)
                        .toArray()
                        .first()
                        .toObject()
                        .value(u"artist"_s)
                        .toString()
                        .isEmpty());
        QCOMPARE(artistDisplayName(u"Unknown artist"_s), u"Unknown artist"_s);
    }
    void optionWidthsFollowDataRoleAndFont()
    {
        OptionMetrics metrics;
        QFont font;
        font.setPixelSize(11);
        metrics.setFont(font);
        metrics.setTextRole(u"label"_s);
        metrics.setModel({ QVariantMap{ { u"label"_s, u"Short"_s },
                                        { u"alternate"_s, u"A much longer label"_s } },
                           QVariantMap{ { u"label"_s, u"Unicode \U0001f3b5"_s } } });
        QCOMPARE(metrics.maximumWidth(),
                 qMax(QFontMetricsF(font).horizontalAdvance(u"Short"_s),
                      QFontMetricsF(font).horizontalAdvance(u"Unicode \U0001f3b5"_s)));
        metrics.setTextRole(u"alternate"_s);
        QCOMPARE(metrics.maximumWidth(),
                 QFontMetricsF(font).horizontalAdvance(u"A much longer label"_s));
        QSignalSpy changes(&metrics, &OptionMetrics::maximumWidthChanged);
        metrics.setModel(metrics.model());
        QCOMPARE(changes.size(), 0);
        font.setPixelSize(22);
        metrics.setFont(font);
        QCOMPARE(metrics.maximumWidth(),
                 QFontMetricsF(font).horizontalAdvance(u"A much longer label"_s));
        metrics.setModel({ u"Direct string label"_s });
        QCOMPARE(metrics.maximumWidth(),
                 QFontMetricsF(font).horizontalAdvance(u"Direct string label"_s));
        metrics.setModel({ });
        QCOMPARE(metrics.maximumWidth(), 0.);
    }
    void genreRowsPreserveIdentityAcrossWrapping()
    {
        GenreRowsModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QFont font;
        font.setPixelSize(10);
        model.setFont(font);
        QVariantList genres;
        for (int i = 0; i < 2000; ++i)
            genres.append(QVariantMap{ { u"id"_s, QString::number(i) },
                                       { u"name"_s, u"\u6d41\u6d3e %1 \U0001f3b5"_s.arg(i) } });
        model.setGenres(genres);
        const int role = model.roleNames().key("entries"_ba);
        for (const double width : { 0., 120., 400., 930. }) {
            model.setWidth(width);
            QStringList ids;
            for (int row = 0; row < model.rowCount(); ++row)
                for (const auto &value : model.data(model.index(row), role).toList()) {
                    const auto entry = value.toMap();
                    ids.append(entry.value(u"id"_s).toString());
                    QVERIFY(entry.value(u"size"_s).toDouble() <= width);
                    QVERIFY(entry.value(u"size"_s).toDouble() >= 0);
                }
            QCOMPARE(ids.size(), 2001);
            QCOMPARE(ids.first(), QString());
            for (int i = 0; i < 2000; ++i)
                QCOMPARE(ids[i + 1], QString::number(i));
        }
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        model.setGenres(genres);
        model.setWidth(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(resets.size(), 0);
        model.setGenres({ });
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), role).toList().size(), 1);
    }
    void genreRowsDrainReentrantChanges()
    {
        GenreRowsModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setGenres(
                { QVariantMap{ { u"id"_s, u"old"_s }, { u"name"_s, QString(100, u'W') } } });
        model.setWidth(400);
        int depth = 0, maximumDepth = 0;
        bool replaced = false;
        const QVariantList latest{ QVariantMap{ { u"id"_s, u"new"_s },
                                                { u"name"_s, QString(100, u'W') } } };
        QFont latestFont;
        latestFont.setPixelSize(24);
        connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] {
            maximumDepth = std::max(maximumDepth, ++depth);
            if (!std::exchange(replaced, true)) {
                model.setWidth(930);
                model.setGenres(latest);
                model.setFont(latestFont);
            }
        });
        connect(&model, &QAbstractItemModel::modelReset, &model, [&] {
            --depth;
        });
        model.setWidth(120);
        QCOMPARE(maximumDepth, 1);
        QCOMPARE(depth, 0);
        QCOMPARE(model.width(), 930);
        QCOMPARE(model.genres(), latest);
        QCOMPARE(model.font(), latestFont);
        GenreRowsModel reference;
        reference.setWidth(930);
        reference.setGenres(latest);
        reference.setFont(latestFont);
        QCOMPARE(model.rowCount(), reference.rowCount());
        const auto role = model.roleNames().key("entries"_ba);
        for (int row = 0; row < model.rowCount(); ++row)
            QCOMPARE(model.data(model.index(row), role),
                     reference.data(reference.index(row), role));
    }
    void sessionDrainsReentrantReplacements_data()
    {
        QTest::addColumn<bool>("empty");
        QTest::addColumn<bool>("aboutToReset");
        for (bool empty : { false, true })
            for (bool aboutToReset : { false, true })
                QTest::newRow(qPrintable(u"%1-%2"_s.arg(empty).arg(aboutToReset)))
                        << empty << aboutToReset;
    }
    void sessionDrainsReentrantReplacements()
    {
        QFETCH(bool, empty);
        QFETCH(bool, aboutToReset);
        LibrarySession session(nullptr);
        QAbstractItemModelTester tester(session.albums(),
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto initial = library(u"single"_s);
        auto latest = empty ? LibrarySnapshot{ } : initial;
        if (!empty) {
            latest.albums[0].id = u"replacement"_s;
            for (auto &track : latest.albums[0].tracks)
                track.albumId = u"replacement"_s;
        }
        bool replaced = false;
        int depth = 0, maximumDepth = 0, publications = 0;
        const auto replace = [&] {
            if (!std::exchange(replaced, true))
                session.setSnapshot(latest);
        };
        connect(session.albums(), &QAbstractItemModel::modelAboutToBeReset, &session, [&] {
            maximumDepth = std::max(maximumDepth, ++depth);
            if (aboutToReset)
                replace();
        });
        connect(session.albums(), &QAbstractItemModel::modelReset, &session, [&] {
            --depth;
            if (!aboutToReset)
                replace();
        });
        connect(&session, &LibrarySession::snapshotChanged, &session, [&] {
            ++publications;
            QCOMPARE(session.snapshot(), session.albums()->snapshot());
        });
        session.setSnapshot(initial);
        QCOMPARE(depth, 0);
        QCOMPARE(maximumDepth, 1);
        QCOMPARE(publications, 2);
        QCOMPARE(session.snapshot(), session.albums()->snapshot());
        QCOMPARE(session.albums()->rowCount(), empty ? 0 : 1);
        if (!empty) {
            QVERIFY(session.navigate(u"replacement"_s, { }));
            QCOMPARE(session.browseInfo()[u"code"_s].toInt(), 1);
            QCOMPARE(session.browseInfo()[u"albumId"_s].toString(), u"replacement"_s);
        }
    }
    void metadataDisplayPreservesProvenanceAndState()
    {
        const QVariantMap source{ { u"name"_s, u"Local"_s },
                                  { u"url"_s, u"https://example.invalid/credit"_s } };
        const QVariantMap payload{ { u"producers"_s,
                                     QVariantList{
                                             QVariantMap{ { u"name"_s, u"\u5236\u4f5c\u4eba"_s },
                                                          { u"role"_s, u"\u5f55\u97f3"_s },
                                                          { u"source"_s, source } } } } };
        const QVariantMap cached{ { u"kind"_s, u"credits"_s },
                                  { u"status"_s, u"Uncertain"_s },
                                  { u"payload"_s, payload },
                                  { u"source"_s, source },
                                  { u"checkedAt"_s, u"2026-10-17"_s },
                                  { u"retryAt"_s, u"tomorrow"_s },
                                  { u"error"_s, u"\u539f\u9519\u8bef"_s } };
        const auto view = metadataPresentation(cached, u"\u4e13\u8f91"_s);
        QCOMPARE(view.value(u"payload"_s).toMap(), payload);
        QCOMPARE(view.value(u"source"_s).toMap(), source);
        QCOMPARE(view.value(u"status"_s).toString(), u"Uncertain"_s);
        QVERIFY(view.value(u"stateText"_s).toString().contains(metadataStatusText(u"Uncertain"_s)));
        QVERIFY(metadataStatusText(u"Uncertain"_s) != u"Uncertain"_s);
        QCOMPARE(metadataStatusText(u"Future state"_s), u"Future state"_s);
        QVERIFY(view.value(u"contentText"_s)
                        .toString()
                        .contains(u"\u5236\u4f5c\u4eba \u00b7 \u5f55\u97f3"_s));
        QVERIFY(view.value(u"contentText"_s)
                        .toString()
                        .contains(u"https://example.invalid/credit"_s));
        QVERIFY(view.value(u"stateText"_s).toString().contains(u"\u539f\u9519\u8bef"_s));
        QVERIFY(view.value(u"stateText"_s).toString().contains(u"tomorrow"_s));
        QVERIFY(view.value(u"sourceText"_s).toString().contains(u"Local"_s));
        const auto empty = metadataPresentation(
                { { u"kind"_s, u"introduction"_s }, { u"status"_s, u"Deferred"_s } },
                u"\u4e13\u8f91"_s);
        QCOMPARE(empty.value(u"contentText"_s).toString(),
                 u"\u6682\u65e0\u4e13\u8f91\u4ecb\u7ecd"_s);
        QVERIFY(!empty.value(u"stateText"_s).toString().contains(u"\u68c0\u67e5\u65f6\u95f4"_s));
    }
    void metadataConfigurationUsesNativeQueryBoundary()
    {
        TestProvider provider;
        LibrarySession session(&provider);
        auto snapshot = library(u"mixed"_s);
        snapshot.fields.insert(
                u"metadataConfiguration"_s,
                QJsonObject{ { u"musicBrainzContact"_s, u"project@example.invalid"_s },
                             { u"futureProviderOption"_s, 7 } });
        session.setSnapshot(snapshot);
        const auto id = snapshot.albums.first().id;
        QVERIFY(session.requestMetadata(id, u"introduction"_s) > 0);
        QCOMPARE(provider.queries.size(), 1);
        const auto &query = provider.queries.first();
        QCOMPARE(query.contact, u"project@example.invalid"_s);
        QCOMPARE(query.configuration[u"futureProviderOption"_s].toInt(), 7);
        QVERIFY(!query.enabled);
        QVERIFY(!session.providerCapabilities()[u"networkEnabled"_s].toBool());
    }
    void browseSnapshotReplacementPreservesPlayback()
    {
        OfflineMetadataProvider provider;
        LibrarySession session(&provider);
        auto snapshot = library(u"mixed"_s);
        QVERIFY(!snapshot.albums.isEmpty());
        const auto id = snapshot.albums.first().id;
        session.setSnapshot(snapshot);
        QVERIFY(session.navigate(id));
        QSignalSpy changes(&session, &LibrarySession::browseInfoChanged);
        snapshot.albums.first().title = u"\u66ff\u6362\u540e\u7684\u6536\u85cf\u6807\u9898"_s;
        session.setSnapshot(snapshot);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(session.browseInfo().value(u"title"_s).toString(), snapshot.albums.first().title);
        QCOMPARE(session.navigationAlbumId(), id);
        QVERIFY(session.playbackTrackId().isEmpty());
        session.search(u"\u5176\u4ed6\u6b4c\u66f2"_s);
        QCOMPARE(changes.size(), 1);
        session.setSnapshot({ });
        QVERIFY(session.browseInfo().isEmpty());
        QVERIFY(session.navigationAlbumId().isEmpty());
        QVERIFY(session.playbackTrackId().isEmpty());
    }
    void initTestCase()
    {
        QFile input((QString::fromUtf8(FIXTURE_ROOT)
                     + u"/tests/fixtures/web-baseline/anonymous.json"_s));
        QVERIFY(input.open(QIODevice::ReadOnly));
        fixtures =
                QJsonDocument::fromJson(input.readAll()).object().value(u"libraries"_s).toObject();
        QFile order((QString::fromUtf8(FIXTURE_ROOT) + u"/docs/contracts/library-order.json"_s));
        QVERIFY(order.open(QIODevice::ReadOnly));
        golden = QJsonDocument::fromJson(order.readAll()).object().value(u"libraries"_s).toObject();
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
            for (const auto &id : expected.value(u"albumIds"_s).toArray())
                expectedIds.append(id.toString());
            QCOMPARE(ids, expectedIds);
            QCOMPARE(order.slotStride, expected.value(u"slotStride"_s).toInt());
            const auto columns = expected.value(u"columns"_s).toArray();
            QCOMPARE(order.columns.size(), columns.size());
            for (int i = 0; i < columns.size(); ++i) {
                QCOMPARE(order.columns[i].label,
                         columns[i].toObject().value(u"label"_s).toString());
                QStringList expectedColumn;
                for (const auto &id : columns[i].toObject().value(u"albumIds"_s).toArray())
                    expectedColumn.append(id.toString());
                QCOMPARE(order.columns[i].albumIds, expectedColumn);
            }
        }
    }
    void normalizedSortingAndGrouping()
    {
        auto snapshot = library(u"odd"_s);
        snapshot.albums[0].title = u" Album 10 "_s;
        snapshot.albums[0].artist = u"\uff21  Artist"_s;
        snapshot.albums[1].title = u"\uff21lbum 2"_s;
        snapshot.albums[1].artist = u"a Artist"_s;
        snapshot.albums[2].title = u"Album 1"_s;
        snapshot.albums[2].artist = u"B"_s;
        const auto ordered = orderLibrary(snapshot, SortMode::Artist);
        QCOMPARE(ordered.indices, QList<int>({ 1, 0, 2 }));
        QCOMPARE(ordered.columns.size(), 2);
        QCOMPARE(ordered.columns[0].albumIds,
                 QStringList({ snapshot.albums[1].id, snapshot.albums[0].id }));
        QCOMPARE(orderLibrary(snapshot, SortMode::Album).indices, QList<int>({ 2, 1, 0 }));
        QCOMPARE(snapshot.albums[0].title, u" Album 10 "_s);
        snapshot.albums[0].title = snapshot.albums[1].title;
        QCOMPARE(orderLibrary(snapshot, SortMode::Artist).indices.first(), 0);
        auto mixed = library();
        mixed.genres[1].name = mixed.genres[0].name;
        QCOMPARE(orderLibrary(mixed, SortMode::Genre).columns.size(), 3);
        auto albums = orderLibrary(mixed, SortMode::Album);
        QCOMPARE(albums.columns.size(), 2);
        QCOMPARE(albums.columns[0].label, u"01\u201312"_s);
        QCOMPARE(albums.columns[1].label, u"13\u201321"_s);
        // Equivalent locale keys stay contiguous through normalized-string fallback.
        snapshot.albums[0].artist = u"\u00e9"_s;
        snapshot.albums[1].artist = u"e"_s;
        snapshot.albums[2].artist = u"\u00e9"_s;
        QCOMPARE(orderLibrary(snapshot, SortMode::Artist).columns.size(), 2);
    }
    void largeDistinctColumns()
    {
        LibrarySnapshot snapshot;
        for (int i = 1023; i >= 0; --i) {
            Album album;
            album.id = u"album-%1"_s.arg(i);
            album.title = u" Album %1 "_s.arg(i);
            album.artist = u" Artist %1 "_s.arg(i);
            album.genreId = u"genre-%1"_s.arg(i % 128);
            snapshot.albums.append(album);
        }
        snapshot.genres.append({ u"genre-7"_s, u"Known genre"_s, { }, { }, { } });
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
                QCOMPARE(order.columns.first().key, u"genre-7"_s);
                QCOMPARE(order.columns.first().label, u"Known genre"_s);
                QCOMPARE(order.columns[1].key, u"genre-127"_s);
                QCOMPARE(order.indices.first(), 0);
            } else {
                QCOMPARE(snapshot.albums[order.indices.first()].id, u"album-0"_s);
                QCOMPARE(snapshot.albums[order.indices.last()].id, u"album-1023"_s);
                QCOMPARE(order.columns.size(), mode == SortMode::Artist ? 1024 : 86);
            }
        }
    }
    void searchRowsFollowSnapshot()
    {
        LibrarySnapshot snapshot;
        for (int i = 0; i < 2; ++i) {
            Album album;
            album.id = u"album-%1"_s.arg(i);
            album.title = u"Collection %1"_s.arg(i);
            Track track;
            track.id = u"same-local-id"_s;
            track.albumId = album.id;
            track.title = u"Song %1"_s.arg(i);
            track.artist = u"Artist %1"_s.arg(i);
            album.tracks.append(track);
            snapshot.albums.append(album);
        }
        SearchListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto first = std::make_shared<const LibrarySnapshot>(snapshot);
        model.update(first, u"song"_s, { });
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), SearchListModel::Title), QVariant(u"Song 0"_s));
        QCOMPARE(model.data(model.index(1), SearchListModel::Title), QVariant(u"Song 1"_s));
        QCOMPARE(model.data(model.index(1), SearchListModel::Artist), QVariant(u"Artist 1"_s));
        QCOMPARE(model.data(model.index(1), SearchListModel::Kind), QVariant(u"track"_s));
        model.update(first, u"collection"_s, { });
        QCOMPARE(model.data(model.index(1), SearchListModel::Title), QVariant(u"Collection 1"_s));
        QCOMPARE(model.data(model.index(1), SearchListModel::TrackId), QVariant(QString()));
        snapshot.albums[1].tracks[0].title = u"Song replacement"_s;
        model.update(std::make_shared<const LibrarySnapshot>(snapshot), u"song"_s, { });
        QCOMPARE(model.data(model.index(1), SearchListModel::Title),
                 QVariant(u"Song replacement"_s));
        model.update({ }, QString(), { });
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.data(model.index(0), SearchListModel::Title).isValid());
    }
    void genreSearchSkipsExcludedAlbumTracks()
    {
        LibrarySnapshot snapshot;
        for (int i = 0; i < 4; ++i) {
            Album album;
            album.id = u"album-%1"_s.arg(i);
            album.title = u"Match"_s;
            album.genreId = i % 2 ? u"included"_s : u"excluded"_s;
            for (int j = 0; j < 64; ++j) {
                Track track;
                track.id = u"track-%1-%2"_s.arg(i).arg(j);
                track.albumId = album.id;
                track.title = u"Match"_s;
                album.tracks.append(track);
            }
            snapshot.albums.append(album);
        }
        SearchListModel model;
        const auto owner = std::make_shared<const LibrarySnapshot>(snapshot);
        for (const auto &query : { u"match"_s, QString() }) {
            model.update(owner, query, u"included"_s);
            const auto expected = searchLibrary(snapshot, query, u"included"_s);
            QCOMPARE(model.rowCount(), expected.size());
            for (int row = 0; row < expected.size(); ++row) {
                QCOMPARE(model.resultAt(row).value(u"albumId"_s).toString(), expected[row].albumId);
                QCOMPARE(model.resultAt(row).value(u"trackId"_s).toString(), expected[row].trackId);
            }
        }
        model.request(u"match"_s, u"missing"_s);
        QTRY_VERIFY(!model.searching());
        QCOMPARE(model.rowCount(), 0);
    }
    void incrementalSearchNotifications()
    {
        LibrarySnapshot snapshot;
        for (int i = 0; i < 1024; ++i) {
            Album album;
            album.id = u"album-%1"_s.arg(i);
            album.title = u"Collection %1"_s.arg(i);
            Track track;
            track.id = "track"_L1;
            track.albumId = album.id;
            track.title = u"Song %1"_s.arg(i);
            album.tracks.append(track);
            snapshot.albums.append(album);
        }
        SearchListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        const auto data = std::make_shared<const LibrarySnapshot>(snapshot);
        model.update(data, u""_s, u""_s);
        reset.clear();
        QPersistentModelIndex first(model.index(0));
        model.update(data, u"collection"_s, u""_s);
        QCOMPARE(first.data(SearchListModel::AlbumId).toString(), u"album-0"_s);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        model.update(data, u"song"_s, u""_s);
        QVERIFY(first.isValid());
        QCOMPARE(first.data(SearchListModel::TrackId).toString(), u"track"_s);
        QCOMPARE(removed.size(), 0);
        for (const auto &query :
             { "s", "so", "son", "song", "song 1", "song 10", "", "missing", "" }) {
            model.update(data, QString::fromUtf8(query), u""_s);
            const auto expected = searchLibrary(snapshot, QString::fromUtf8(query), u""_s);
            QCOMPARE(model.rowCount(), expected.size());
            for (int row = 0; row < model.rowCount(); ++row) {
                const auto actual = model.resultAt(row);
                QCOMPARE(actual.value(u"albumId"_s).toString(), expected[row].albumId);
                QCOMPARE(actual.value(u"trackId"_s).toString(), expected[row].trackId);
            }
        }
        QCOMPARE(reset.size(), 0);
        QCOMPARE(model.rowCount(), 1024);
    }
    void asynchronousSearchKeepsLatestQuery()
    {
        LibrarySnapshot snapshot;
        Album album;
        album.id = u"first"_s;
        album.title = u"Collection"_s;
        for (int i = 0; i < 1024; ++i) {
            Track track;
            track.id = u"track-%1"_s.arg(i);
            track.albumId = album.id;
            track.title = u"Song %1"_s.arg(i);
            album.tracks.append(track);
        }
        snapshot.albums.append(album);
        SearchListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto data = std::make_shared<const LibrarySnapshot>(snapshot);
        model.update(data, { }, { });
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.request(u"song"_s, { });
        QVERIFY(model.searching());
        model.request(u"missing"_s, { });
        model.request(u"song 1023"_s, { });
        QTRY_VERIFY(!model.searching());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.resultAt(0).value(u"trackId"_s).toString(), u"track-1023"_s);
        QCOMPARE(reset.size(), 0);
        model.request(u"song"_s, { });
        model.update({ }, { }, { });
        QVERIFY(!model.searching());
        QCOMPARE(model.rowCount(), 0);
        snapshot.albums[0].id = u"replacement"_s;
        snapshot.albums[0].tracks.clear();
        model.update(std::make_shared<const LibrarySnapshot>(snapshot), { }, { });
        model.request(u"collection"_s, { });
        QTRY_VERIFY(!model.searching());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.resultAt(0).value(u"albumId"_s).toString(), u"replacement"_s);
    }
    void searchObserversCanReplaceSnapshot_data()
    {
        QTest::addColumn<QString>("notification");
        QTest::addColumn<int>("replacementCount");
        QTest::newRow("changed") << u"changed"_s << 8;
        QTest::newRow("removed") << u"removed"_s << 8;
        QTest::newRow("inserted") << u"inserted"_s << 8;
        QTest::newRow("about-to-remove") << u"about-to-remove"_s << 8;
        QTest::newRow("about-to-insert") << u"about-to-insert"_s << 8;
        QTest::newRow("about-to-reset") << u"about-to-reset"_s << 8;
        QTest::newRow("about-to-remove-empty") << u"about-to-remove"_s << 0;
        QTest::newRow("about-to-insert-empty") << u"about-to-insert"_s << 0;
        QTest::newRow("about-to-reset-empty") << u"about-to-reset"_s << 0;
    }
    void searchObserversCanReplaceSnapshot()
    {
        QFETCH(QString, notification);
        QFETCH(int, replacementCount);
        LibrarySnapshot original, replacement;
        for (int i = 0; i < 8; ++i) {
            Album album;
            album.id = u"album-%1"_s.arg(i);
            Track track;
            track.id = u"track-%1"_s.arg(i);
            track.albumId = album.id;
            track.title = u"Song %1"_s.arg(i);
            album.tracks.append(track);
            replacement.albums.append(album);
            if (i < 3)
                original.albums.append(album);
        }
        replacement.albums.resize(replacementCount);
        const auto first = std::make_shared<const LibrarySnapshot>(original);
        const auto second = std::make_shared<const LibrarySnapshot>(replacement);
        SearchListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        const bool inserting =
                notification == "inserted"_L1 || notification == "about-to-insert"_L1;
        model.update(first, inserting ? u"song 2"_s : QString(), { });
        bool replaced = false;
        const auto replace = [&] {
            if (replaced)
                return;
            replaced = true;
            // A burst of reentrant updates keeps only its final intent.
            model.update({ }, { }, { });
            model.update(second, { }, { });
        };
        if (notification == "changed"_L1)
            connect(&model, &QAbstractItemModel::dataChanged, &model, replace);
        else if (notification == "removed"_L1)
            connect(&model, &QAbstractItemModel::rowsRemoved, &model, replace);
        else if (notification == "inserted"_L1)
            connect(&model, &QAbstractItemModel::rowsInserted, &model, replace);
        else if (notification == "about-to-remove"_L1)
            connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model, replace);
        else if (notification == "about-to-insert"_L1)
            connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model, replace);
        else
            connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, replace);
        model.update(notification == "about-to-reset"_L1 ? Snapshot() : first,
                     inserting ? u"song"_s : u"song 2"_s, { });
        QVERIFY(replaced);
        QCOMPARE(model.rowCount(), replacementCount);
        for (int row = 0; row < replacementCount; ++row) {
            QCOMPARE(model.resultAt(row).value(u"albumId"_s).toString(), u"album-%1"_s.arg(row));
            QVERIFY(model.resultAt(row).value(u"trackId"_s).toString().isEmpty());
        }
    }
    void searchReentrantQueryRetainsThePendingReplacementSnapshot()
    {
        LibrarySnapshot original, replacement;
        for (int i = 0; i < 3; ++i) {
            Album album;
            album.id = u"old-%1"_s.arg(i);
            original.albums.append(album);
        }
        Album fresh;
        fresh.id = u"new"_s;
        fresh.title = u"Needle"_s;
        replacement.albums.append(fresh);
        SearchListModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.update(std::make_shared<const LibrarySnapshot>(original), { }, { });
        bool replaced = false;
        connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model, [&] {
            if (replaced)
                return;
            replaced = true;
            model.update(std::make_shared<const LibrarySnapshot>(replacement), { }, { });
            model.request(u"needle"_s, { });
        });
        // Use the same snapshot to trigger an incremental removal, rather than a reset.
        const auto owner = std::make_shared<const LibrarySnapshot>(original);
        model.update(owner, { }, { });
        model.update(owner, u"missing"_s, { });
        QVERIFY(replaced);
        QTRY_VERIFY(!model.searching());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.resultAt(0).value(u"albumId"_s).toString(), u"new"_s);
    }
    void searchRowsObserverCanRequestLatestQuery()
    {
        LibrarySnapshot snapshot;
        Album album;
        album.id = u"album"_s;
        for (int i = 0; i < 3; ++i) {
            Track track;
            track.id = u"track-%1"_s.arg(i);
            track.title = u"Song %1"_s.arg(i);
            album.tracks.append(track);
        }
        snapshot.albums.append(album);
        const auto owner = std::make_shared<const LibrarySnapshot>(snapshot);
        SearchListModel model;
        model.update(owner, u"song"_s, { });
        QList<int> counts;
        connect(&model, &SearchListModel::countChanged, &model, [&] {
            counts.append(model.rowCount());
        });
        connect(&model, &SearchListModel::rowsRemoved, &model, [&] {
            model.request(u"song"_s, { });
        });
        model.update(owner, u"song 2"_s, { });
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(counts, QList<int>{ 1 });
        QTRY_VERIFY(!model.searching());
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(counts, (QList<int>{ 1, 3 }));
    }
    void frozenSearch()
    {
        QFile file(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/core/search-golden.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases =
                QJsonDocument::fromJson(file.readAll()).object().value(u"cases"_s).toArray();
        QCOMPARE(cases.size(), 216);
        QSet<QString> coveredLibraries;
        for (const auto &entry : cases)
            coveredLibraries.insert(entry.toObject()[u"library"_s].toString());
        QCOMPARE(coveredLibraries,
                 (QSet<QString>{ u"empty"_s, u"single"_s, u"mixed"_s, u"large"_s, u"odd"_s,
                                 u"even"_s }));
        SearchListModel model;
        for (const auto &entry : cases) {
            const auto object = entry.toObject();
            const auto actual = searchLibrary(library(object.value(u"library"_s).toString()),
                                              object.value(u"query"_s).toString(),
                                              object.value(u"genreId"_s).toString());
            QJsonArray ids;
            for (const auto &hit : actual)
                ids.append(QJsonObject{ { u"albumId"_s, hit.albumId },
                                        { u"trackId"_s, hit.trackId } });
            QCOMPARE(ids, object.value(u"results"_s).toArray());
            model.update(std::make_shared<const LibrarySnapshot>(
                                 library(object.value(u"library"_s).toString())),
                         object.value(u"query"_s).toString(),
                         object.value(u"genreId"_s).toString());
            QJsonArray indexed;
            for (int row = 0; row < model.rowCount(); ++row)
                indexed.append(QJsonObject::fromVariantMap(model.resultAt(row)));
            QCOMPARE(indexed, ids);
        }
    }
    void rejectsInvalidNumericMetadataWithoutReplacingSnapshot()
    {
        const auto base = fixtures.value(u"single"_s).toObject();
        const auto rejected = [&](const QJsonObject &input) {
            auto snapshot = library(u"single"_s);
            const auto previous = toJson(snapshot);
            QString error;
            QVERIFY(!readLibrary(input, snapshot, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(toJson(snapshot), previous);
        };
        const QList<QJsonValue> invalid{ 1.5, double(std::numeric_limits<int>::max()) + 1,
                                         double(std::numeric_limits<int>::min()) - 1, u"24"_s,
                                         true };
        for (const auto &value : invalid) {
            for (const auto &key : { u"year"_s, u"discCount"_s }) {
                auto input = base;
                auto album = input.value(u"albums"_s).toArray().first().toObject();
                album[key] = value;
                input[u"albums"_s] = QJsonArray{ album };
                rejected(input);
            }
            auto input = base;
            auto genre = input.value(u"genres"_s).toArray().first().toObject();
            genre[u"albumCount"_s] = value;
            input[u"genres"_s] = QJsonArray{ genre };
            rejected(input);
            for (const auto &key : { u"trackNumber"_s, u"discNumber"_s, u"bitsPerSample"_s,
                                     u"sampleRate"_s, u"bitrate"_s }) {
                input = base;
                auto album = input.value(u"albums"_s).toArray().first().toObject();
                auto track = album.value(u"tracks"_s).toArray().first().toObject();
                track[key] = value;
                album[u"tracks"_s] = QJsonArray{ track };
                input[u"albums"_s] = QJsonArray{ album };
                rejected(input);
            }
        }
        for (const auto &value : QList<QJsonValue>{ -1, u"120"_s, true }) {
            auto input = base;
            auto album = input.value(u"albums"_s).toArray().first().toObject();
            auto track = album.value(u"tracks"_s).toArray().first().toObject();
            track[u"duration"_s] = value;
            album[u"tracks"_s] = QJsonArray{ track };
            input[u"albums"_s] = QJsonArray{ album };
            rejected(input);
        }
    }
    void integerMetadataDomainBoundaries()
    {
        const auto base = fixtures.value(u"single"_s).toObject();
        const auto check = [&](const QString &key, bool trackField, bool genreField, int minimum) {
            for (const auto &value :
                 QList<QJsonValue>{ QJsonValue(QJsonValue::Null), minimum,
                                    std::numeric_limits<int>::max(), minimum - 1 }) {
                auto input = base;
                if (genreField) {
                    auto genre = input[u"genres"_s].toArray().first().toObject();
                    genre[key] = value;
                    input[u"genres"_s] = QJsonArray{ genre };
                } else {
                    auto album = input[u"albums"_s].toArray().first().toObject();
                    if (trackField) {
                        auto track = album[u"tracks"_s].toArray().first().toObject();
                        track[key] = value;
                        album[u"tracks"_s] = QJsonArray{ track };
                    } else {
                        album[key] = value;
                    }
                    input[u"albums"_s] = QJsonArray{ album };
                }
                auto snapshot = library(u"single"_s);
                const auto previous = toJson(snapshot);
                QString error;
                if (value.isNull() || value.toInt() >= minimum) {
                    QVERIFY2(readLibrary(input, snapshot, &error), qPrintable(error));
                } else {
                    QVERIFY(!readLibrary(input, snapshot, &error));
                    QVERIFY(!error.isEmpty());
                    QCOMPARE(toJson(snapshot), previous);
                }
            }
        };
        check(u"albumCount"_s, false, true, 0);
        check(u"discCount"_s, false, false, 0);
        for (const auto &key : { u"trackNumber"_s, u"discNumber"_s, u"bitsPerSample"_s,
                                 u"sampleRate"_s, u"bitrate"_s })
            check(key, true, false, 1);
    }
    void invalidLocalUrlsDoNotBecomeMediaSources()
    {
        for (const auto &url : { u"file:relative.flac"_s, u"file:///tmp/song.flac?download"_s,
                                 u"file:///tmp/song.flac#fragment"_s, u"file:///tmp/song\0.flac"_s,
                                 u"file:///tmp/song%00.flac"_s }) {
            auto input = fixtures.value(u"single"_s).toObject();
            auto album = input.value(u"albums"_s).toArray().first().toObject();
            auto track = album.value(u"tracks"_s).toArray().first().toObject();
            track[u"sourceUrl"_s] = url;
            album[u"folderUrl"_s] = url;
            album[u"coverUrl"_s] = url;
            input[u"roots"_s] = QJsonArray{ QJsonObject{ { u"path"_s, url } } };
            album[u"tracks"_s] = QJsonArray{ track };
            input[u"albums"_s] = QJsonArray{ album };
            LibrarySnapshot snapshot;
            QVERIFY(readLibrary(input, snapshot));
            QVERIFY(snapshot.albums.first().tracks.first().sourceUrl.isEmpty());
            QVERIFY(snapshot.albums.first().folder.isEmpty());
            QVERIFY(snapshot.albums.first().coverUrl.isEmpty());
            QVERIFY(snapshot.roots.first().path.isEmpty());
        }
    }
    void roundTripAndLocalUrls()
    {
        auto input = fixtures.value(u"single"_s).toObject();
        auto album = input.value(u"albums"_s).toArray().first().toObject();
        album.insert(u"customAlbum"_s, QJsonObject{ { u"nested"_s, 42 } });
        album.insert(u"folder"_s, QDir::tempPath() + u"/\u97f3\u4e50 \u76ee\u5f55"_s);
        album.insert(u"descriptionSource"_s,
                     QJsonObject{ { u"name"_s, u"manual"_s },
                                  { u"url"_s, u"https://example.invalid/info"_s },
                                  { u"license"_s, u"test"_s },
                                  { u"custom"_s, true } });
        album.insert(u"introduction"_s,
                     QJsonObject{ { u"status"_s, u"uncertain"_s },
                                  { u"candidateUrl"_s, u"https://example.invalid/candidate"_s } });
        album.insert(u"creditsLookup"_s,
                     QJsonObject{ { u"status"_s, u"partial"_s },
                                  { u"source"_s, u"QQ Music"_s },
                                  { u"matchedTracks"_s, 1 },
                                  { u"totalTracks"_s, 3 } });
        album.insert(u"online"_s,
                     QJsonObject{ { u"status"_s, u"matched"_s },
                                  { u"releaseId"_s, u"release"_s },
                                  { u"descriptionStatus"_s, u"available"_s } });
        auto tracks = album.value(u"tracks"_s).toArray();
        auto track = tracks.first().toObject();
        track.insert(u"customTrack"_s, 7);
        track.insert(
                u"sourceUrl"_s,
                QUrl::fromLocalFile(QDir::tempPath() + u"/\u97f3\u4e50 # 1.FLAC"_s).toString());
        track.insert(u"bitsPerSample"_s, 24);
        track.insert(u"lossless"_s, true);
        tracks[0] = track;
        album.insert(u"tracks"_s, tracks);
        input.insert(u"albums"_s, QJsonArray{ album });
        input.insert(u"customLibrary"_s, true);
        LibrarySnapshot snapshot;
        QVERIFY(readLibrary(input, snapshot));
        QVERIFY(snapshot.albums.first().folder.isLocalFile());
        const auto &loaded = snapshot.albums.first().tracks.first();
        QVERIFY(loaded.sourceUrl.isLocalFile());
        QVERIFY(loaded.sourceUrl.toLocalFile().endsWith(u"\u97f3\u4e50 # 1.FLAC"_s));
        QCOMPARE(loaded.mediaSupport, MediaSupport::Unknown);
        QCOMPARE(loaded.bitsPerSample, std::optional<int>(24));
        const auto saved = toJson(snapshot);
        QCOMPARE(saved.value(u"customLibrary"_s), input.value(u"customLibrary"_s));
        auto savedAlbum = saved.value(u"albums"_s).toArray().first().toObject();
        QCOMPARE(savedAlbum.value(u"customAlbum"_s), album.value(u"customAlbum"_s));
        for (const auto &key :
             { u"descriptionSource"_s, u"introduction"_s, u"creditsLookup"_s, u"online"_s })
            QCOMPARE(savedAlbum.value(key), album.value(key));
        QCOMPARE(savedAlbum.value(u"tracks"_s).toArray().first().toObject().value(u"customTrack"_s),
                 QJsonValue(7));
        LibrarySnapshot again;
        QVERIFY(readLibrary(saved, again));
        QCOMPARE(toJson(again), saved);
        track.insert(u"sourceUrl"_s, u"https://example.invalid/song.flac"_s);
        tracks[0] = track;
        album.insert(u"tracks"_s, tracks);
        input.insert(u"albums"_s, QJsonArray{ album });
        QVERIFY(readLibrary(input, snapshot));
        QVERIFY(snapshot.albums.first().tracks.first().sourceUrl.isEmpty());
        input.insert(u"albums"_s, QJsonArray{ album, album });
        QString error;
        const auto previous = toJson(snapshot);
        QVERIFY(!readLibrary(input, snapshot, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(toJson(snapshot), previous);
        album.insert(u"tracks"_s, true);
        input.insert(u"albums"_s, QJsonArray{ album });
        QVERIFY(!readLibrary(input, snapshot, &error));
        QCOMPARE(toJson(snapshot), previous);
    }
    void searchFilterSurvivesViewAbsenceAndLibraryReplacement()
    {
        OfflineMetadataProvider provider;
        LibrarySession session(&provider);
        auto snapshot = library(u"single"_s);
        const auto oldGenre = snapshot.albums.first().genreId;
        QVERIFY(!oldGenre.isEmpty());
        session.setSnapshot(snapshot);
        QSignalSpy changes(&session, &LibrarySession::searchChanged);
        session.search(u"Typography"_s, oldGenre);
        QTRY_VERIFY(!session.searchResults()->searching());
        QCOMPARE(session.searchQuery(), u"Typography"_s);
        QCOMPARE(session.searchGenreId(), oldGenre);
        const auto expected = session.searchResults()->rowCount();
        QVERIFY(expected > 0);
        session.search(u"Typography"_s, oldGenre);
        QCOMPARE(changes.size(), 1);
        // No QML view exists to repair the filter when its genre disappears.
        snapshot.genres.first().id = u"replacement-genre"_s;
        snapshot.albums.first().genreId = u"replacement-genre"_s;
        session.setSnapshot(snapshot);
        QTRY_VERIFY(!session.searchResults()->searching());
        QCOMPARE(session.searchGenreId(), QString{ });
        QCOMPARE(session.searchQuery(), u"Typography"_s);
        QCOMPARE(session.searchResults()->rowCount(), expected);
        QCOMPARE(changes.size(), 2);
        session.search(u"Typography"_s, u"absent-genre"_s);
        QCOMPARE(session.searchGenreId(), QString{ });
        QCOMPARE(changes.size(), 2);
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
        QCOMPARE(session.albums()->roleNames().value(AlbumListModel::AlbumId), "albumId"_ba);
        const auto &a = session.snapshot()->albums[0];
        const auto albumId = a.id, trackId = a.tracks[0].id;
        QVERIFY(session.setPlaybackIds(albumId, trackId));
        playback.clear();
        session.sort(u"artist"_s);
        session.search(u"Typography"_s);
        QTRY_COMPARE(session.searchResults()->rowCount(), 28);
        QVERIFY(session.navigate(session.snapshot()->albums[1].id));
        session.requestMetadata(albumId, u"introduction"_s);
        QTRY_COMPARE(session.metadata().size(), 1);
        QCOMPARE(playback.size(), 0);
        QCOMPARE(session.playbackAlbumId(), albumId);
        QCOMPARE(session.playbackTrackId(), trackId);
        QVERIFY(!session.navigate(u"missing"_s));
        QVERIFY(!session.setPlaybackIds(albumId, u"missing"_s));
        session.setSnapshot(library(u"empty"_s));
        QCOMPARE(session.albums()->rowCount(), 0);
        QCOMPARE(session.searchResults()->rowCount(), 0);
        QVERIFY(session.navigationAlbumId().isEmpty());
        QCOMPARE(session.playbackTrackId(), trackId);
        QVERIFY(!session.albums()
                         ->data(session.albums()->index(0), AlbumListModel::Title)
                         .isValid());
    }
    void persistedMetadataHasTheSameStatusAndSourceAsProviderCache()
    {
        OfflineMetadataProvider provider;
        auto snapshot = library(u"single"_s);
        auto &album = snapshot.albums[0];
        album.introduction = { { u"status"_s, u"matched"_s } };
        album.creditsLookup = { { u"status"_s, u"not-found"_s },
                                { u"source"_s, u"MusicBrainz"_s },
                                { u"sourceUrl"_s, u"https://musicbrainz.org/release/fixture"_s } };
        LibrarySession session(&provider);
        session.setSnapshot(snapshot);
        QCOMPARE(session.metadataFor(album.id, u"introduction"_s).value(u"status"_s).toString(),
                 u"Matched"_s);
        const auto credits = session.metadataFor(album.id, u"credits"_s);
        QCOMPARE(credits.value(u"status"_s).toString(), u"NotFound"_s);
        QCOMPARE(credits.value(u"source"_s).toMap().value(u"url"_s).toString(),
                 u"https://musicbrainz.org/release/fixture"_s);
        QVERIFY(session.metadataFor(album.id, u"invalid"_s).isEmpty());
    }
    void offlineCacheAndCancellation()
    {
        OfflineMetadataProvider provider;
        auto snapshot = library(u"single"_s);
        auto &album = snapshot.albums[0];
        album.introduction = { { u"status"_s, u"matched"_s }, { u"checkedAt"_s, u"2026-10-01"_s } };
        album.creditsLookup = { { u"status"_s, u"not-found"_s },
                                { u"retryAt"_s, u"2026-11-01"_s },
                                { u"source"_s, u"QQ Music"_s } };
        provider.setCache(std::make_shared<const LibrarySnapshot>(snapshot));
        QSignalSpy spy(&provider, &IMetadataProvider::resultReady);
        AlbumQuery query;
        query.albumId = album.id;
        query.force = true;
        QVERIFY(provider.requestIntroduction(query, { 1, 2, album.id }));
        QCOMPARE(spy.size(), 0);
        QTRY_COMPARE(spy.size(), 1);
        auto result = qvariant_cast<ProviderResult>(spy.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::Matched);
        QCOMPARE(result.checkedAt, u"2026-10-01"_s);
        QCOMPARE(result.payload.value(u"description"_s).toString(), album.description);
        QVERIFY(provider.requestCredits(query, { 2, 2, album.id }));
        QTRY_COMPARE(spy.size(), 1);
        result = qvariant_cast<ProviderResult>(spy.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::NotFound);
        QCOMPARE(result.retryAt, u"2026-11-01"_s);
        QVERIFY(provider.requestEnrichment(query, { 3, 2, album.id }));
        QTRY_COMPARE(spy.size(), 1);
        result = qvariant_cast<ProviderResult>(spy.takeFirst()[0]);
        QCOMPARE(result.status, ProviderStatus::Deferred);
        QVERIFY(result.retryAt.isEmpty());
        QVERIFY(result.checkedAt.isEmpty());
        QVERIFY(result.payload.isEmpty());
        QCOMPARE(result.error, u"\u8054\u7f51\u8d44\u6599\u529f\u80fd\u6682\u672a\u542f\u7528"_s);
        QVERIFY(provider.requestIntroduction(query, { 4, 2, album.id }));
        provider.cancel({ 4, 2, album.id });
        QCoreApplication::processEvents();
        QCOMPARE(spy.size(), 0);
        query.albumId = "missing"_L1;
        QVERIFY(provider.requestCredits(query, { 5, 2, query.albumId }));
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(qvariant_cast<ProviderResult>(spy.takeFirst()[0]).status,
                 ProviderStatus::Deferred);
        QVERIFY(!provider.capabilities().networkEnabled);
        auto *temporary = new OfflineMetadataProvider;
        connect(temporary, &IMetadataProvider::resultReady, this, [] {
            QFAIL("Provider emitted after destruction");
        });
        QVERIFY(temporary->requestIntroduction(query, { 1, 0, query.albumId }));
        delete temporary;
        QCoreApplication::processEvents();
    }
    void offlineCacheRetainsProvenanceAndEnrichment()
    {
        OfflineMetadataProvider provider;
        auto snapshot = library(u"single"_s);
        auto &album = snapshot.albums[0];
        const QJsonObject provenance{ { u"name"_s, u"MusicBrainz"_s },
                                      { u"url"_s, u"https://musicbrainz.org/release/fixture"_s },
                                      { u"license"_s, u"CC0"_s } };
        album.creditsLookup = { { u"status"_s, u"Partial"_s }, { u"sourceDetails"_s, provenance } };
        album.online = { { u"status"_s, u"matched"_s }, { u"sourceDetails"_s, provenance } };
        const QJsonArray genres{ u"Jazz"_s, u"Ambient"_s };
        album.fields.insert(u"_onlineGenres"_s, genres);
        provider.setCache(std::make_shared<const LibrarySnapshot>(snapshot));
        QSignalSpy results(&provider, &IMetadataProvider::resultReady);
        AlbumQuery query;
        query.albumId = album.id;
        QVERIFY(provider.requestCredits(query, { 1, 7, album.id }));
        QVERIFY(provider.requestEnrichment(query, { 2, 7, album.id }));
        QTRY_COMPARE(results.size(), 2);
        const auto credits = qvariant_cast<ProviderResult>(results[0][0]);
        QCOMPARE(credits.status, ProviderStatus::Partial);
        QCOMPARE(credits.source, provenance);
        QCOMPARE(credits.generation, quint64(7));
        const auto enrichment = qvariant_cast<ProviderResult>(results[1][0]);
        QCOMPARE(enrichment.status, ProviderStatus::Matched);
        QCOMPARE(enrichment.source, provenance);
        QCOMPARE(enrichment.payload.value(u"genres"_s).toArray(), genres);
        QCOMPARE(enrichment.albumId, album.id);
    }
    void offlineAdmissionIsBoundedAndCancelledWorkDoesNotAccumulate()
    {
        OfflineMetadataProvider provider;
        auto snapshot = library(u"single"_s);
        snapshot.albums.first().description = u"First cache"_s;
        const auto albumId = snapshot.albums.first().id;
        provider.setCache(std::make_shared<const LibrarySnapshot>(snapshot));
        AlbumQuery query;
        query.albumId = albumId;
        QSignalSpy results(&provider, &IMetadataProvider::resultReady);
        for (quint64 id = 1; id <= 10000; ++id) {
            const RequestToken token{ id, 0, albumId };
            QVERIFY(provider.requestIntroduction(query, token));
            provider.cancel(token);
        }
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 0);
        for (quint64 id = 10001; id <= 10008; ++id)
            QVERIFY(provider.requestIntroduction(query, { id, 0, albumId }));
        QVERIFY(!provider.requestIntroduction(query, { 10009, 0, albumId }));
        snapshot.albums.first().description = u"Replacement cache"_s;
        provider.setCache(std::make_shared<const LibrarySnapshot>(snapshot));
        QTRY_COMPARE(results.size(), 8);
        for (int index = 0; index < results.size(); ++index) {
            const auto result = qvariant_cast<ProviderResult>(results[index][0]);
            QCOMPARE(result.requestId, quint64(10001 + index));
            QCOMPARE(result.payload.value(u"description"_s).toString(), u"First cache"_s);
        }
        results.clear();
        QVERIFY(provider.requestIntroduction(query, { 10010, 0, albumId }));
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(qvariant_cast<ProviderResult>(results.first()[0])
                         .payload.value(u"description"_s)
                         .toString(),
                 u"Replacement cache"_s);
    }
    void sessionHandlesProviderAdmissionFailure()
    {
        OfflineMetadataProvider provider;
        LibrarySession session(&provider);
        session.setSnapshot(library(u"single"_s));
        AlbumQuery query;
        query.albumId = session.snapshot()->albums.first().id;
        for (quint64 id = 100; id < 108; ++id)
            QVERIFY(provider.requestIntroduction(query, { id, 0, query.albumId }));
        QCOMPARE(session.requestMetadata(query.albumId, u"introduction"_s), quint64(0));
        for (quint64 id = 100; id < 108; ++id)
            provider.cancel({ id, 0, query.albumId });
        QSignalSpy delivered(&session, &LibrarySession::metadataDelivered);
        const auto accepted = session.requestMetadata(query.albumId, u"introduction"_s);
        QVERIFY(accepted > 0);
        QTRY_COMPARE(delivered.size(), 1);
        QCOMPARE(qvariant_cast<ProviderResult>(delivered.first()[0]).requestId, accepted);
    }
    void replacingSnapshotDuringCancellationRejectsOldQuery()
    {
        TestProvider provider;
        LibrarySession session(&provider);
        session.setSnapshot(library(u"single"_s));
        const auto albumId = session.snapshot()->albums.first().id;
        QVERIFY(session.requestMetadataBatch(u"introduction"_s));
        QCOMPARE(provider.requests.size(), 1);
        bool replaced = false;
        connect(&session, &LibrarySession::metadataProgressChanged, &session, [&] {
            if (!replaced && !session.metadataProgress().value(u"active"_s).toBool()) {
                replaced = true;
                session.setSnapshot({ });
            }
        });
        QCOMPARE(session.requestMetadata(albumId, u"introduction"_s), quint64(0));
        QVERIFY(replaced);
        QCOMPARE(provider.requests.size(), 1);
        QVERIFY(session.snapshot()->albums.isEmpty());
    }
    void batchGenerationAndMergedLateSubscribers()
    {
        TestProvider provider;
        LibrarySession session(&provider);
        auto snapshot = library(u"mixed"_s);
        snapshot.albums = snapshot.albums.mid(0, 3);
        session.setSnapshot(snapshot);
        QSignalSpy delivered(&session, &LibrarySession::metadataDelivered);
        QVERIFY(session.requestMetadataBatch(u"introduction"_s));
        QCOMPARE(provider.requests.size(), 1);
        QVERIFY(session.metadataProgress()[u"active"_s].toBool());
        provider.deliver(0, ProviderStatus::Matched);
        QTRY_COMPARE(provider.requests.size(), 2);
        QCOMPARE(session.metadataProgress()[u"completed"_s].toInt(), 1);
        session.cancelAllMetadata();
        QVERIFY(session.requestMetadataBatch(u"introduction"_s));
        QCOMPARE(provider.requests.size(), 3);
        // A coalescing provider can finish the old and new subscription together.
        provider.deliver(1, ProviderStatus::Error);
        provider.deliver(2, ProviderStatus::Uncertain);
        QTRY_COMPARE(provider.requests.size(), 4);
        QCOMPARE(delivered.size(), 2);
        QCOMPARE(session.metadataProgress()[u"completed"_s].toInt(), 1);
        QCOMPARE(session.metadataFor(snapshot.albums[0].id, u"introduction"_s)[u"status"_s]
                         .toString(),
                 u"Uncertain"_s);
        provider.deliver(3, ProviderStatus::Error);
        QTRY_COMPARE(provider.requests.size(), 5);
        provider.deliver(4, ProviderStatus::Matched);
        QTRY_VERIFY(!session.metadataProgress()[u"active"_s].toBool());
        QCOMPARE(session.metadataProgress()[u"completed"_s].toInt(), 3);
        QCOMPARE(session.metadataProgress()[u"errors"_s].toInt(), 1);
        QCOMPARE(delivered.size(), 4);
        QVERIFY(session.requestMetadataBatch(u"credits"_s));
        session.setSnapshot(snapshot);
        provider.deliver(5, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        QCOMPARE(delivered.size(), 4);
        QVERIFY(session.metadata().isEmpty());
        QVERIFY(!session.metadataProgress()[u"active"_s].toBool());
        QVERIFY(session.playbackTrackId().isEmpty());
        QVERIFY(session.navigationAlbumId().isEmpty());
    }
    void metadataUpdatesRejectStructuralChanges_data()
    {
        QTest::addColumn<int>("mutation");
        QTest::newRow("removed-track") << 0;
        QTest::newRow("renamed-track-id") << 1;
        QTest::newRow("changed-album-sort-key") << 2;
        QTest::newRow("changed-track-content") << 3;
        QTest::newRow("changed-genre") << 4;
        QTest::newRow("changed-cover") << 5;
    }
    void metadataUpdatesRejectStructuralChanges()
    {
        QFETCH(int, mutation);
        LibrarySession session(nullptr);
        session.setSnapshot(library(u"single"_s));
        const auto original = session.snapshot();
        const auto albumId = original->albums.first().id;
        const auto trackId = original->albums.first().tracks.last().id;
        QVERIFY(session.navigate(albumId, trackId));
        session.searchResults()->update(original, original->albums.first().tracks.last().title,
                                        { });
        auto candidate = *original;
        auto &album = candidate.albums.first();
        switch (mutation) {
        case 0:
            album.tracks.removeLast();
            break;
        case 1:
            album.tracks.last().id = u"replacement"_s;
            break;
        case 2:
            album.title = u"Different sort key"_s;
            break;
        case 3:
            album.tracks.last().title = u"Different search content"_s;
            break;
        case 4:
            candidate.genres.first().name = u"Different genre"_s;
            break;
        case 5:
            album.coverUrl = QUrl(u"qrc:/different.png"_s);
            break;
        }
        const auto changed = std::make_shared<const LibrarySnapshot>(std::move(candidate));
        QSignalSpy notifications(session.albums(), &QAbstractItemModel::dataChanged);
        session.adoptMetadataSnapshot(changed);
        QCOMPARE(session.snapshot(), original);
        QCOMPARE(session.navigationTrackId(), trackId);
        QVERIFY(session.contains(albumId, trackId));
        QCOMPARE(session.trackIndex(albumId, trackId),
                 int(original->albums.first().tracks.size() - 1));
        QCOMPARE(notifications.size(), 0);
        // The model entry points are independently safe for callers outside LibrarySession.
        session.albums()->updateMetadataSnapshot(changed);
        session.searchResults()->updateMetadataSnapshot(changed);
        session.searchResults()->updateMetadataSnapshot({ });
        QCOMPARE(session.albums()->snapshot(), original);
        bool found = false;
        for (int row = 0; row < session.searchResults()->rowCount(); ++row) {
            const auto index = session.searchResults()->index(row);
            if (session.searchResults()->data(index, SearchListModel::TrackId).toString()
                == trackId) {
                found = true;
                QCOMPARE(session.searchResults()->data(index, SearchListModel::Title).toString(),
                         original->albums.first().tracks.last().title);
            }
        }
        QVERIFY(found);
    }
    void metadataContentChangesNotifyWithoutStateChanges()
    {
        LibrarySession session(nullptr);
        session.setSnapshot(library(u"single"_s));
        const auto original = session.snapshot();
        auto updated = *original;
        auto &album = updated.albums.first();
        album.description = u"Updated text with the same query state"_s;
        album.descriptionSource = { { u"url"_s, u"https://example.test/source"_s } };
        Producer producer;
        producer.name = u"New contributor"_s;
        producer.role = u"Composer"_s;
        album.producers.append(producer);
        QSignalSpy changed(session.albums(), &QAbstractItemModel::dataChanged);
        QSignalSpy reset(session.albums(), &QAbstractItemModel::modelReset);
        session.adoptMetadataSnapshot(std::make_shared<const LibrarySnapshot>(updated));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(reset.size(), 0);
        QCOMPARE(session.albums()
                         ->data(session.albums()->index(0), AlbumListModel::Description)
                         .toString(),
                 album.description);
        QCOMPARE(session.snapshot()->albums.first().descriptionSource, album.descriptionSource);
        QCOMPARE(original->albums.first().introduction, album.introduction);
    }
    void metadataNotificationCannotRestoreAReplacedLibrary()
    {
        LibrarySession session(nullptr);
        session.setSnapshot(library(u"single"_s));
        auto updated = *session.snapshot();
        updated.albums.first().introduction = { { u"status"_s, u"matched"_s } };
        bool replaced = false;
        connect(session.albums(), &QAbstractItemModel::dataChanged, &session, [&] {
            replaced = true;
            session.setSnapshot({ });
        });
        session.adoptMetadataSnapshot(std::make_shared<const LibrarySnapshot>(updated));
        QVERIFY(replaced);
        QVERIFY(session.snapshot()->albums.isEmpty());
        QCOMPARE(session.albums()->rowCount(), 0);
        session.searchResults()->request(u"anything"_s, { });
        QTRY_VERIFY(!session.searchResults()->searching());
        QCOMPARE(session.searchResults()->rowCount(), 0);
    }
    void cancellingBatchFromRequestNotificationKeepsCancelledState_data()
    {
        QTest::addColumn<bool>("cancelAll");
        QTest::newRow("cancel-all") << true;
        QTest::newRow("cancel-current-request") << false;
    }
    void cancellingBatchFromRequestNotificationKeepsCancelledState()
    {
        QFETCH(bool, cancelAll);
        TestProvider provider;
        LibrarySession session(&provider);
        session.setSnapshot(library(u"single"_s));
        bool cancelled = false;
        connect(&session, &LibrarySession::metadataChanged, &session, [&] {
            if (!cancelled && session.metadataBusy()) {
                cancelled = true;
                if (cancelAll)
                    session.cancelAllMetadata();
                else
                    session.cancelMetadata(provider.requests.last().requestId);
            }
        });
        QVERIFY(session.requestMetadataBatch(u"introduction"_s));
        QVERIFY(cancelled);
        QVERIFY(!session.metadataBusy());
        QVERIFY(!session.metadataProgress().value(u"active"_s).toBool());
        QCOMPARE(session.metadataProgress().value(u"status"_s).toString(), u"Cancelled"_s);
    }
    void replacingBatchFromRequestNotificationPreservesNewBatch()
    {
        TestProvider provider;
        LibrarySession session(&provider);
        session.setSnapshot(library(u"single"_s));
        bool replaced = false;
        connect(&session, &LibrarySession::metadataChanged, &session, [&] {
            if (!replaced && session.metadataBusy()) {
                replaced = true;
                QVERIFY(session.requestMetadataBatch(u"credits"_s));
            }
        });
        QVERIFY(session.requestMetadataBatch(u"introduction"_s));
        QVERIFY(replaced);
        QCOMPARE(provider.requests.size(), 2);
        QCOMPARE(session.metadataProgress().value(u"kind"_s).toString(), u"credits"_s);
        QVERIFY(session.metadataProgress().value(u"active"_s).toBool());
        provider.deliver(1, ProviderStatus::Matched);
        QTRY_VERIFY(!session.metadataProgress().value(u"active"_s).toBool());
        QCOMPARE(session.metadataProgress().value(u"completed"_s).toInt(), 1);
        QCOMPARE(session.metadataProgress().value(u"status"_s).toString(), u"Matched"_s);
    }
    void providerCancellationCannotOverwriteNewerBatchIntent_data()
    {
        QTest::addColumn<bool>("completeRequest");
        QTest::newRow("complete-request") << true;
        QTest::newRow("early-return-with-pending-request") << false;
    }
    void providerCancellationCannotOverwriteNewerBatchIntent()
    {
        QFETCH(bool, completeRequest);
        TestProvider provider;
        {
            LibrarySession session(&provider);
            session.setSnapshot(library(u"single"_s));
            // QtTest assertions return from this scope. The callback must be gone before
            // its captures expire, even when the replacement request is still pending.
            const auto exercise = [&] {
                const auto id = session.snapshot()->albums.first().id;
                QVERIFY(session.requestMetadata(id, u"introduction"_s));
                bool replaced = false;
                const auto clearCallback = qScopeGuard([&provider] {
                    provider.onCancel = { };
                });
                provider.onCancel = [&] {
                    if (!replaced) {
                        replaced = true;
                        QVERIFY(session.requestMetadataBatch(u"credits"_s));
                    }
                };
                QVERIFY(!session.requestMetadataBatch(u"introduction"_s));
                QVERIFY(replaced);
                QCOMPARE(provider.requests.size(), 2);
                QCOMPARE(session.metadataProgress().value(u"kind"_s).toString(), u"credits"_s);
                if (!completeRequest)
                    return;
                provider.deliver(1, ProviderStatus::Matched);
                QTRY_COMPARE(session.metadataProgress().value(u"completed"_s).toInt(), 1);
            };
            exercise();
            QVERIFY(!provider.onCancel);
            QCOMPARE(session.metadataBusy(), !completeRequest);
        }
        QCOMPARE(provider.cancelled.size(), completeRequest ? 1 : 2);
    }
    void shutdownRejectsReentrantProviderRequests()
    {
        TestProvider provider;
        auto session = std::make_unique<LibrarySession>(&provider);
        session->setSnapshot(library(u"single"_s));
        const auto id = session->snapshot()->albums.first().id;
        QVERIFY(session->requestMetadata(id, u"introduction"_s));
        auto *closing = session.get();
        quint64 request = 1;
        bool batch = true;
        const auto clearCallback = qScopeGuard([&provider] {
            provider.onCancel = { };
        });
        provider.onCancel = [&] {
            request = closing->requestMetadata(id, u"credits"_s);
            batch = closing->requestMetadataBatch(u"credits"_s);
        };
        session.reset();
        QCOMPARE(request, quint64(0));
        QVERIFY(!batch);
        QCOMPARE(provider.requests.size(), 1);
    }
    void injectedProviderStaleResults()
    {
        TestProvider provider;
        LibrarySession session(&provider);
        session.setSnapshot(library(u"single"_s));
        const QString albumId = session.snapshot()->albums[0].id;
        QSignalSpy results(&session, &LibrarySession::metadataDelivered);
        session.requestMetadata(albumId, u"introduction"_s);
        session.requestMetadata(albumId, u"introduction"_s);
        QCOMPARE(provider.cancelled.size(), 1);
        provider.deliver(0, ProviderStatus::Matched);
        provider.deliver(1, ProviderStatus::Matched, 1);
        provider.deliver(1, ProviderStatus::Matched, 0, u"wrong"_s);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 0);
        provider.deliver(1, ProviderStatus::Uncertain);
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(session.metadata()
                         .value(albumId + u"/introduction"_s)
                         .toMap()
                         .value(u"status"_s)
                         .toString(),
                 u"Uncertain"_s);
        provider.deliver(1, ProviderStatus::Error);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 1);
        const auto cancelled = session.requestMetadata(albumId, u"credits"_s);
        session.cancelMetadata(cancelled);
        provider.deliver(2, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 1);
        session.requestMetadata(albumId, u"enrichment"_s);
        session.setSnapshot(library(u"single"_s));
        provider.deliver(3, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 1);
        session.requestMetadata(albumId, u"credits"_s);
        provider.deliver(4, ProviderStatus::Error);
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(session.metadata()
                         .value(albumId + u"/credits"_s)
                         .toMap()
                         .value(u"status"_s)
                         .toString(),
                 u"Error"_s);
        session.requestMetadata(albumId, u"enrichment"_s);
        provider.deliver(5, ProviderStatus::Partial);
        QTRY_COMPARE(results.size(), 3);
        auto *destroyed = new LibrarySession(&provider);
        destroyed->setSnapshot(library(u"single"_s));
        destroyed->requestMetadata(albumId, u"introduction"_s);
        delete destroyed;
        provider.deliver(6, ProviderStatus::Matched);
        QCoreApplication::processEvents();
        auto *shortLived = new TestProvider;
        LibrarySession survivor(shortLived);
        survivor.setSnapshot(library(u"single"_s));
        survivor.requestMetadata(albumId, u"introduction"_s);
        QCOMPARE(survivor.metadataFor(albumId, u"introduction"_s).value(u"status"_s).toString(),
                 u"Pending"_s);
        QSignalSpy providerLost(&survivor, &LibrarySession::metadataChanged);
        const auto revision = survivor.metadataRevision();
        delete shortLived;
        QVERIFY(!survivor.metadataBusy());
        QCOMPARE(providerLost.size(), 1);
        QCOMPARE(survivor.metadataRevision(), revision + 1);
        QVERIFY(survivor.metadataFor(albumId, u"introduction"_s).value(u"status"_s).toString()
                != u"Pending"_s);
        QCOMPARE(survivor.requestMetadata(albumId, u"credits"_s), quint64(0));
    }
};
QTEST_MAIN(LibraryDataTest)
#include "LibraryDataTest.moc"
