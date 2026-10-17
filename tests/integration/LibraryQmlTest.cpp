#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include <QFile>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

using namespace yalahoo;
class InjectedProvider : public IMetadataProvider
{
public:
    using IMetadataProvider::IMetadataProvider;
    ProviderStatus status = ProviderStatus::Matched;
    ProviderCapabilities capabilities() const override
    {
        return { };
    }
    bool requestIntroduction(AlbumQuery, RequestToken token) override
    {
        ProviderResult result;
        result.requestId = token.requestId;
        result.generation = token.generation;
        result.albumId = token.albumId;
        result.status = status;
        result.payload = { { u"description"_s, u"injected"_s } };
        Q_EMIT resultReady(
                result); // Router guarantees queued delivery even for this test provider.
        return true;
    }
    bool requestCredits(AlbumQuery query, RequestToken token) override
    {
        return requestIntroduction(query, token);
    }
    bool requestEnrichment(AlbumQuery query, RequestToken token) override
    {
        return requestIntroduction(query, token);
    }
    void cancel(RequestToken) override
    {
    }
};
class LibraryQmlTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void readsRolesAndInjectedResults()
    {
        qmlRegisterUncreatableType<LibrarySession>("Yalahoo.DataTest", 1, 0, "LibrarySession",
                                                   u"Injected"_s);
        qmlRegisterUncreatableType<AlbumListModel>("Yalahoo.DataTest", 1, 0, "AlbumListModel",
                                                   u"Owned"_s);
        qmlRegisterUncreatableType<SearchListModel>("Yalahoo.DataTest", 1, 0, "SearchListModel",
                                                    u"Owned"_s);
        QFile input((QString::fromUtf8(FIXTURE_ROOT)
                     + u"/tests/fixtures/web-baseline/anonymous.json"_s));
        QVERIFY(input.open(QIODevice::ReadOnly));
        LibrarySnapshot snapshot;
        QVERIFY(readLibrary(QJsonDocument::fromJson(input.readAll())
                                    .object()
                                    .value(u"libraries"_s)
                                    .toObject()
                                    .value(u"single"_s)
                                    .toObject(),
                            snapshot));
        const auto album = snapshot.albums.first();
        const QByteArray qml = R"(
            import QtQuick
            import Yalahoo.DataTest
            Item {
                required property LibrarySession librarySession
                property int albumCount: librarySession.albums.count
                property int searchCount: librarySession.searchResults.count
                property var metadataValues: librarySession.metadata
                function roleAt(index) {
                    const item = rows.itemAt(index);
                    return item ? { albumId: item.albumId, title: item.title, trackId: item.tracks[0].id,
                        support: item.tracks[0].mediaSupport, description: item.description, source: item.tracks[0].sourceUrl } : {};
                }
                Repeater {
                    id: rows
                    model: librarySession.albums
                    delegate: Item {
                        required property string albumId
                        required property string title
                        required property var tracks
                        required property string description
                    }
                }
            }
        )"_ba;
        const auto run = [&](IMetadataProvider *provider, bool injected) {
            LibrarySession session(provider);
            session.setSnapshot(snapshot);
            QQmlEngine engine;
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            QQmlComponent component(&engine);
            component.setData(qml, QUrl(u"qrc:/library-data-test.qml"_s));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            std::unique_ptr<QObject> root(component.createWithInitialProperties(
                    { { u"librarySession"_s, QVariant::fromValue(&session) } }));
            QVERIFY2(root, qPrintable(component.errorString()));
            QTRY_COMPARE(root->property("albumCount").toInt(), 1);
            QVariant result;
            QVERIFY(QMetaObject::invokeMethod(root.get(), "roleAt", Q_RETURN_ARG(QVariant, result),
                                              Q_ARG(QVariant, 0)));
            const auto roles = result.toMap();
            QCOMPARE(roles.value(u"albumId"_s).toString(), album.id);
            QCOMPARE(roles.value(u"title"_s).toString(), album.title);
            QCOMPARE(roles.value(u"trackId"_s).toString(), album.tracks.first().id);
            QCOMPARE(roles.value(u"support"_s).toString(), u"Unknown"_s);
            QCOMPARE(roles.value(u"description"_s).toString(), album.description);
            session.search(u"Typography"_s);
            QTRY_COMPARE(root->property("searchCount").toInt(), 1);
            const auto key = album.id + "/introduction"_L1;
            session.requestMetadata(album.id, u"introduction"_s);
            QTRY_VERIFY(root->property("metadataValues").toMap().contains(key));
            QCOMPARE(root->property("metadataValues")
                             .toMap()
                             .value(key)
                             .toMap()
                             .value(u"status"_s)
                             .toString(),
                     u"Matched"_s);
            if (injected) {
                auto *test = static_cast<InjectedProvider *>(provider);
                for (auto status : { ProviderStatus::Uncertain, ProviderStatus::Error }) {
                    test->status = status;
                    session.requestMetadata(album.id, u"introduction"_s);
                    QTRY_COMPARE(root->property("metadataValues")
                                         .toMap()
                                         .value(key)
                                         .toMap()
                                         .value(u"status"_s)
                                         .toString(),
                                 statusName(status));
                }
            }
            session.setSnapshot({ });
            QTRY_COMPARE(root->property("albumCount").toInt(), 0);
            QTRY_COMPARE(root->property("searchCount").toInt(), 0);
            QCOMPARE(warnings.size(), 0);
        };
        OfflineMetadataProvider offline;
        run(&offline, false);
        InjectedProvider injected;
        run(&injected, true);
    }
};
QTEST_MAIN(LibraryQmlTest)
#include "LibraryQmlTest.moc"
