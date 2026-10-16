#include "StartupCoordinator.h"
#include "DemoLibrary.h"
#include <QTest>
#include <QSignalSpy>
using namespace yalahoo;
namespace {
Snapshot snapshot(const QString &title)
{
    auto value = demonstrationLibrary();
    value.albums[0].title = title;
    return std::make_shared<const LibrarySnapshot>(std::move(value));
}
struct Fixture
{
    LibrarySession library{ nullptr };
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OpeningController opening{ &motion, &scene, &selection };
    StartupCoordinator startup{ &library, &opening, motion.clock(), true };
};
}
class StartupTest : public QObject
{
    Q_OBJECT
private slots:
    void initialReadinessAndLateSnapshot()
    {
        Fixture f;
        QSignalSpy applied(&f.library, &LibrarySession::snapshotChanged);
        QSignalSpy invalidated(&f.startup, &StartupCoordinator::preparationInvalidated);
        f.opening.replay();
        f.startup.scenePrepared();
        QVERIFY(f.opening.preparing());
        f.startup.receive(snapshot("first"));
        f.startup.receive(snapshot("latest"));
        QTRY_COMPARE(applied.size(), 1);
        QCOMPARE(f.library.snapshot()->albums[0].title, "latest");
        QCOMPARE(invalidated.size(), 1);
        f.startup.receive(snapshot("after-polish"));
        f.startup.scenePrepared();
        QVERIFY(f.opening.preparing());
        QCOMPARE(invalidated.size(), 2);
        QCOMPARE(f.library.snapshot()->albums[0].title, "after-polish");
        f.startup.scenePrepared();
        QVERIFY(f.opening.film());
        QCOMPARE(f.opening.time(), 0);
        f.startup.receive(snapshot("during-film"));
        QCoreApplication::sendPostedEvents();
        QCOMPARE(applied.size(), 2);
        f.opening.skip();
        QTRY_COMPARE(applied.size(), 3);
        QCOMPARE(f.library.snapshot()->albums[0].title, "during-film");
    }
    void hiddenCoalescingAndResumeInterval()
    {
        Fixture f;
        f.opening.replay();
        f.motion.clock()->setActive(true);
        f.motion.clock()->advance(0);
        f.startup.setVisualActive(false);
        QSignalSpy applied(&f.library, &LibrarySession::snapshotChanged);
        for (int i = 0; i < 100; ++i)
            f.startup.receive(snapshot(QString::number(i)));
        QCoreApplication::sendPostedEvents();
        QCOMPARE(applied.size(), 0);
        QCOMPARE(f.startup.statistics()["pendingSnapshots"].toInt(), 1);
        QCOMPARE(f.startup.statistics()["coalescedSnapshots"].toInt(), 99);
        const auto frames = f.motion.clock()->frameCount();
        f.motion.clock()->advance(300);
        QCOMPARE(f.motion.clock()->frameCount(), frames);
        f.startup.setVisualActive(true);
        QTRY_COMPARE(applied.size(), 1);
        QCOMPARE(f.library.snapshot()->albums[0].title, "99");
        f.motion.clock()->setActive(true);
        QSignalSpy intervals(f.motion.clock(), &FrameClock::frame);
        f.motion.clock()->advance(300);
        QCOMPARE(intervals[0][0].toDouble(), 0);
        f.startup.scenePrepared();
        QVERIFY(f.opening.film());
    }
    void replayAndQueuedDestruction()
    {
        Fixture f;
        f.startup.receive(snapshot("initial"));
        QTRY_VERIFY(f.startup.statistics()["initialReady"].toBool());
        f.opening.replay();
        f.startup.scenePrepared();
        f.startup.receive(snapshot("pending"));
        f.opening.replay();
        QTRY_COMPARE(f.library.snapshot()->albums[0].title, "pending");
        auto temporary = std::make_unique<StartupCoordinator>(&f.library, &f.opening,
                                                              f.motion.clock(), false);
        temporary->receive(snapshot("destroyed"));
        temporary.reset();
        QCoreApplication::sendPostedEvents();
        QCOMPARE(f.library.snapshot()->albums[0].title, "pending");
    }
    void openingCompletionInvalidationSurvivesSleep()
    {
        FrameClock clock;
        clock.setActive(true);
        clock.advance(0);
        clock.startOpening();
        connect(&clock, &FrameClock::openingFinished, &clock, &FrameClock::requestVisualFrame);
        for (int i = 0; i < 232; ++i)
            clock.advance(.05);
        QVERIFY(!clock.openingRunning());
        QVERIFY(clock.workPending());
    }
    void invalidationDuringFrameSurvivesSleep()
    {
        FrameClock clock;
        clock.setActive(true);
        clock.advance(0);
        QVERIFY(!clock.workPending());
        clock.requestVisualFrame();
        QVERIFY(clock.workPending());
        auto connection = connect(&clock, &FrameClock::frame, &clock, [&] {
            clock.requestVisualFrame();
        });
        clock.advance(.016);
        QVERIFY(clock.workPending());
        disconnect(connection);
        clock.advance(.016);
        QVERIFY(!clock.workPending());
    }
};
QTEST_GUILESS_MAIN(StartupTest)
#include "StartupTest.moc"
