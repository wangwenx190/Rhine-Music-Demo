#include "FrameClock.h"
#include "LibrarySession.h"
#include "Localization.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "OfflineMetadataProvider.h"
#include <QSignalSpy>
#include <QTest>
#include <QAbstractItemModelTester>
#include <utility>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
struct Fixture final
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
                     events << u"enter"_s;
                     archiveReady = false;
                     presentationReady = false;
                 },
                 [&] {
                     events << u"return"_s;
                     archiveReady = false;
                     presentationReady = false;
                 },
                 [&](const PresentationSelection &s) {
                     events << "select:"_L1 + s.albumId;
                     archiveReady = false;
                 },
                 [&](const PresentationSelection &s) {
                     events << "switch:"_L1 + s.albumId;
                     presentationReady = false;
                 },
                 { },
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
class PresentationTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresentationTest)
public:
    PresentationTest() = default;

private:
    yalahoo::Localization localization{ QLocale(QLocale::Chinese, QLocale::China) };
private Q_SLOTS:
    void cancelledPreparationCannotHideBrowse()
    {
        for (const bool indicator : { false, true }) {
            Fixture f;
            bool once = false;
            auto cancel = [&] {
                if (!std::exchange(once, true))
                    f.controller.reset();
            };
            if (indicator)
                connect(&f.controller, &PresentationController::tabIndicatorChanged, &f.controller,
                        cancel);
            else
                connect(&f.controller, &PresentationController::prepared, &f.controller, cancel);
            f.controller.open();
            f.controller.browseSurface()->finish();
            f.controller.update();
            QCOMPARE(f.controller.phase(), u"archive"_s);
            QVERIFY(f.controller.browseSurface()->shown());
            QCOMPARE(f.controller.browseSurface()->opacity(), 1.0);
            QVERIFY(f.events.isEmpty());
        }
    }
    void selectionCallbacksCannotReviveCancelledPresentation()
    {
        for (const bool admission : { false, true }) {
            Fixture f;
            auto ports = f.ports();
            PresentationController *active = nullptr;
            ports.canSelect = [&](const PresentationSelection &) {
                if (admission)
                    active->reset();
                return true;
            };
            ports.select = [&](const PresentationSelection &) {
                active->reset();
            };
            PresentationController controller(nullptr, ports);
            active = &controller;
            controller.select(u"Album"_s, true);
            controller.update();
            QCOMPARE(controller.phase(), u"archive"_s);
            QVERIFY(controller.browseSurface()->shown());
            QVERIFY(!controller.pendingSelection());
        }
    }
    void detailResetObserversKeepOneCoherentLatestAlbum()
    {
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        for (int i = 0; i < 2; ++i) {
            Album album;
            album.id = QString::number(i);
            album.title = u"Album %1"_s.arg(i);
            Track track;
            track.id = u"track%1"_s.arg(i);
            track.title = album.title;
            album.tracks.append(track);
            snapshot.albums.append(album);
        }
        library.setSnapshot(snapshot);
        library.navigate(u"0"_s);
        DetailModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        bool once = false;
        int depth = 0, maximumDepth = 0;
        int scenario = 0;
        connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] {
            maximumDepth = std::max(maximumDepth, ++depth);
            if (!std::exchange(once, true)) {
                if (scenario == 2) {
                    snapshot.albums[1].title = u"Replacement album"_s;
                    library.setSnapshot(snapshot);
                }
                library.navigate(u"1"_s);
                if (scenario != 1)
                    model.prepare(library);
            }
        });
        connect(&model, &QAbstractItemModel::modelReset, &model, [&] {
            --depth;
        });
        model.prepare(library);
        QCOMPARE(maximumDepth, 1);
        QCOMPARE(model.album()[u"id"_s].toString(), u"1"_s);
        QCOMPARE(model.album()[u"code"_s], library.browseInfo()[u"code"_s]);
        QCOMPARE(model.data(model.index(0), DetailModel::TrackId).toString(), u"track1"_s);
        // Navigation alone during a reset must also be detected without a nested prepare call.
        scenario = 1;
        once = false;
        library.navigate(u"0"_s);
        model.prepare(library);
        QCOMPARE(model.album()[u"id"_s].toString(), u"1"_s);
        scenario = 2;
        once = false;
        library.navigate(u"0"_s);
        model.prepare(library);
        QCOMPARE(maximumDepth, 1);
        QCOMPARE(model.album()[u"title"_s].toString(), u"Replacement album"_s);
        QCOMPARE(model.album()[u"code"_s], library.browseInfo()[u"code"_s]);
    }

    void openingReadinessAndSingleNotification()
    {
        Fixture f;
        QSignalSpy changes(&f.controller, &PresentationController::changed);
        f.controller.open();
        QCOMPARE(changes.size(), 1);
        QCOMPARE(f.controller.phase(), u"opening"_s);
        QVERIFY(!f.controller.archiveEnabled());
        QVERIFY(f.events.isEmpty());
        f.presentationReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"opening"_s);
        f.controller.browseSurface()->finish();
        QCOMPARE(f.events, QStringList{ u"enter"_s });
        f.presentationReady = true;
        for (int i = 0; i < 120; ++i)
            f.controller.update();
        QCOMPARE(f.controller.phase(), u"opening"_s);
        f.controller.layoutReady(f.controller.layoutRevision() - 1);
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"opening"_s);
        f.controller.layoutReady(f.controller.layoutRevision());
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"detail"_s);
        const auto count = changes.size();
        f.controller.update();
        f.controller.open();
        QCOMPARE(changes.size(), count);
    }
    void latestSelectionDuringBrowseExit()
    {
        Fixture f;
        f.controller.open();
        f.controller.select(u"first"_s, true);
        f.controller.select(u"last"_s, true);
        f.controller.browseSurface()->finish();
        QCOMPARE(f.events, QStringList{ u"select:last"_s });
        QCOMPARE(f.controller.phase(), u"selecting"_s);
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"selecting"_s);
        f.archiveReady = true;
        f.controller.update();
        f.controller.browseSurface()->finish();
        QCOMPARE(f.events, (QStringList{ u"select:last"_s, u"enter"_s }));
    }
    void rapidDetailSwitchAndCancellation()
    {
        Fixture f;
        f.openDetail();
        f.events.clear();
        f.controller.select(u"a"_s, true);
        f.controller.select(u"b"_s, true);
        QCOMPARE(f.controller.phase(), u"switch-hiding"_s);
        QVERIFY(f.events.isEmpty());
        f.controller.detailSurface()->finish();
        QCOMPARE(f.events, QStringList{ u"switch:b"_s });
        f.controller.select(u"c"_s, true);
        f.presentationReady = true;
        const auto staleLayout = f.controller.layoutRevision();
        f.controller.update();
        QCOMPARE(f.events, (QStringList{ u"switch:b"_s, u"switch:c"_s }));
        QCOMPARE(f.controller.phase(), u"switching"_s);
        f.controller.layoutReady(staleLayout);
        f.presentationReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"switching"_s);
        f.controller.layoutReady(f.controller.layoutRevision());
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"detail"_s);
        f.controller.select(u"cancelled"_s, true);
        f.controller.back();
        f.controller.detailSurface()->finish();
        QCOMPARE(f.events.last(), u"return"_s);
        QVERIFY(!f.events.contains(u"switch:cancelled"_s));
        QVERIFY(!f.controller.pendingSelection());
    }
    void archiveRoute_data()
    {
        QTest::addColumn<QString>("phase");
        for (const auto &phase : { "opening", "detail", "switch-hiding", "switching" })
            QTest::newRow(phase) << QString::fromUtf8(phase);
    }
    void archiveRoute()
    {
        QFETCH(QString, phase);
        Fixture f;
        if (phase == "opening"_L1) {
            f.controller.open();
            f.controller.browseSurface()->finish();
        } else {
            f.openDetail();
        }
        if (phase.startsWith(u"switch"_s)) {
            f.controller.select(u"old"_s, true);
            if (phase == "switching"_L1)
                f.controller.detailSurface()->finish();
        }
        f.events.clear();
        f.controller.select(u"search"_s, true, true, u"track"_s);
        f.controller.detailSurface()->finish();
        QCOMPARE(f.controller.phase(), u"returning"_s);
        QCOMPARE(f.events, QStringList{ u"return"_s });
        f.interactive = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"returning"_s);
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.events, (QStringList{ u"return"_s, u"select:search"_s }));
        QCOMPARE(f.controller.phase(), u"selecting"_s);
        f.controller.back();
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"archive"_s);
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
        QCOMPARE(f.controller.phase(), u"detail"_s);
        QCOMPARE(f.events.count(u"enter"_s), 1);
        f.controller.select(u"obsolete"_s, true);
        f.controller.reset();
        f.controller.detailSurface()->finish();
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"archive"_s);
        QVERIFY(!f.events.contains(u"switch:obsolete"_s));
    }
    void reducedMotionAndSpeed()
    {
        Fixture f;
        f.controller.open();
        f.controller.setReducedMotion(true);
        QCOMPARE(f.events, QStringList{ u"enter"_s });
        f.controller.layoutReady(f.controller.layoutRevision());
        f.presentationReady = true;
        f.controller.update();
        QVERIFY(!f.controller.detailSurface()->running());
        f.controller.select(u"next"_s, true, true);
        QCOMPARE(f.controller.phase(), u"returning"_s);
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"selecting"_s);
        f.archiveReady = true;
        f.controller.update();
        QCOMPARE(f.controller.phase(), u"opening"_s);
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
    void indicatorRelayoutDoesNotAdvanceContentFade()
    {
        FrameClock clock;
        Fixture f;
        PresentationController controller(&clock, f.ports());
        clock.setActive(true);
        clock.advance(0);
        controller.revealTabContent();
        controller.setTabIndicator(30, 60, false);
        QCOMPARE(controller.tabOpacity(), .35);
        QCOMPARE(controller.tabX(), 30);
        QCOMPARE(controller.tabWidth(), 60);
        clock.advance(.04);
        const auto opacity = controller.tabOpacity();
        QVERIFY(opacity > .35 && opacity < 1);
        controller.setTabIndicator(50, 80, false);
        QCOMPARE(controller.tabOpacity(), opacity);
        QCOMPARE(controller.tabX(), 50);
        for (int i = 0; i < 4; ++i)
            clock.advance(.04);
        QCOMPARE(controller.tabOpacity(), 1);
    }
    void lookupStatusReflectsSavedResult()
    {
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        Album album;
        album.id = u"status"_s;
        album.description = u"Saved introduction"_s;
        album.introduction = { { u"status"_s, u"matched"_s } };
        album.creditsLookup = { { u"status"_s, u"matched"_s },
                                { u"matchedTracks"_s, 1 },
                                { u"totalTracks"_s, 1 } };
        snapshot.albums.append(album);
        library.setSnapshot(snapshot);
        QVERIFY(library.navigate(album.id));
        DetailModel model;
        model.prepare(library);
        QCOMPARE(model.album()[u"introductionStatus"_s].toString(),
                 QCoreApplication::translate("Yalahoo", "The introduction is saved locally.")
                         + QCoreApplication::translate(
                                 "Yalahoo", " Existing metadata is retained for offline reading."));
        QCOMPARE(model.album()[u"creditsStatus"_s].toString(),
                 QCoreApplication::translate("Yalahoo", "Track credits verified: %1 / %2.")
                         .arg(1)
                         .arg(1));
        snapshot.albums[0].introduction = { { u"status"_s, u"error"_s },
                                            { u"error"_s, u"Request failed"_s } };
        library.setSnapshot(snapshot);
        model.prepare(library);
        QVERIFY(model.album()[u"introductionStatus"_s].toString().contains(u"Request failed"_s));
        QVERIFY(!model.album()[u"introductionStatus"_s].toString().contains(u"not enabled"_s));
    }
    void offlineStatusAndTrackAttribution()
    {
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        Album a;
        a.id = "offline"_L1;
        a.offline = true;
        a.description = u"\u4fdd\u7559\u7684\u4ecb\u7ecd"_s;
        a.introduction = { { u"status"_s, u"uncertain"_s } };
        a.creditsLookup = { { u"status"_s, u"partial"_s },
                            { u"matchedTracks"_s, 1 },
                            { u"totalTracks"_s, 2 },
                            { u"retryAt"_s, u"2099-01-01T00:00:00Z"_s } };
        Track t;
        t.id = "track"_L1;
        t.title = u"\u5b9e\u9645\u66f2\u540d"_s;
        a.tracks.append(t);
        a.producers.append({ u"\u5236\u4f5c\u4eba"_s,
                             u"\u5236\u4f5c"_s,
                             u"manual"_s,
                             u"track"_s,
                             u"\u8fc7\u671f\u66f2\u540d"_s,
                             u"https://example.invalid/credit"_s,
                             { } });
        snapshot.albums.append(a);
        library.setSnapshot(snapshot);
        library.navigate(a.id);
        DetailModel model;
        model.prepare(library);
        QVERIFY(model.album()[u"offline"_s].toBool());
        QVERIFY(model.album()[u"introductionStatus"_s].toString().contains(u"\u5b58\u7591"_s));
        QVERIFY(model.album()[u"introductionStatus"_s].toString().contains(
                u"\u79bb\u7ebf\u9605\u8bfb"_s));
        QVERIFY(model.album()[u"creditsStatus"_s].toString().contains(u"1 / 2"_s));
        QVERIFY(model.album()[u"creditsStatus"_s].toString().contains(u"2099"_s));
        const auto credit = model.album()[u"producerGroups"_s].toList().first().toMap();
        const auto creditedTrack = credit[u"tracks"_s].toStringList().first();
        QVERIFY(creditedTrack.startsWith(t.title));
        QVERIFY(!creditedTrack.contains(u"\u8fc7\u671f\u66f2\u540d"_s));
        QCOMPARE(credit[u"rolesText"_s].toString(), u"\u5236\u4f5c"_s);
        QCOMPARE(credit[u"sourcesText"_s].toString(),
                 u"manual \u00b7 https://example.invalid/credit"_s);
        QCOMPARE(model.album()[u"description"_s].toString(), a.description);
        QVERIFY(library.playbackTrackId().isEmpty());
    }
    void immutableDetailAndDataFields()
    {
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        Album a;
        a.id = "a"_L1;
        a.title = u"\u533f\u540d\u6536\u85cf"_s;
        a.localNote = u"\u672c\u5730\u5907\u6ce8"_s;
        a.descriptionSource = { { u"name"_s, u"\u533f\u540d\u6765\u6e90"_s },
                                { u"url"_s, u"https://example.invalid"_s } };
        a.producers.append({ u"\u53c2\u4e0e\u4eba\u5458"_s,
                             u"\u5236\u4f5c"_s,
                             u"\u6765\u6e90"_s,
                             { },
                             { },
                             u"https://example.invalid"_s,
                             { } });
        for (int i = 0; i < 10000; ++i) {
            Track t;
            t.id = QString::number(i);
            t.albumId = a.id;
            t.title = u"\u66f2\u76ee"_s;
            t.duration = 61;
            t.discNumber = i < 5000 ? 1 : 2;
            a.tracks.append(t);
        }
        snapshot.albums.append(a);
        library.setSnapshot(snapshot);
        library.navigate(u"a"_s);
        DetailModel model;
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.prepare(library);
        QCOMPARE(model.rowCount(), 10000);
        QCOMPARE(model.data(model.index(5000), DetailModel::DiscHeading).toBool(), true);
        QCOMPARE(model.data(model.index(5001), DetailModel::DiscHeading).toBool(), false);
        QCOMPARE(model.data(model.index(9999), DetailModel::Duration).toString(), u"1:01"_s);
        QCOMPARE(model.album().value(u"localNote"_s).toString(), a.localNote);
        const auto groups = model.album().value(u"producerGroups"_s).toList();
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.first().toMap()[u"unscopedText"_s].toString(), u"\u5236\u4f5c"_s);
        model.prepare(library);
        QCOMPARE(reset.size(), 1);
        library.search(u"other"_s);
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
