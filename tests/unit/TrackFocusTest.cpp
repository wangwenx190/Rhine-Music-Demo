#include "../support/LibraryFixture.h"
#include "FrameClock.h"
#include "LibrarySession.h"
#include "PresentationController.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "DetailPorts.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "SceneAdapter.h"
#include "OfflineMetadataProvider.h"
#include <QSignalSpy>
#include <QTest>
#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
struct Fixture final
{
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OfflineMetadataProvider provider;
    LibrarySession library{ &provider };
    PresentationController presentation{ motion.clock(), detailPorts(scene, selection, library) };
    TrackFocusController focus{ &library, &presentation, motion.clock() };
    Fixture()
    {
        LibrarySnapshot snapshot;
        for (int i = 0; i < 15; ++i) {
            Album album;
            album.id = u"album-%1"_s.arg(i);
            album.artist = i < 7 ? "Artist A"_L1 : "Artist B"_L1;
            album.title = QString::number(i);
            for (int j = 0; j < 60; ++j) {
                Track track;
                track.id = u"track-%1"_s.arg(j);
                track.albumId = album.id;
                track.title = u"Song %1"_s.arg(j);
                album.tracks.append(track);
            }
            snapshot.albums.append(album);
        }
        library.setSnapshot(snapshot);
        library.sort(u"artist"_s);
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        test::bindLibrarySelection(library, selection, scene);
        library.setPlaybackIds(u"album-0"_s, u"track-0"_s);
        QObject::connect(&presentation, &PresentationController::prepared, &presentation, [&] {
            presentation.layoutReady(presentation.layoutRevision());
        });
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
    }
    void step(int frames = 1)
    {
        for (int i = 0; i < frames; ++i)
            motion.clock()->advance(1. / 120);
    }
    bool untilDetail()
    {
        for (int i = 0; i < 3600 && !presentation.detailEnabled(); ++i)
            step();
        return presentation.detailEnabled();
    }
};
}
class TrackFocusTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackFocusTest)
public:
    TrackFocusTest() = default;

private Q_SLOTS:
    void synchronousRequestNotifications_data()
    {
        QTest::addColumn<int>("stage");
        QTest::addColumn<bool>("replace");
        for (int stage = 0; stage < 3; ++stage)
            for (bool replace : { false, true })
                QTest::newRow(qPrintable(QString::number(stage)
                                         + (replace ? u"-replace"_s : u"-cancel"_s)))
                        << stage << replace;
    }
    void synchronousRequestNotifications()
    {
        QFETCH(int, stage);
        QFETCH(bool, replace);
        Fixture f;
        bool acted = false;
        const auto intervene = [&] {
            if (acted)
                return;
            acted = true;
            if (replace)
                QVERIFY(f.focus.request(u"album-3"_s, u"track-17"_s));
            else
                f.focus.cancel();
        };
        if (stage == 1)
            connect(&f.focus, &TrackFocusController::navigationRequested, &f.focus, intervene);
        else
            connect(&f.focus, &TrackFocusController::changed, &f.focus, [&] {
                if (stage == 0 || f.focus.active())
                    intervene();
            });
        QVERIFY(!f.focus.request(u"album-8"_s, u"track-59"_s));
        QVERIFY(acted);
        if (replace) {
            QVERIFY(f.untilDetail());
            QCOMPARE(f.selection.albumId(), u"album-3"_s);
            QCOMPARE(f.library.navigationTrackId(), u"track-17"_s);
        } else {
            f.step(120);
            QCOMPARE(f.presentation.phase(), u"archive"_s);
            QVERIFY(!f.focus.active());
        }
        QCOMPARE(f.library.playbackAlbumId(), u"album-0"_s);
    }
    void reducedMotionDuringHighlight_data()
    {
        QTest::addColumn<double>("elapsed");
        QTest::newRow("start") << 0.;
        QTest::newRow("fade-in") << .1;
        QTest::newRow("late-pulse") << 1.5;
    }
    void reducedMotionDuringHighlight()
    {
        QFETCH(double, elapsed);
        Fixture f;
        QVERIFY(f.focus.locatePlayback());
        QVERIFY(f.untilDetail());
        f.focus.beginReveal(f.focus.revision(), 0, 0);
        f.focus.advance(elapsed);
        QVERIFY(f.focus.highlight() < 1);
        QSignalSpy changed(&f.focus, &TrackFocusController::changed);
        f.focus.setReducedMotion(true);
        QCOMPARE(f.focus.highlight(), 1.);
        QCOMPARE(changed.size(), 1);
        f.focus.setReducedMotion(true);
        QCOMPARE(changed.size(), 1);
        f.focus.advance(1.);
        QVERIFY(f.focus.active());
        QCOMPARE(f.focus.highlight(), 1.);
        f.focus.advance(.11);
        QVERIFY(!f.focus.active());
    }
    void listOriginRefinementPreservesMotionDistance()
    {
        Fixture f;
        QVERIFY(f.focus.request(u"album-0"_s, u"track-10"_s));
        QVERIFY(f.untilDetail());
        const auto revision = f.focus.revision();
        f.focus.beginListReveal(revision, 10, 100, 0, 500, 400, 40, 200, 20);
        QCOMPARE(f.focus.listScrollY(100), 110.);
        f.focus.advance(.1);
        const double scroll = f.focus.listScrollY(100);
        f.focus.refineListReveal(150, 450, 40, 200, 20, 500);
        QCOMPARE(f.focus.listScrollY(150), scroll + 50);
        f.focus.refineListReveal(150, 470, 40, 200, 20, 500);
        QCOMPARE(f.focus.listScrollY(150), scroll + 70);
        // An unavailable final delegate is distinct from invalid numeric geometry.
        f.focus.refineListReveal(150, 470, 40, 200, 20, QVariant());
        QCOMPARE(f.focus.listScrollY(150), scroll + 70);
        for (double invalid :
             { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity() }) {
            f.focus.refineListReveal(150, 999, 40, 200, 20, invalid);
            QCOMPARE(f.focus.listScrollY(150), scroll + 70);
        }
        f.focus.advance(1);
        QCOMPARE(f.focus.listScrollY(150), 380.);
        f.focus.cancel();
        const auto final = f.focus.listScrollY(150);
        f.focus.beginListReveal(revision, 0, 0, 0, 500, 0, 0, 200, 0);
        QCOMPARE(f.focus.listScrollY(150), final);
    }
    void synchronousScrollCancellationCannotRestoreHighlight()
    {
        Fixture f;
        QVERIFY(f.focus.request(u"album-0"_s, u"track-10"_s));
        QVERIFY(f.untilDetail());
        f.focus.beginReveal(f.focus.revision(), 0, 1000);
        QSignalSpy focused(&f.focus, &TrackFocusController::focusRequested);
        connect(&f.focus, &TrackFocusController::scrollChanged, &f.focus,
                &TrackFocusController::cancel);
        f.focus.advance(1);
        QVERIFY(!f.focus.active());
        QVERIFY(!f.focus.revealing());
        QCOMPARE(focused.size(), 0);
        f.focus.advance(1);
        QCOMPARE(focused.size(), 0);
    }
    void synchronousHighlightCancellationCannotFocusOldRow()
    {
        Fixture f;
        QVERIFY(f.focus.request(u"album-0"_s, u"track-10"_s));
        QVERIFY(f.untilDetail());
        f.focus.beginReveal(f.focus.revision(), 0, 1000);
        QSignalSpy focused(&f.focus, &TrackFocusController::focusRequested);
        connect(&f.focus, &TrackFocusController::changed, &f.focus, [&] {
            if (f.focus.highlight() == 0 && f.focus.revealing())
                f.focus.cancel();
        });
        f.focus.advance(1);
        QVERIFY(!f.focus.active());
        QVERIFY(!f.focus.revealing());
        QCOMPARE(focused.size(), 0);
    }
    void lastTargetArchiveRouteAndPlaybackIsolation()
    {
        Fixture f;
        QSignalSpy revealed(&f.focus, &TrackFocusController::revealRequested);
        QSignalSpy playback(&f.library, &LibrarySession::playbackChanged);
        f.presentation.open();
        QVERIFY(f.untilDetail());
        QVERIFY(f.focus.request(u"album-8"_s, u"track-59"_s));
        f.step(12);
        QVERIFY(f.focus.request(u"album-3"_s, u"track-17"_s));
        f.step(6);
        QVERIFY(f.focus.request(u"album-12"_s, u"track-45"_s));
        QVERIFY(f.untilDetail());
        QCOMPARE(f.selection.albumId(), u"album-12"_s);
        QCOMPARE(f.library.navigationTrackId(), u"track-45"_s);
        QCOMPARE(revealed.size(), 1);
        QCOMPARE(revealed[0][1].toInt(), 45);
        QCOMPARE(f.library.playbackAlbumId(), u"album-0"_s);
        QCOMPARE(playback.size(), 0);
        QVERIFY(!f.focus.request(u"missing"_s, u"track-0"_s));
        QVERIFY(!f.focus.active());
    }
    void reuseScrollCancelAndReducedMotion()
    {
        Fixture f;
        QVERIFY(f.focus.locatePlayback());
        QVERIFY(f.untilDetail());
        const auto layout = f.presentation.layoutRevision();
        const auto sceneRevision = f.selection.revision();
        QSignalSpy reveal(&f.focus, &TrackFocusController::revealRequested);
        QSignalSpy focused(&f.focus, &TrackFocusController::focusRequested);
        QVERIFY(f.focus.request(u"album-0"_s, u"track-30"_s));
        QCOMPARE(reveal.size(), 1);
        QCOMPARE(f.library.navigationTrackId(), u"track-30"_s);
        QCOMPARE(f.presentation.layoutRevision(), layout);
        QCOMPARE(f.selection.revision(), sceneRevision);
        const auto revision = f.focus.revision();
        f.focus.beginReveal(revision, 100, 1100);
        f.focus.advance(.11);
        QVERIFY(std::abs(f.focus.scrollY() - 162.5) < 1e-6);
        f.focus.advance(.22);
        QVERIFY(std::abs(f.focus.scrollY() - 1037.5) < 1e-6);
        f.focus.cancel();
        f.focus.advance(1);
        QCOMPARE(focused.size(), 0);
        f.focus.beginReveal(revision, 0, 2000);
        QCOMPARE(focused.size(), 0);
        QVERIFY(f.focus.request(u"album-0"_s, u"track-59"_s));
        f.focus.setReducedMotion(true);
        f.focus.beginReveal(f.focus.revision(), 0, 4000);
        QCOMPARE(f.focus.scrollY(), 4000.);
        QCOMPARE(focused.size(), 1);
        QCOMPARE(f.focus.highlight(), 1.);
        f.focus.advance(1.1);
        QVERIFY(!f.focus.active());
        QVERIFY(f.focus.request(u"album-0"_s, u"track-1"_s));
        f.presentation.back();
        QVERIFY(!f.focus.active());
        f.library.setSnapshot({ });
        QVERIFY(!f.focus.locatePlayback());
    }
    void panelsReverseAndUseOnlySharedClock()
    {
        FrameClock clock;
        PanelController panels(&clock);
        QSignalSpy closed(&panels, &PanelController::closed);
        clock.setActive(true);
        clock.advance(0);
        panels.open(u"search"_s);
        clock.advance(.05);
        QVERIFY(panels.surface()->opacity() > 0);
        panels.close();
        clock.advance(.01);
        panels.open(u"library"_s);
        for (int i = 0; i < 20; ++i)
            clock.advance(.05);
        QCOMPARE(panels.panel(), u"library"_s);
        QCOMPARE(closed.size(), 0);
        panels.close();
        panels.setReducedMotion(true);
        QCOMPARE(closed.size(), 1);
        QVERIFY(!panels.surface()->shown());
    }
    void panelResultWaitsForExitAndKeepsLastIntent()
    {
        FrameClock clock;
        PanelController panels(&clock);
        QSignalSpy result(&panels, &PanelController::resultRequested);
        panels.open(u"search"_s);
        panels.surface()->finish();
        panels.selectResult(u"first"_s, u"old-track"_s);
        QVERIFY(result.isEmpty());
        panels.selectResult(u"last"_s, u"new-track"_s);
        panels.surface()->finish();
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first(), QVariantList({ u"last"_s, u"new-track"_s }));
        QVERIFY(panels.panel().isEmpty());
        QVERIFY(!panels.surface()->shown());

        panels.open(u"search"_s);
        panels.selectResult(u"cancelled"_s, { });
        panels.open(u"library"_s);
        panels.surface()->finish();
        QCOMPARE(result.size(), 1);
        QCOMPARE(panels.panel(), u"library"_s);
        panels.selectResult(u"wrong-panel"_s, { });
        QVERIFY(panels.surface()->shown());

        panels.open(u"search"_s);
        panels.selectResult(u"dismissed"_s, { });
        panels.close();
        panels.surface()->finish();
        QCOMPARE(result.size(), 1);
        panels.setReducedMotion(true);
        panels.open(u"search"_s);
        panels.selectResult(u"immediate"_s, { });
        QCOMPARE(result.size(), 2);
        panels.setReducedMotion(false);
        panels.open(u"search"_s);
        panels.selectResult(u"reset"_s, { });
        panels.reset();
        QVERIFY(!panels.surface()->shown());
        QCOMPARE(result.size(), 2);
    }
    void panelSignalsCannotResurrectOrNavigateObsoleteIntent()
    {
        FrameClock clock;
        PanelController panels(&clock);
        QSignalSpy result(&panels, &PanelController::resultRequested);
        const auto dismiss = connect(&panels, &PanelController::changed, &panels, [&] {
            if (panels.panel() == "search"_L1)
                panels.close();
        });
        panels.open(u"search"_s);
        QVERIFY(panels.panel().isEmpty());
        QVERIFY(!panels.surface()->shown());
        disconnect(dismiss);

        panels.open(u"search"_s);
        panels.surface()->finish();
        connect(&panels, &PanelController::closed, &panels, [&] {
            panels.open(u"settings"_s);
        });
        panels.selectResult(u"obsolete"_s, u"track"_s);
        panels.surface()->finish();
        QVERIFY(result.isEmpty());
        QCOMPARE(panels.panel(), u"settings"_s);
        QVERIFY(panels.surface()->shown());
    }
};
QTEST_GUILESS_MAIN(TrackFocusTest)
#include "TrackFocusTest.moc"
