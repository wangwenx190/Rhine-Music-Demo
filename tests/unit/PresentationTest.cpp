#include "PresentationController.h"
#include "DetailModel.h"
#include "OfflineMetadataProvider.h"
#include <QSignalSpy>
#include <QTest>

using namespace yalahoo;
namespace {
struct Fixture
{
    bool archiveReady = true, presentationReady = false, interactive = false;
    QStringList events;
    PresentationPorts ports()
    {
        return { [&] {
                    return presentationReady;
                },
                 [&] {
                     return archiveReady;
                 },
                 [&] {
                     return interactive;
                 },
                 [&] {
                     events << "enter";
                     archiveReady = false;
                     presentationReady = false;
                 },
                 [&] {
                     events << "return";
                     archiveReady = false;
                     presentationReady = false;
                 },
                 [&](const PresentationSelection &s) {
                     events << "select:" + s.albumId;
                     archiveReady = false;
                 },
                 [&](const PresentationSelection &s) {
                     events << "switch:" + s.albumId;
                     presentationReady = false;
                 },
                 { },
                 { },
                 { } };
    }
    PresentationController controller{ nullptr, ports() };
    void openDetail()
    {
        controller.open();
        controller.browseSurface()->finish();
        controller.layoutReady(controller.layoutRevision());
        presentationReady = true;
        controller.update();
        controller.detailSurface()->finish();
    }
};
}
class PresentationTest : public QObject
{
    Q_OBJECT
private slots:
    void openingReadinessAndSingleNotification()
    {
        Fixture f;
        QSignalSpy changes(&f.controller, &PresentationController::changed);
        f.controller.open();
        QCOMPARE(changes.size(), 1);
        QCOMPARE(f.controller.phase(), "opening");
        QVERIFY(!f.controller.archiveEnabled());
        QVERIFY(f.events.isEmpty());
        f.presentationReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), "opening");
        f.controller.browseSurface()->finish();
        QCOMPARE(f.events, QStringList{ "enter" });
        f.presentationReady = true;
        for (int i = 0; i < 120; ++i)
            f.controller.update();
        QCOMPARE(f.controller.phase(), "opening");
        f.controller.layoutReady(f.controller.layoutRevision() - 1);
        f.controller.update();
        QCOMPARE(f.controller.phase(), "opening");
        f.controller.layoutReady(f.controller.layoutRevision());
        f.controller.update();
        QCOMPARE(f.controller.phase(), "detail");
        const auto count = changes.size();
        f.controller.update();
        f.controller.open();
        QCOMPARE(changes.size(), count);
    }
    void latestSelectionDuringBrowseExit()
    {
        Fixture f;
        f.controller.open();
        f.controller.select("first", true);
        f.controller.select("last", true);
        f.controller.browseSurface()->finish();
        QCOMPARE(f.events, QStringList{ "select:last" });
        QCOMPARE(f.controller.phase(), "selecting");
        f.controller.update();
        QCOMPARE(f.controller.phase(), "selecting");
        f.archiveReady = true;
        f.controller.update();
        f.controller.browseSurface()->finish();
        QCOMPARE(f.events, (QStringList{ "select:last", "enter" }));
    }
    void rapidDetailSwitchAndCancellation()
    {
        Fixture f;
        f.openDetail();
        f.events.clear();
        f.controller.select("a", true);
        f.controller.select("b", true);
        QCOMPARE(f.controller.phase(), "switch-hiding");
        QVERIFY(f.events.isEmpty());
        f.controller.detailSurface()->finish();
        QCOMPARE(f.events, QStringList{ "switch:b" });
        f.controller.select("c", true);
        f.presentationReady = true;
        const auto staleLayout = f.controller.layoutRevision();
        f.controller.update();
        QCOMPARE(f.events, (QStringList{ "switch:b", "switch:c" }));
        QCOMPARE(f.controller.phase(), "switching");
        f.controller.layoutReady(staleLayout);
        f.presentationReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), "switching");
        f.controller.layoutReady(f.controller.layoutRevision());
        f.controller.update();
        QCOMPARE(f.controller.phase(), "detail");
        f.controller.select("cancelled", true);
        f.controller.back();
        f.controller.detailSurface()->finish();
        QCOMPARE(f.events.last(), "return");
        QVERIFY(!f.events.contains("switch:cancelled"));
        QVERIFY(!f.controller.pendingSelection());
    }
    void archiveRoute_data()
    {
        QTest::addColumn<QString>("phase");
        for (const auto &phase : { "opening", "detail", "switch-hiding", "switching" })
            QTest::newRow(phase) << QString(phase);
    }
    void archiveRoute()
    {
        QFETCH(QString, phase);
        Fixture f;
        if (phase == "opening") {
            f.controller.open();
            f.controller.browseSurface()->finish();
        } else {
            f.openDetail();
        }
        if (phase.startsWith("switch")) {
            f.controller.select("old", true);
            if (phase == "switching")
                f.controller.detailSurface()->finish();
        }
        f.events.clear();
        f.controller.select("search", true, true, "track");
        f.controller.detailSurface()->finish();
        QCOMPARE(f.controller.phase(), "returning");
        QCOMPARE(f.events, QStringList{ "return" });
        f.interactive = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), "returning");
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.events, (QStringList{ "return", "select:search" }));
        QCOMPARE(f.controller.phase(), "selecting");
        f.controller.back();
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), "archive");
        QVERIFY(!f.controller.wantsDetail());
    }
    void staleExitAndReset()
    {
        Fixture f;
        f.controller.open();
        f.controller.back();
        f.controller.open();
        f.archiveReady = true;
        f.controller.update();
        f.controller.browseSurface()->finish();
        f.presentationReady = true;
        f.controller.layoutReady(f.controller.layoutRevision());
        f.controller.update();
        QCOMPARE(f.controller.phase(), "detail");
        QCOMPARE(f.events.count("enter"), 1);
        f.controller.select("obsolete", true);
        f.controller.reset();
        f.controller.detailSurface()->finish();
        f.controller.update();
        QCOMPARE(f.controller.phase(), "archive");
        QVERIFY(!f.events.contains("switch:obsolete"));
    }
    void reducedMotionAndSpeed()
    {
        Fixture f;
        f.controller.open();
        f.controller.setReducedMotion(true);
        QCOMPARE(f.events, QStringList{ "enter" });
        f.controller.layoutReady(f.controller.layoutRevision());
        f.presentationReady = true;
        f.controller.update();
        QVERIFY(!f.controller.detailSurface()->running());
        f.controller.select("next", true, true);
        QCOMPARE(f.controller.phase(), "returning");
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), "selecting");
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), "opening");
        SurfaceTransition surface(360, 240, true, false);
        surface.show(false);
        surface.advance(.09);
        const auto opacity = surface.opacity();
        int stale = 0;
        surface.hide(false, [&] {
            ++stale;
        });
        QCOMPARE(surface.opacity(), opacity);
        surface.show(false);
        QCOMPARE(surface.opacity(), opacity);
        surface.finish();
        QCOMPARE(stale, 0);
        QCOMPARE(surface.offset(), 0);
        FrameClock clock;
        PresentationController timed(&clock, f.ports());
        timed.open();
        clock.setActive(true);
        clock.advance(.01);
        clock.advance(.04);
        const auto before = timed.browseSurface()->opacity();
        clock.setSpeed(3);
        QCOMPARE(timed.browseSurface()->opacity(), before);
        clock.advance(.04);
        QVERIFY(!timed.browseSurface()->running());
    }
    void tabIndicatorUsesSharedClock()
    {
        FrameClock clock;
        Fixture f;
        PresentationController controller(&clock, f.ports());
        controller.setTabIndicator(0, 40, false);
        QCOMPARE(controller.tabWidth(), 40);
        controller.setTabIndicator(60, 70, true);
        QCOMPARE(controller.tabX(), 0);
        clock.setActive(true);
        clock.advance(.01);
        clock.advance(.04);
        QVERIFY(controller.tabX() > 0 && controller.tabX() < 60);
        const auto before = controller.tabX();
        clock.setSpeed(3);
        QCOMPARE(controller.tabX(), before);
        clock.setApplicationVisible(false);
        clock.setActive(false);
        clock.advance(.04);
        QCOMPARE(controller.tabX(), before);
        clock.setApplicationVisible(true);
        clock.setActive(true);
        controller.revealTabContent();
        QCOMPARE(controller.tabOpacity(), .35);
        clock.advance(.01);
        clock.advance(.01);
        QVERIFY(controller.tabOpacity() > .35 && controller.tabOpacity() < 1);
        controller.setReducedMotion(true);
        QCOMPARE(controller.tabOpacity(), 1);
        QCOMPARE(controller.tabX(), 60);
        QCOMPARE(controller.tabWidth(), 70);
    }
    void offlineStatusAndTrackAttribution()
    {
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        Album a;
        a.id = "offline";
        a.offline = true;
        a.description = "保留的介绍";
        a.introduction = { { "status", "uncertain" } };
        a.creditsLookup = { { "status", "partial" },
                            { "matchedTracks", 1 },
                            { "totalTracks", 2 },
                            { "retryAt", "2099-01-01T00:00:00Z" } };
        Track t;
        t.id = "track";
        t.title = "实际曲名";
        a.tracks.append(t);
        a.producers.append({ "制作人",
                             "制作",
                             "manual",
                             "track",
                             "过期曲名",
                             "https://example.invalid/credit",
                             { } });
        snapshot.albums.append(a);
        library.setSnapshot(snapshot);
        library.navigate(a.id);
        DetailModel model;
        model.prepare(library);
        QVERIFY(model.album()["offline"].toBool());
        QVERIFY(model.album()["introductionStatus"].toString().contains("存疑"));
        QVERIFY(model.album()["introductionStatus"].toString().contains("离线阅读"));
        QVERIFY(model.album()["creditsStatus"].toString().contains("1 / 2"));
        QVERIFY(model.album()["creditsStatus"].toString().contains("2099"));
        QCOMPARE(model.album()["producers"].toList()[0].toMap()["trackTitle"].toString(), t.title);
        QCOMPARE(model.album()["description"].toString(), a.description);
        QVERIFY(library.playbackTrackId().isEmpty());
    }
    void immutableDetailAndDataFields()
    {
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        Album a;
        a.id = "a";
        a.title = "匿名收藏";
        a.localNote = "本地备注";
        a.descriptionSource = { { "name", "匿名来源" }, { "url", "https://example.invalid" } };
        a.producers.append(
                { "参与人员", "制作", "来源", { }, { }, "https://example.invalid", { } });
        for (int i = 0; i < 10000; ++i) {
            Track t;
            t.id = QString::number(i);
            t.albumId = a.id;
            t.title = "曲目";
            t.duration = 61;
            t.discNumber = i < 5000 ? 1 : 2;
            a.tracks.append(t);
        }
        snapshot.albums.append(a);
        library.setSnapshot(snapshot);
        library.navigate("a");
        DetailModel model;
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.prepare(library);
        QCOMPARE(model.rowCount(), 10000);
        QCOMPARE(model.data(model.index(5000), DetailModel::DiscHeading).toBool(), true);
        QCOMPARE(model.data(model.index(5001), DetailModel::DiscHeading).toBool(), false);
        QCOMPARE(model.data(model.index(9999), DetailModel::Duration).toString(), "1:01");
        QCOMPARE(model.album().value("localNote").toString(), a.localNote);
        QCOMPARE(model.album().value("producers").toList().size(), 1);
        model.prepare(library);
        QCOMPARE(reset.size(), 1);
        library.search("other");
        QCOMPARE(model.rowCount(), 10000);
        QVERIFY(library.playbackAlbumId().isEmpty());
        library.setSnapshot({ });
        QCOMPARE(model.rowCount(), 10000); // Old immutable snapshot stays alive until the handoff.
        model.prepare(library);
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(reset.size(), 2);
    }
};
QTEST_GUILESS_MAIN(PresentationTest)
#include "PresentationTest.moc"
