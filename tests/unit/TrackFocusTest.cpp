#include "DetailPorts.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "SceneAdapter.h"
#include "OfflineMetadataProvider.h"
#include <QSignalSpy>
#include <QTest>
#include <cmath>

using namespace yalahoo;
namespace {
struct Fixture
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
            album.id = QString("album-%1").arg(i);
            album.artist = i < 7 ? "Artist A" : "Artist B";
            album.title = QString::number(i);
            for (int j = 0; j < 60; ++j) {
                Track track;
                track.id = QString("track-%1").arg(j);
                track.albumId = album.id;
                track.title = QString("Song %1").arg(j);
                album.tracks.append(track);
            }
            snapshot.albums.append(album);
        }
        library.setSnapshot(snapshot);
        library.sort("artist");
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        scene.setSelection(&selection);
        library.navigate(selection.albumId());
        library.setPlaybackIds("album-0", "track-0");
        QObject::connect(&selection, &SelectionController::selectionChanged, &library, [&] {
            library.navigate(selection.albumId());
        });
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
class TrackFocusTest : public QObject
{
    Q_OBJECT
private slots:
    void lastTargetArchiveRouteAndPlaybackIsolation()
    {
        Fixture f;
        QSignalSpy revealed(&f.focus, &TrackFocusController::revealRequested);
        QSignalSpy playback(&f.library, &LibrarySession::playbackChanged);
        f.presentation.open();
        QVERIFY(f.untilDetail());
        QVERIFY(f.focus.request("album-8", "track-59"));
        f.step(12);
        QVERIFY(f.focus.request("album-3", "track-17"));
        f.step(6);
        QVERIFY(f.focus.request("album-12", "track-45"));
        QVERIFY(f.untilDetail());
        QCOMPARE(f.selection.albumId(), QString("album-12"));
        QCOMPARE(f.library.navigationTrackId(), QString("track-45"));
        QCOMPARE(revealed.size(), 1);
        QCOMPARE(revealed[0][1].toInt(), 45);
        QCOMPARE(f.library.playbackAlbumId(), QString("album-0"));
        QCOMPARE(playback.size(), 0);
        QVERIFY(!f.focus.request("missing", "track-0"));
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
        QVERIFY(f.focus.request("album-0", "track-30"));
        QCOMPARE(reveal.size(), 1);
        QCOMPARE(f.library.navigationTrackId(), QString("track-30"));
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
        QVERIFY(f.focus.request("album-0", "track-59"));
        f.focus.setReducedMotion(true);
        f.focus.beginReveal(f.focus.revision(), 0, 4000);
        QCOMPARE(f.focus.scrollY(), 4000.);
        QCOMPARE(focused.size(), 1);
        QCOMPARE(f.focus.highlight(), 1.);
        f.focus.advance(1.1);
        QVERIFY(!f.focus.active());
        QVERIFY(f.focus.request("album-0", "track-1"));
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
        panels.open("search");
        clock.advance(.05);
        QVERIFY(panels.surface()->opacity() > 0);
        panels.close();
        clock.advance(.01);
        panels.open("library");
        for (int i = 0; i < 20; ++i)
            clock.advance(.05);
        QCOMPARE(panels.panel(), QString("library"));
        QCOMPARE(closed.size(), 0);
        panels.close();
        panels.setReducedMotion(true);
        QCOMPARE(closed.size(), 1);
        QVERIFY(!panels.surface()->shown());
    }
};
QTEST_GUILESS_MAIN(TrackFocusTest)
#include "TrackFocusTest.moc"
