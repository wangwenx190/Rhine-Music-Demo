#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include <QFile>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

using namespace yalahoo;
class InjectedProvider : public IMetadataProvider {
public:
    using IMetadataProvider::IMetadataProvider;
    ProviderStatus status = ProviderStatus::Matched;
    ProviderCapabilities capabilities() const override { return {}; }
    void requestIntroduction(AlbumQuery, RequestToken token) override {
        ProviderResult result;
        result.requestId = token.requestId; result.generation = token.generation; result.albumId = token.albumId;
        result.status = status; result.payload = {{"description", "injected"}};
        emit resultReady(result); // Router guarantees queued delivery even for this test provider.
    }
    void requestCredits(AlbumQuery query, RequestToken token) override { requestIntroduction(query, token); }
    void requestEnrichment(AlbumQuery query, RequestToken token) override { requestIntroduction(query, token); }
    void cancel(RequestToken) override {}
};
class LibraryQmlTest : public QObject {
    Q_OBJECT
private slots:
    void readsRolesAndInjectedResults() {
        qmlRegisterUncreatableType<LibrarySession>("Yalahoo.DataTest", 1, 0, "LibrarySession", "Injected");
        qmlRegisterUncreatableType<AlbumListModel>("Yalahoo.DataTest", 1, 0, "AlbumListModel", "Owned");
        qmlRegisterUncreatableType<SearchListModel>("Yalahoo.DataTest", 1, 0, "SearchListModel", "Owned");
        QFile input(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/web-baseline/anonymous.json"));
        QVERIFY(input.open(QIODevice::ReadOnly));
        LibrarySnapshot snapshot;
        QVERIFY(readLibrary(QJsonDocument::fromJson(input.readAll()).object().value("libraries").toObject().value("single").toObject(), snapshot));
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
        )";
        const auto run = [&](IMetadataProvider *provider, bool injected) {
            LibrarySession session(provider); session.setSnapshot(snapshot);
            QQmlEngine engine;
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            QQmlComponent component(&engine);
            component.setData(qml, QUrl(QStringLiteral("qrc:/library-data-test.qml")));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            std::unique_ptr<QObject> root(component.createWithInitialProperties({{"librarySession", QVariant::fromValue(&session)}}));
            QVERIFY2(root, qPrintable(component.errorString()));
            QTRY_COMPARE(root->property("albumCount").toInt(), 1);
            QVariant result;
            QVERIFY(QMetaObject::invokeMethod(root.get(), "roleAt", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 0)));
            const auto roles = result.toMap();
            QCOMPARE(roles.value("albumId").toString(), album.id); QCOMPARE(roles.value("title").toString(), album.title);
            QCOMPARE(roles.value("trackId").toString(), album.tracks.first().id);
            QCOMPARE(roles.value("support").toString(), QStringLiteral("Unknown"));
            QCOMPARE(roles.value("description").toString(), album.description);
            session.search("Typography"); QTRY_COMPARE(root->property("searchCount").toInt(), 1);
            const auto key = album.id + "/introduction";
            session.requestMetadata(album.id, "introduction");
            QTRY_VERIFY(root->property("metadataValues").toMap().contains(key));
            QCOMPARE(root->property("metadataValues").toMap().value(key).toMap().value("status").toString(), QStringLiteral("Matched"));
            if (injected) {
                auto *test = static_cast<InjectedProvider *>(provider);
                for (auto status : {ProviderStatus::Uncertain, ProviderStatus::Error}) {
                    test->status = status; session.requestMetadata(album.id, "introduction");
                    QTRY_COMPARE(root->property("metadataValues").toMap().value(key).toMap().value("status").toString(), statusName(status));
                }
            }
            session.setSnapshot({}); QTRY_COMPARE(root->property("albumCount").toInt(), 0);
            QTRY_COMPARE(root->property("searchCount").toInt(), 0);
            QCOMPARE(warnings.size(), 0);
        };
        OfflineMetadataProvider offline; run(&offline, false);
        InjectedProvider injected; run(&injected, true);
    }
};
QTEST_MAIN(LibraryQmlTest)
#include "LibraryQmlTest.moc"
