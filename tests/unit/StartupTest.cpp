#include "SceneAdapter.h"
#include "FrameClock.h"
#include "LibrarySession.h"
#include "opening/OpeningController.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "StartupCoordinator.h"
#include "DemoLibrary.h"
#include "LibraryPersistence.h"
#include "LibraryStore.h"
#include "OnlineMetadataProvider.h"
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
Snapshot snapshot(const QString &title)
{
    auto value = demonstrationLibrary();
    value.albums[0].title = title;
    return std::make_shared<const LibrarySnapshot>(std::move(value));
}
struct Fixture final
{
    explicit Fixture(IMetadataProvider *provider = nullptr) : library(provider)
    {
    }
    LibrarySession library;
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OpeningController opening{ &motion, &scene, &selection };
    StartupCoordinator startup{ &library, &opening, motion.clock(), true };
};
}
class StartupTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StartupTest)
public:
    StartupTest() = default;

private Q_SLOTS:
    void persistentLibraryBindingPropagatesAndDisconnects()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        OnlineMetadataProvider provider;
        Fixture f(&provider);
        LibraryStore store(root.path());
        auto binding = std::make_unique<LibraryPersistence>(store, provider, f.library, f.startup);
        store.load();
        QTRY_VERIFY(!store.busy());
        QTRY_VERIFY(f.startup.statistics().value(u"initialReady"_s).toBool());
        QVERIFY(store.setMetadataConfiguration(
                { { u"networkEnabled"_s, false },
                  { u"musicBrainzContact"_s, u"first@example.test"_s } }));
        QCOMPARE(f.library.metadataConfiguration().value(u"musicBrainzContact"_s).toString(),
                 u"first@example.test"_s);
        const auto received = f.startup.statistics().value(u"receivedSnapshots"_s).toInteger();
        QTRY_VERIFY(!store.busy());
        // A queued enrichment wakeup belongs to the binding, as do all durable/live connections.
        Q_EMIT store.scanFinished(1, 0);
        binding.reset();
        QVERIFY(store.setMetadataConfiguration(
                { { u"musicBrainzContact"_s, u"second@example.test"_s } }));
        QCOMPARE(f.library.metadataConfiguration().value(u"musicBrainzContact"_s).toString(),
                 u"first@example.test"_s);
        QTRY_VERIFY(!store.busy());
        store.load();
        QTRY_VERIFY(!store.busy());
        QCOMPARE(f.startup.statistics().value(u"receivedSnapshots"_s).toInteger(), received);
        QVERIFY(!f.library.metadataBusy());
    }
    void initialReadinessAndLateSnapshot()
    {
        Fixture f;
        QSignalSpy applied(&f.library, &LibrarySession::snapshotChanged);
        QSignalSpy invalidated(&f.startup, &StartupCoordinator::preparationInvalidated);
        f.opening.replay();
        f.startup.scenePrepared();
        QVERIFY(f.opening.preparing());
        f.startup.receive(snapshot(u"first"_s));
        f.startup.receive(snapshot(u"latest"_s));
        QTRY_COMPARE(applied.size(), 1);
        QCOMPARE(f.library.snapshot()->albums[0].title, u"latest"_s);
        QCOMPARE(invalidated.size(), 1);
        f.startup.receive(snapshot(u"after-polish"_s));
        f.startup.scenePrepared();
        QVERIFY(f.opening.preparing());
        QCOMPARE(invalidated.size(), 2);
        QCOMPARE(f.library.snapshot()->albums[0].title, u"after-polish"_s);
        f.startup.scenePrepared();
        QVERIFY(f.opening.film());
        QCOMPARE(f.opening.time(), 0);
        f.startup.receive(snapshot(u"during-film"_s));
        QCoreApplication::sendPostedEvents();
        QCOMPARE(applied.size(), 2);
        f.opening.skip();
        QTRY_COMPARE(applied.size(), 3);
        QCOMPARE(f.library.snapshot()->albums[0].title, u"during-film"_s);
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
        QCOMPARE(f.startup.statistics()[u"pendingSnapshots"_s].toInt(), 1);
        QCOMPARE(f.startup.statistics()[u"coalescedSnapshots"_s].toInt(), 99);
        const auto frames = f.motion.clock()->frameCount();
        f.motion.clock()->advance(300);
        QCOMPARE(f.motion.clock()->frameCount(), frames);
        f.startup.setVisualActive(true);
        QTRY_COMPARE(applied.size(), 1);
        QCOMPARE(f.library.snapshot()->albums[0].title, u"99"_s);
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
        f.startup.receive(snapshot(u"initial"_s));
        QTRY_VERIFY(f.startup.statistics()[u"initialReady"_s].toBool());
        f.opening.replay();
        f.startup.scenePrepared();
        f.startup.receive(snapshot(u"pending"_s));
        f.opening.replay();
        QTRY_COMPARE(f.library.snapshot()->albums[0].title, u"pending"_s);
        auto temporary = std::make_unique<StartupCoordinator>(&f.library, &f.opening,
                                                              f.motion.clock(), false);
        temporary->receive(snapshot(u"destroyed"_s));
        temporary.reset();
        QCoreApplication::sendPostedEvents();
        QCOMPARE(f.library.snapshot()->albums[0].title, u"pending"_s);
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
    void reducedArchiveDrainsVisualDemand()
    {
        Fixture f;
        f.library.setSnapshot(demonstrationLibrary());
        f.selection.setColumns(f.library.albums()->order().columns, f.library.snapshot());
        f.scene.setSelection(&f.selection);
        connect(&f.motion, &MotionController::stateChanged, f.motion.clock(),
                &FrameClock::requestVisualFrame);
        connect(&f.scene, &SceneAdapter::frameChanged, f.motion.clock(),
                &FrameClock::requestVisualFrame);
        f.motion.setReducedMotion(true);
        f.scene.setReducedMotion(true);
        f.motion.clock()->setActive(true);
        for (int i = 0; i < 3 && f.motion.clock()->workPending(); ++i)
            f.motion.clock()->advance(1.0 / 60);
        QVERIFY(!f.motion.clock()->workPending());
        const auto revision = f.scene.frameRevision();
        f.motion.clock()->advance(1.0 / 60);
        QCOMPARE(f.scene.frameRevision(), revision);
        QVERIFY(!f.motion.clock()->workPending());
        f.selection.moveRow(1);
        QVERIFY(f.motion.clock()->workPending());
        for (int i = 0; i < 3 && f.motion.clock()->workPending(); ++i)
            f.motion.clock()->advance(1.0 / 60);
        QVERIFY(!f.motion.clock()->workPending());
        QVERIFY(f.scene.frameRevision() > revision);
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
