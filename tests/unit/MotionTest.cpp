// Checks exact spring/quintic motion against independent source samples and interruption
// invariants. Explicit fixture literals are independent inputs or expected results; timing
// bounds have named constants.

#include "FrameClock.h"
#include "MotionController.h"
#include "MotionFunctions.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
constexpr std::array<QLatin1StringView, 4> keys{ "rail"_L1, "column"_L1, "shoulder"_L1, "lane"_L1 };
QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Missing oracle %s", qPrintable(path));
    QJsonParseError error;
    const auto result = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        qFatal("Malformed oracle");
    return result.object();
}
}
class MotionTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MotionTest)
public:
    MotionTest() = default;

private:
    void near(double actual, double expected)
    {
        const double error = std::abs(actual - expected);
        QVERIFY2(std::isfinite(actual) && error <= 1e-6,
                 qPrintable(u"actual %1 expected %2 error %3"_s.arg(actual, 0, 'g', 17)
                                    .arg(expected, 0, 'g', 17)
                                    .arg(error, 0, 'g', 17)));
    }
    void compareState(Kinematics state, const QJsonObject &expected)
    {
        near(state.value, expected.value(u"value"_s).toDouble());
        near(state.velocity, expected.value(u"velocity"_s).toDouble());
        near(state.acceleration, expected.value(u"acceleration"_s).toDouble());
    }
    void compareArchive(const ArchiveMotion &motion, const QJsonObject &expected)
    {
        const auto state = motion.state();
        for (size_t i = 0; i < 4; ++i)
            compareState(state[i], expected.value(keys[i]).toObject());
    }
private Q_SLOTS:
    void frozenNonzeroAndVariableFrames()
    {
        const auto oracle = readJson(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/motion/kernel-golden.json"_s));
        for (const auto caseValue : oracle.value(u"cases"_s).toArray()) {
            const auto testCase = caseValue.toObject();
            ArchiveSprings springs{ };
            const ArchiveValues initialValues{ 2, -1, 4, .5 }, initialVelocity{ .2, -.3, .4, -.5 };
            for (size_t i = 0; i < 4; ++i) {
                springs[i].value = initialValues[i];
                springs[i].velocity = initialVelocity[i];
            }
            ArchiveMotion archive;
            archive.observe(springs, { 3, 2, 6, 1 }, { 3.7, 3.7, 9, 6.5 });
            compareArchive(archive, testCase.value(u"initial"_s).toObject());
            archive.retarget(springs, { 7.44, 15.6, 12, 3 });
            ColumnScalar scalar(2);
            scalar.retarget(5);
            ScalarSpring spring;
            spring.value = -2;
            spring.velocity = .7;
            FrameClock clock;
            clock.setActive(true);
            clock.advance(0);
            double deliveredDt = 0;
            connect(&clock, &FrameClock::frame, this, [&](double, double motionDt) {
                deliveredDt = motionDt;
            });
            for (const auto sampleValue : testCase.value(u"samples"_s).toArray()) {
                const auto sample = sampleValue.toObject();
                for (const auto eventValue : sample.value(u"events"_s).toArray()) {
                    const auto event = eventValue.toString();
                    if (event == "guide"_L1)
                        scalar.retarget(18, true, 1.8);
                    if (event == "reverse"_L1) {
                        scalar.retarget(-3, true, 1.4);
                        archive.retarget(springs, { -.62, -5.2, -1, -1 });
                    }
                    if (event == "rebase"_L1) {
                        archive.translate({ -.62, -5.2, -1, -1 });
                        const ArchiveValues offset{ -.62, -5.2, -1, -1 };
                        for (size_t i = 0; i < 4; ++i)
                            springs[i].translate(offset[i]);
                    }
                    if (event == "fast"_L1)
                        scalar.retarget(8, false, 1.4, 9);
                    if (event == "guide-again"_L1)
                        scalar.retarget(-6, true, .9);
                }
                clock.setSpeed(sample.value(u"speed"_s).toDouble());
                clock.advance(sample.value(u"rawDt"_s).toDouble());
                near(deliveredDt, sample.value(u"dt"_s).toDouble());
                spring.update(1, 3.7, deliveredDt);
                archive.update(springs, deliveredDt, sample.value(u"reduced"_s).toBool());
                scalar.update(deliveredDt, sample.value(u"reduced"_s).toBool());
                compareArchive(archive, sample.value(u"quintic"_s).toObject());
                compareState(scalar.state(), sample.value(u"scalar"_s).toObject());
                near(spring.value,
                     sample.value(u"spring"_s).toObject().value(u"value"_s).toDouble());
                near(spring.velocity,
                     sample.value(u"spring"_s).toObject().value(u"velocity"_s).toDouble());
                QCOMPARE(scalar.guided(),
                         sample.value(u"scalar"_s).toObject().value(u"guided"_s).toBool());
            }
        }
    }
    void frozenFunctions()
    {
        const auto oracle = readJson(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/motion/kernel-golden.json"_s));
        for (const auto value : oracle.value(u"functions"_s).toArray()) {
            const auto s = value.toObject();
            const double t = s.value(u"time"_s).toDouble();
            near(smooth(t), s.value(u"smooth"_s).toDouble());
            near(archiveWave(14, 3, t), s.value(u"archiveWave"_s).toDouble());
            near(extraction(t), s.value(u"extraction"_s).toDouble());
            near(settlingWave(-2, t), s.value(u"settlingWave"_s).toDouble());
            near(baselineSelectionWave(-2, t), s.value(u"baselineSelectionWave"_s).toDouble());
            near(musicSelectionWave(-2, t), s.value(u"musicSelectionWave"_s).toDouble());
            near(selectionWave(-2, t), s.value(u"selectionWave"_s).toDouble());
            near(rippleEnvelope(-2, t), s.value(u"rippleEnvelope"_s).toDouble());
            near(columnStrength(3, 2, t), s.value(u"columnStrength"_s).toDouble());
            near(idleWave(14, 3, t), s.value(u"idleWave"_s).toDouble());
            near(cinematicField(14, 3, t), s.value(u"cinematicField"_s).toDouble());
            near(returnStep(.3, std::max(0.0, t)), s.value(u"returnStep"_s).toDouble());
            near(returnStep(.3, std::max(0.0, t), true),
                 s.value(u"returnStepReduced"_s).toDouble());
        }
    }
    void exactSpringAndJoins()
    {
        ScalarSpring split, whole;
        split.value = whole.value = 4;
        split.velocity = whole.velocity = -.8;
        whole.update(-2, 3.7, 1.6);
        for (int i = 0; i < 192; ++i)
            split.update(-2, 3.7, 1.6 / 192);
        near(split.value, whole.value);
        near(split.velocity, whole.velocity);
        near(split.acceleration, whole.acceleration);
        QuinticTrack track;
        track.observe({ 1, .7, -.3 });
        track.retarget(8, 1.4);
        track.update(.4);
        const auto before = track.state();
        QVERIFY(track.retarget(-2, 1.4));
        near(track.state().value, before.value);
        near(track.state().velocity, before.velocity);
        near(track.state().acceleration, before.acceleration);
        track.update(.2);
        const auto elapsed = track.elapsed();
        QVERIFY(!track.retarget(-2, .1));
        QCOMPARE(track.elapsed(), elapsed);
        const auto beforeRebase = track.state();
        track.translate(1000);
        track.update(0);
        near(track.state().value, beforeRebase.value + 1000);
        near(track.state().velocity, beforeRebase.velocity);
        near(track.state().acceleration, beforeRebase.acceleration);
        track.update(100);
        QCOMPARE(track.state().value, 998.0);
        QCOMPARE(track.state().velocity, 0.0);
        QCOMPARE(track.state().acceleration, 0.0);
        ColumnScalar scalar;
        scalar.retarget(8, true, 1.4);
        scalar.update(.4);
        const auto scalarBefore = scalar.state();
        QVERIFY(!scalar.retarget(8, true, .1));
        scalar.update(0);
        near(scalar.state().value, scalarBefore.value);
        near(scalar.state().velocity, scalarBefore.velocity);
        near(scalar.state().acceleration, scalarBefore.acceleration);
        scalar.translate(-100);
        scalar.update(0, true);
        QCOMPARE(scalar.state().value, -92.0);
        near(archiveTravelDuration(0, 0), 1.4);
        near(archiveTravelDuration(100, 100), 2.4);
    }
    void clockSuspensionAndOpening()
    {
        FrameClock clock;
        QSignalSpy frames(&clock, &FrameClock::frame), done(&clock, &FrameClock::openingFinished);
        clock.setActive(true);
        clock.advance(3);
        QCOMPARE(clock.visualTime(), 0.0);
        clock.startOpening();
        clock.setSpeed(3);
        clock.advance(.2);
        near(clock.visualTime(), .15);
        near(clock.openingTime(), .05);
        clock.setActive(false);
        clock.advance(50);
        const auto count = clock.frameCount();
        clock.setActive(true);
        clock.advance(50);
        QCOMPARE(clock.frameCount(), count + 1);
        near(clock.visualTime(), .15);
        near(clock.openingTime(), .05);
        clock.setSpeed(.25);
        clock.advance(.04);
        near(clock.visualTime(), .16);
        near(clock.openingTime(), .09);
        clock.advance(-1);
        clock.advance(std::numeric_limits<double>::quiet_NaN());
        near(clock.openingTime(), .09);
        clock.setSpeed(50);
        QCOMPARE(clock.speed(), 3.0);
        clock.setSpeed(-2);
        QCOMPARE(clock.speed(), .25);
        clock.setSpeed(std::numeric_limits<double>::infinity());
        QCOMPARE(clock.speed(), .25);
        for (int i = 0; i < 300; ++i)
            clock.advance(.05);
        QCOMPARE(clock.openingTime(), 11.6);
        QCOMPARE(done.size(), 1);
        clock.finishOpening();
        QCOMPARE(done.size(), 1);
        QVERIFY(frames.size() > 300);
    }
    void nonfiniteControllerInputKeepsTheCurrentTrajectory()
    {
        MotionController controller;
        controller.clock()->setActive(true);
        controller.clock()->advance(0);
        controller.travel(4, 5, 6, 7);
        controller.clock()->advance(.01);
        const auto state = controller.springs();
        const auto revision = controller.revision();
        QSignalSpy changes(&controller, &MotionController::stateChanged);
        for (const double invalid :
             { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity() }) {
            QVERIFY(!controller.retarget({ invalid, 1, 2, 3 }));
            controller.translate({ 1, invalid, 2, 3 });
            auto corrupt = state;
            corrupt[0].value = invalid;
            controller.observe(corrupt, { 1, 2, 3, 4 }, { 1, 1, 1, 1 });
            QCOMPARE(controller.revision(), revision);
            for (size_t axis = 0; axis < state.size(); ++axis) {
                QCOMPARE(controller.springs()[axis].value, state[axis].value);
                QCOMPARE(controller.springs()[axis].velocity, state[axis].velocity);
            }
        }
        QVERIFY(changes.isEmpty());
    }
    void controllerCompletionAndRevisions()
    {
        MotionController controller;
        QSignalSpy done(&controller, &MotionController::completed);
        controller.clock()->setActive(true);
        controller.clock()->advance(0);
        QVERIFY(controller.retarget({ 7.44, 15.6, 12, 3 }));
        for (int i = 0; i < 10; ++i)
            controller.clock()->advance(.05);
        const auto before = controller.archive().state();
        const auto elapsed = controller.archive().progress();
        QVERIFY(!controller.retarget({ 7.44, 15.6, 12, 3 }));
        QCOMPARE(controller.revision(), quint64(1));
        QCOMPARE(controller.archive().progress(), elapsed);
        QVERIFY(controller.retarget({ -.62, -5.2, -1, -1 }));
        const auto after = controller.archive().state();
        for (size_t i = 0; i < 4; ++i) {
            near(after[i].value, before[i].value);
            near(after[i].velocity, before[i].velocity);
            near(after[i].acceleration, before[i].acceleration);
        }
        controller.translate({ -.62, -5.2, -1, -1 });
        controller.clock()->startOpening();
        controller.setReducedMotion(true);
        QVERIFY(!controller.moving());
        near(controller.rail(), -1.24);
        near(controller.column(), -10.4);
        QCOMPARE(done.size(), 1);
        QCOMPARE(done[0][0].toULongLong(), quint64(2));
        QCOMPARE(controller.clock()->openingTime(), 11.6);
        controller.finish();
        controller.clock()->advance(.05);
        QCOMPARE(done.size(), 1);
        QVERIFY(controller.retarget({ 0, 0, 0, 0 }));
        QCOMPARE(done.size(), 2);
        QCOMPARE(done[1][0].toULongLong(), quint64(3));
        controller.setReducedMotion(false);
        QVERIFY(controller.retarget({ 1, 2, 3, 4 }));
        controller.clock()->setSpeed(3);
        for (int i = 0; i < 30; ++i)
            controller.clock()->advance(.05);
        QCOMPARE(done.size(), 3);
        QCOMPARE(done[2][0].toULongLong(), quint64(4));
        QCOMPARE(controller.springs()[0].velocity, 0.0);
        QCOMPARE(controller.springs()[0].acceleration, 0.0);
        // A completion callback may initiate a new request; the old revision remains identifiable.
        connect(&controller, &MotionController::completed, &controller, [&](quint64 revision) {
            if (revision == 5)
                controller.retarget({ 7, 8, 9, 10 });
        });
        controller.retarget({ 3, 4, 5, 6 });
        controller.finish();
        QCOMPARE(controller.revision(), quint64(6));
        QVERIFY(controller.moving());
        QCOMPARE(done.last()[0].toULongLong(), quint64(5));
    }
};
QTEST_GUILESS_MAIN(MotionTest)
#include "MotionTest.moc"
