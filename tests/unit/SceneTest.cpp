#include "SceneAdapter.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <numbers>
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
private slots:
    void initTestCase()
    {
        QFile input(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/scene/camera-golden.json"));
        QVERIFY(input.open(QIODevice::ReadOnly));
        oracle = QJsonDocument::fromJson(input.readAll()).object();
        QCOMPARE(oracle["cases"].toArray().size(), 48);
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
        for (auto value : oracle["cases"].toArray()) {
            const auto c = value.toObject(), p = c["pose"].toObject();
            scene.layout()->setViewport(c["width"].toDouble(), c["height"].toDouble());
            scene.setDetail(c["detail"].toDouble());
            scene.snapToTarget();
            const auto &f = scene.frame();
            maxPose = std::max({ maxPose, difference(f.camera.position, vector(p["position"])),
                                 difference(f.camera.aim, vector(p["aim"])),
                                 std::abs(f.camera.span - p["span"].toDouble()) });
            QVERIFY(maxPose < 1e-6);
            QCOMPARE(scene.layout()->kind(), c["metrics"].toObject()["kind"].toString());
            QVERIFY(std::abs(scene.layout()->layoutScale()
                             - c["metrics"].toObject()["scale"].toDouble())
                    < 1e-10);
            QVERIFY(difference(f.casePosition, vector(c["casePosition"])) < 1e-10);
            QVERIFY(std::abs(f.fieldOfView - c["fov"].toDouble()) < 1e-10);
            const auto forward = f.cameraRotation.rotatedVector({ 0, 0, -1 });
            QVERIFY((forward - (f.camera.aim - f.camera.position).normalized().qt()).length()
                    < 1e-5);
            for (auto point : c["projections"].toArray()) {
                const auto q = point.toObject();
                const auto actual = scene.projectWorldPoint(vector(q["world"]).qt());
                const auto expected = q["pixel"].toArray();
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
        for (auto sequence : oracle["sequences"].toArray()) {
            SceneAdapter scene;
            scene.observe({ { -62.26, 35.98, 43.28 },
                            { -.5, 1.1, .4 },
                            2 * SceneVector{ -62.26 + .5, 35.98 - 1.1, 43.28 - .4 }.length()
                                    * std::tan(6.15 * std::numbers::pi / 360) },
                          0);
            for (auto value : sequence.toObject()["samples"].toArray()) {
                const auto c = value.toObject();
                scene.layout()->setViewport(c["width"].toDouble(), c["height"].toDouble());
                scene.setDetail(c["detail"].toDouble());
                scene.setReducedMotion(c["reduced"].toBool());
                scene.advance(c["dt"].toDouble());
                maxMotion =
                        std::max({ maxMotion,
                                   difference(scene.frame().camera.position, vector(c["position"])),
                                   difference(scene.frame().camera.aim, vector(c["aim"])) });
                maxFov = std::max(maxFov, std::abs(scene.fieldOfView() - c["fov"].toDouble()));
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
        QCOMPARE(scene.layout()->kind(), QString("cinematic"));
        QCOMPARE(scene.layout()->referenceWidth(), 1920.0);
        QCOMPARE(scene.layout()->referenceHeight(), 1080.0);
    }
    void frozenNavigationTargets()
    {
        QCOMPARE(oracle["navigation"].toArray().size(), 6);
        for (const auto runValue : oracle["navigation"].toArray()) {
            const auto run = runValue.toObject();
            SceneAdapter scene;
            scene.layout()->setViewport(run["width"].toDouble(), run["height"].toDouble(),
                                        run["coarse"].toBool());
            for (const auto sampleValue : run["samples"].toArray()) {
                const auto sample = sampleValue.toObject();
                scene.setDetail(sample["detail"].toDouble());
                scene.setNavigation(sample["laneSpeed"].toDouble(), sample["rowSpeed"].toDouble(),
                                    sample["realistic"].toBool());
                scene.setReducedMotion(sample["reduced"].toBool());
                scene.advance(sample["dt"].toDouble());
                const auto pose = scene.target();
                maxNavigation = std::max({ maxNavigation,
                                           difference(pose.position, vector(sample["position"])),
                                           difference(pose.aim, vector(sample["aim"])),
                                           std::abs(pose.span - sample["span"].toDouble()) });
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
        QCOMPARE(hit["objectId"].toString(), QString("inspection-case"));
        QCOMPARE(hit["frameRevision"].toULongLong(), scene.frameRevision());
        QVERIFY(scene.hitTest(0, 0).isEmpty());
    }
    void selectionSnapshotLifetime()
    {
        MotionController motion;
        SceneAdapter scene;
        auto selection = std::make_unique<SelectionController>(&motion);
        selection->setColumns({ { QStringLiteral("test"),
                                  QStringLiteral("Test"),
                                  { QStringLiteral("album") } } });
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
        QSaveFile output(QStringLiteral(SCENE_REPORT));
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(QJsonObject{ { "targetCases", 48 },
                                                { "navigationTargets", 36 },
                                                { "maxNavigationTargetError", maxNavigation },
                                                { "cameraSamples", 694 },
                                                { "projectionPoints", 528 },
                                                { "maxProjectionPixels", maxProjection },
                                                { "maxTargetError", maxPose },
                                                { "maxCameraPositionAimError", maxMotion },
                                                { "maxFovError", maxFov } })
                             .toJson());
        QVERIFY(output.commit());
    }
};
QTEST_GUILESS_MAIN(SceneTest)
#include "SceneTest.moc"
