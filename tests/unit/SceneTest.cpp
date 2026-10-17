#include "SceneAdapter.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <numbers>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
SceneVector vector(const QJsonValue &v)
{
    const auto a = v.toArray();
    return { a[0].toDouble(), a[1].toDouble(), a[2].toDouble() };
}
double difference(SceneVector a, SceneVector b)
{
    return (a - b).length();
}
}
class SceneTest : public QObject
{
    Q_OBJECT
    QJsonObject oracle;
    double maxProjection = 0, maxPose = 0, maxMotion = 0, maxFov = 0, maxNavigation = 0;
private Q_SLOTS:
    void initTestCase()
    {
        QFile input(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/scene/camera-golden.json"_s));
        QVERIFY(input.open(QIODevice::ReadOnly));
        oracle = QJsonDocument::fromJson(input.readAll()).object();
        QCOMPARE(oracle[u"cases"_s].toArray().size(), 48);
    }
    void fogTracksRenderedCameraAndDetail()
    {
        SceneAdapter scene;
        for (double detail : { 0.0, .25, .5, 1.0, .3, 0.0 }) {
            scene.setDetail(detail);
            // Observe an entry pose far from the target, including interrupted dolly motion.
            scene.observe({ { -62.26, 35.98, 43.28 }, { -.5, 1.1, .4 }, 12 }, 0);
            for (int step = 0; step < 20; ++step) {
                scene.advance(1.0 / 120);
                const auto &frame = scene.frame();
                const double distance = (frame.camera.position - frame.camera.aim).length();
                QVERIFY(std::abs(frame.fogNear - (distance + 5 * (1 - detail) - detail)) < 1e-12);
                QVERIFY(std::abs(frame.fogFar - (distance + 25 * (1 - detail) + 12 * detail))
                        < 1e-12);
                QVERIFY(frame.fogFar > frame.fogNear);
            }
        }
        // A detail-only change must publish fog even before the camera moves.
        const auto previous = scene.frame();
        scene.setDetail(.5);
        QVERIFY(scene.frame().revision > previous.revision);
        QCOMPARE(scene.frame().camera.position, previous.camera.position);
        QVERIFY(scene.frame().fogNear != previous.fogNear);
    }
    void frozenTargetsAndProjection()
    {
        SceneAdapter scene;
        for (auto value : oracle[u"cases"_s].toArray()) {
            const auto c = value.toObject(), p = c[u"pose"_s].toObject();
            scene.layout()->setViewport(c[u"width"_s].toDouble(), c[u"height"_s].toDouble());
            scene.setDetail(c[u"detail"_s].toDouble());
            scene.snapToTarget();
            const auto &f = scene.frame();
            maxPose = std::max({ maxPose, difference(f.camera.position, vector(p[u"position"_s])),
                                 difference(f.camera.aim, vector(p[u"aim"_s])),
                                 std::abs(f.camera.span - p[u"span"_s].toDouble()) });
            QVERIFY(maxPose < 1e-6);
            QCOMPARE(scene.layout()->kind(), c[u"metrics"_s].toObject()[u"kind"_s].toString());
            QVERIFY(std::abs(scene.layout()->layoutScale()
                             - c[u"metrics"_s].toObject()[u"scale"_s].toDouble())
                    < 1e-10);
            QVERIFY(difference(f.casePosition, vector(c[u"casePosition"_s])) < 1e-10);
            QVERIFY(std::abs(f.fieldOfView - c[u"fov"_s].toDouble()) < 1e-10);
            const auto forward = f.cameraRotation.rotatedVector({ 0, 0, -1 });
            QVERIFY((forward - (f.camera.aim - f.camera.position).normalized().qt()).length()
                    < 1e-5);
            for (auto point : c[u"projections"_s].toArray()) {
                const auto q = point.toObject();
                const auto actual = scene.projectWorldPoint(vector(q[u"world"_s]).qt());
                const auto expected = q[u"pixel"_s].toArray();
                const double error = std::hypot(actual.x() - expected[0].toDouble(),
                                                actual.y() - expected[1].toDouble());
                maxProjection = std::max(maxProjection, error);
                QVERIFY2(error <= 2, qPrintable(QString::number(error)));
            }
            const auto center =
                    scene.projectWorldPoint((f.casePosition + SceneVector{ 0, 1.85, 0 }).qt());
            QVERIFY(!scene.hitTest(center.x(), center.y()).isEmpty());
            QVERIFY(scene.hitTest(-1, -1).isEmpty());
        }
    }
    void frozenCameraInterruptions()
    {
        for (auto sequence : oracle[u"sequences"_s].toArray()) {
            SceneAdapter scene;
            scene.observe({ { -62.26, 35.98, 43.28 },
                            { -.5, 1.1, .4 },
                            2 * SceneVector{ -62.26 + .5, 35.98 - 1.1, 43.28 - .4 }.length()
                                    * std::tan(6.15 * std::numbers::pi / 360) },
                          0);
            for (auto value : sequence.toObject()[u"samples"_s].toArray()) {
                const auto c = value.toObject();
                scene.layout()->setViewport(c[u"width"_s].toDouble(), c[u"height"_s].toDouble());
                scene.setDetail(c[u"detail"_s].toDouble());
                scene.setReducedMotion(c[u"reduced"_s].toBool());
                scene.advance(c[u"dt"_s].toDouble());
                maxMotion = std::max(
                        { maxMotion,
                          difference(scene.frame().camera.position, vector(c[u"position"_s])),
                          difference(scene.frame().camera.aim, vector(c[u"aim"_s])) });
                maxFov = std::max(maxFov, std::abs(scene.fieldOfView() - c[u"fov"_s].toDouble()));
                QVERIFY(maxMotion < 1e-6);
                QVERIFY(maxFov < 1e-6);
            }
        }
    }
    void atomicLayoutAndClock()
    {
        FrameClock clock;
        SceneAdapter scene(&clock);
        QSignalSpy changed(&scene, &SceneAdapter::frameChanged);
        const auto position = scene.frame().camera.position;
        scene.layout()->setViewport(944, 900);
        QVERIFY(scene.layout()->portrait());
        QVERIFY(scene.frame().portrait);
        QVERIFY(changed.count() > 0);
        QVERIFY(difference(scene.frame().camera.position, position)
                == 0); // resize changes aspect immediately, retains dolly state
        scene.layout()->setViewport(945, 900);
        QVERIFY(!scene.layout()->portrait());
        QVERIFY(!scene.frame().portrait);
        scene.setDetail(1);
        clock.setActive(true);
        clock.advance(1.0 / 60);
        clock.advance(1.0 / 60);
        QVERIFY(difference(scene.frame().camera.position, position) > 0);
        const auto frame = scene.frameRevision();
        clock.setActive(false);
        clock.advance(.2);
        QCOMPARE(scene.frameRevision(), frame);
        scene.setReducedMotion(true);
        QVERIFY(scene.settled());
        scene.layout()->setViewport(390, 844);
        QVERIFY(scene.settled());
        scene.layout()->setViewport(0, -1);
        QCOMPARE(scene.layout()->width(), 1.0);
        QCOMPARE(scene.layout()->height(), 1.0);
        scene.layout()->setViewport(1920, 1080, false, true);
        QCOMPARE(scene.layout()->kind(), u"cinematic"_s);
        QCOMPARE(scene.layout()->referenceWidth(), 1920.0);
        QCOMPARE(scene.layout()->referenceHeight(), 1080.0);
    }
    void frozenNavigationTargets()
    {
        QCOMPARE(oracle[u"navigation"_s].toArray().size(), 6);
        for (const auto runValue : oracle[u"navigation"_s].toArray()) {
            const auto run = runValue.toObject();
            SceneAdapter scene;
            scene.layout()->setViewport(run[u"width"_s].toDouble(), run[u"height"_s].toDouble(),
                                        run[u"coarse"_s].toBool());
            for (const auto sampleValue : run[u"samples"_s].toArray()) {
                const auto sample = sampleValue.toObject();
                scene.setDetail(sample[u"detail"_s].toDouble());
                scene.setNavigation(sample[u"laneSpeed"_s].toDouble(),
                                    sample[u"rowSpeed"_s].toDouble(),
                                    sample[u"realistic"_s].toBool());
                scene.setReducedMotion(sample[u"reduced"_s].toBool());
                scene.advance(sample[u"dt"_s].toDouble());
                const auto pose = scene.target();
                maxNavigation = std::max({ maxNavigation,
                                           difference(pose.position, vector(sample[u"position"_s])),
                                           difference(pose.aim, vector(sample[u"aim"_s])),
                                           std::abs(pose.span - sample[u"span"_s].toDouble()) });
                QVERIFY(maxNavigation <= 1e-6);
            }
        }
    }
    void navigationAndPicking()
    {
        SceneAdapter scene;
        const auto stable = scene.target();
        scene.setNavigation(3, 4, true);
        scene.advance(.05);
        QVERIFY(scene.target().position.y == stable.position.y); // real columns suppress row orbit
        scene.setNavigation(0, 4, false);
        scene.advance(.05);
        QVERIFY(scene.target().position.y != stable.position.y);
        scene.setReducedMotion(true);
        scene.setDetail(1);
        const auto point = scene.projectWorldPoint(
                (scene.frame().casePosition + SceneVector{ .17, 1.85, .07 }).qt());
        const auto hit = scene.hitTest(point.x(), point.y());
        QCOMPARE(hit[u"objectId"_s].toString(), u"inspection-case"_s);
        QCOMPARE(hit[u"frameRevision"_s].toULongLong(), scene.frameRevision());
        QVERIFY(scene.hitTest(0, 0).isEmpty());
    }
    void selectionSnapshotLifetime()
    {
        MotionController motion;
        SceneAdapter scene;
        auto selection = std::make_unique<SelectionController>(&motion);
        selection->setColumns({ { u"test"_s, u"Test"_s, { u"album"_s } } });
        scene.setSelection(selection.get());
        QVERIFY(scene.frame().hasArchivePool);
        QVERIFY(!scene.frame().instances.isEmpty());

        // Removing the pool must publish its empty snapshot, including when its
        // selected pose is visually indistinguishable from the default single case.
        const auto revision = scene.frameRevision();
        scene.setSelection(nullptr);
        QVERIFY(scene.frameRevision() > revision);
        QVERIFY(!scene.frame().hasArchivePool);
        QVERIFY(scene.frame().instances.isEmpty());

        scene.setSelection(selection.get());
        QVERIFY(scene.frame().hasArchivePool);
        selection.reset();
        QVERIFY(!scene.selection());
        QVERIFY(!scene.frame().hasArchivePool);
        QVERIFY(scene.frame().instances.isEmpty());
    }
    void cleanupTestCase()
    {
        QSaveFile output(QString::fromUtf8(SCENE_REPORT));
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(QJsonObject{ { u"targetCases"_s, 48 },
                                                { u"navigationTargets"_s, 36 },
                                                { u"maxNavigationTargetError"_s, maxNavigation },
                                                { u"cameraSamples"_s, 694 },
                                                { u"projectionPoints"_s, 528 },
                                                { u"maxProjectionPixels"_s, maxProjection },
                                                { u"maxTargetError"_s, maxPose },
                                                { u"maxCameraPositionAimError"_s, maxMotion },
                                                { u"maxFovError"_s, maxFov } })
                             .toJson());
        QVERIFY(output.commit());
    }
};
QTEST_GUILESS_MAIN(SceneTest)
#include "SceneTest.moc"
