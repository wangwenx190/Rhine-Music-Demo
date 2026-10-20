#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "OpeningController.h"
#include "OpeningInk.h"
#include "BrandPaths.h"
#include "MotionFunctions.h"
#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <limits>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
void near(double actual, double expected)
{
    QVERIFY2(std::abs(actual - expected) <= 1e-6,
             qPrintable(
                     u"actual %1 expected %2"_s.arg(actual, 0, 'g', 17).arg(expected, 0, 'g', 17)));
}
struct Fixture final
{
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OpeningController opening{ &motion, &scene, &selection };
    Fixture()
    {
        selection.setColumns({ { u"column"_s, u"column"_s, { u"album-a"_s, u"album-b"_s } } });
        scene.setSelection(&selection);
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
    }
};
}
class OpeningTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OpeningTest)
public:
    OpeningTest() = default;

private Q_SLOTS:
    void sourceFormulaAndCamera()
    {
        QFile file((QString::fromUtf8(FIXTURE_ROOT))
                   + "/tests/fixtures/opening/source-golden.json"_L1);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto source = QJsonDocument::fromJson(file.readAll()).object();
        for (const auto &value : source[u"frames"_s].toArray()) {
            const auto row = value.toObject();
            const auto actual = openingFrame(row[u"time"_s].toDouble()).values();
            for (auto it = actual.cbegin(); it != actual.cend(); ++it) {
                if (it.key() == "phase"_L1)
                    QCOMPARE(it.value().toString(), row[it.key()].toString());
                else
                    near(it.value().toDouble(), row[it.key()].toDouble());
            }
            near(openingSceneTime(row[u"time"_s].toDouble()), row[u"sceneTime"_s].toDouble());
        }
        for (const auto &value : source[u"poses"_s].toArray()) {
            const auto row = value.toObject();
            const auto selected = row[u"selected"_s].toArray();
            const auto pose = openingCamera(
                    row[u"shot"_s].toDouble(), row[u"width"_s].toDouble(),
                    row[u"height"_s].toDouble(),
                    { selected[0].toDouble(), selected[1].toDouble(), selected[2].toDouble() });
            const auto p = row[u"position"_s].toArray(), a = row[u"aim"_s].toArray();
            near(pose.position.x, p[0].toDouble());
            near(pose.position.y, p[1].toDouble());
            near(pose.position.z, p[2].toDouble());
            near(pose.aim.x, a[0].toDouble());
            near(pose.aim.y, a[1].toDouble());
            near(pose.aim.z, a[2].toDouble());
            near(pose.span, row[u"span"_s].toDouble());
            Fixture f;
            f.scene.layout()->setViewport(row[u"width"_s].toDouble(), row[u"height"_s].toDouble());
            f.selection.setOpeningTime(row[u"shot"_s].toDouble());
            const auto position = f.selection.selectedPosition();
            near(position.x, selected[0].toDouble());
            near(position.y, selected[1].toDouble());
            near(position.z, selected[2].toDouble());
        }
        QCOMPARE(openingFrame(std::numeric_limits<double>::quiet_NaN()).values(),
                 openingFrame(0).values());
    }
    void ratesReadinessAndHandoff()
    {
        for (double fps : { 20, 30, 60, 120 }) {
            for (double speed : { .25, 1.0, 3.0 }) {
                Fixture f;
                f.motion.clock()->setSpeed(speed);
                QSignalSpy handed(&f.opening, &OpeningController::handedOff),
                        completed(&f.opening, &OpeningController::completed);
                f.opening.replay();
                for (int i = 0; i < 10; ++i)
                    f.motion.clock()->advance(.05);
                QCOMPARE(f.opening.time(), 0);
                QVERIFY(f.opening.preparing());
                f.opening.scenePrepared();
                f.opening.scenePrepared();
                const int frames = qRound(11.6 * fps);
                for (int i = 0; i < frames; ++i) {
                    f.motion.clock()->advance(1 / fps);
                    near(f.opening.time(), std::min(11.6, (i + 1) / fps));
                }
                QCOMPARE(handed.size(), 1);
                QVERIFY(f.opening.active());
                QVERIFY(!f.opening.sceneOccluded());
                const auto endpoint = f.scene.frame().camera;
                const auto target = f.scene.target();
                near((endpoint.position - target.position).length(), 0);
                near((endpoint.aim - target.aim).length(), 0);
                for (int i = 0; i < int(std::ceil(.72 * fps)); ++i)
                    f.motion.clock()->advance(1 / fps);
                QVERIFY(!f.opening.active());
                QCOMPARE(completed.size(), 1);
                f.opening.skip();
                f.motion.clock()->advance(.05);
                QCOMPARE(handed.size(), 1);
                QCOMPARE(completed.size(), 1);
            }
        }
    }
    void interruptionAndSuspension()
    {
        Fixture f;
        QSignalSpy handed(&f.opening, &OpeningController::handedOff),
                completed(&f.opening, &OpeningController::completed);
        f.opening.replay();
        f.opening.skip();
        QVERIFY(f.opening.preparing());
        f.opening.scenePrepared();
        QCOMPARE(handed.size(), 1);
        f.opening.replay();
        f.opening.scenePrepared();
        for (int i = 0; i < 60; ++i)
            f.motion.clock()->advance(1.0 / 60);
        const double paused = f.opening.time();
        f.motion.clock()->setActive(false);
        f.motion.clock()->advance(20);
        near(f.opening.time(), paused);
        f.motion.clock()->setActive(true);
        f.motion.clock()->advance(20);
        near(f.opening.time(), paused);
        f.motion.setReducedMotion(true);
        QVERIFY(!f.opening.active());
        QCOMPARE(handed.size(), 2);
        QCOMPARE(completed.size(), 1);
        f.opening.replay();
        f.opening.scenePrepared();
        QVERIFY(!f.opening.active());
        QCOMPARE(handed.size(), 3);
        QCOMPARE(completed.size(), 2);
    }
    void reentrantReplayCannotBeCompletedByOldHandoff()
    {
        Fixture f;
        f.motion.setReducedMotion(true);
        QSignalSpy completed(&f.opening, &OpeningController::completed);
        bool restarted = false;
        connect(&f.opening, &OpeningController::handedOff, &f.opening, [&] {
            if (!restarted) {
                restarted = true;
                f.opening.replay();
            }
        });
        f.opening.replay();
        const auto oldRevision = f.opening.revision();
        f.opening.scenePrepared();
        QVERIFY(restarted);
        QVERIFY(f.opening.revision() > oldRevision);
        QVERIFY(f.opening.preparing());
        QCOMPARE(completed.size(), 0);
        f.opening.scenePrepared();
        QVERIFY(!f.opening.active());
        QCOMPARE(completed.size(), 1);
    }
    void changedCallbackReplayRejectsObsoleteCompletion()
    {
        Fixture f;
        f.motion.setReducedMotion(true);
        QSignalSpy completed(&f.opening, &OpeningController::completed);
        bool restarted = false;
        connect(&f.opening, &OpeningController::changed, &f.opening, [&] {
            if (!restarted && !f.opening.active()) {
                restarted = true;
                f.opening.replay();
            }
        });
        f.opening.replay();
        f.opening.scenePrepared();
        QVERIFY(restarted);
        QVERIFY(f.opening.preparing());
        QCOMPARE(completed.size(), 0);
    }
    void brandArcLengthsAndReveal()
    {
        const auto paths = loadOpeningPaths();
        QCOMPARE(paths.size(), 19);
        int centerlines = 0;
        double last = 0;
        for (const auto &path : paths) {
            if (path.decoration)
                continue;
            ++centerlines;
            near(path.start, last);
            last = path.end;
            const double length = path.centerline.length();
            QVERIFY(length > 0);
            const auto half = path.centerline.trimmed(0, .5);
            QVERIFY(std::abs(half.length() - length / 2) < .1);
        }
        QCOMPARE(centerlines, 17);
        near(last, 1);
        for (double delay : { 0.0, .045, .06, .09, .11, .14 }) {
            QCOMPARE(openingReveal(delay, delay), 0);
            QCOMPARE(openingReveal(.72, delay), 1);
            QVERIFY(openingReveal(.5, delay) > 0 && openingReveal(.5, delay) < 1);
        }
    }
};
QTEST_GUILESS_MAIN(OpeningTest)
#include "OpeningTest.moc"
