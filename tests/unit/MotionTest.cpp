#include "MotionController.h"
#include "MotionFunctions.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
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
ArchiveValues values(const QJsonObject &object)
{
    ArchiveValues result;
    for (size_t i = 0; i < 4; ++i)
        result[i] = object.value(keys[i]).toDouble();
    return result;
}
}
class MotionTest : public QObject
{
    Q_OBJECT
    std::array<double, 3> maxError{ };
    int r01Samples = 0, extendedSamples = 0;
    void near(double actual, double expected, size_t derivative = 0)
    {
        const double error = std::abs(actual - expected);
        maxError[derivative] = std::max(maxError[derivative], error);
        QVERIFY2(std::isfinite(actual) && error <= 1e-6,
                 qPrintable(u"actual %1 expected %2 error %3"_s.arg(actual, 0, 'g', 17)
                                    .arg(expected, 0, 'g', 17)
                                    .arg(error, 0, 'g', 17)));
    }
    void compareState(Kinematics state, const QJsonObject &expected)
    {
        near(state.value, expected.value(u"value"_s).toDouble());
        near(state.velocity, expected.value(u"velocity"_s).toDouble(), 1);
        near(state.acceleration, expected.value(u"acceleration"_s).toDouble(), 2);
    }
    void compareArchive(const ArchiveMotion &motion, const QJsonObject &expected)
    {
        const auto state = motion.state();
        for (size_t i = 0; i < 4; ++i)
            compareState(state[i], expected.value(keys[i]).toObject());
    }
private Q_SLOTS:
    void r01Golden()
    {
        const auto oracle = readJson(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/docs/contracts/motion-golden.json"_s));
        const auto input = oracle.value(u"inputs"_s).toObject();
        const auto archiveInput = input.value(u"quintic"_s).toObject();
        int previousFps = 0;
        ScalarSpring spring;
        ArchiveSprings springs{ };
        ArchiveMotion archive;
        ColumnScalar scalar;
        FrameClock clock;
        for (const auto sampleValue : oracle.value(u"samples"_s).toArray()) {
            const auto sample = sampleValue.toObject();
            const int fps = sample.value(u"fps"_s).toInt();
            if (fps != previousFps) {
                previousFps = fps;
                spring = { };
                spring.velocity = .2;
                springs = { };
                archive.reset();
                archive.observe(springs, { }, { 3.7, 3.7, 3.7, 3.7 });
                archive.retarget(springs, values(archiveInput.value(u"target"_s).toObject()));
                scalar = ColumnScalar(0);
                scalar.retarget(18, true, 1.8);
                clock.setActive(false);
                clock.setActive(true);
                clock.startOpening();
                clock.advance(0);
            }
            for (const auto eventValue : sample.value(u"appliedEvents"_s).toArray()) {
                const auto type = eventValue.toObject().value(u"type"_s).toString();
                if (type == "reverse"_L1) {
                    scalar.retarget(-3, true, 1.4);
                    archive.retarget(springs,
                                     values(archiveInput.value(u"reverseTarget"_s).toObject()));
                } else if (type == "rebase"_L1) {
                    const auto offset = values(archiveInput.value(u"rebase"_s).toObject());
                    archive.translate(offset);
                    for (size_t i = 0; i < 4; ++i)
                        springs[i].translate(offset[i]);
                }
            }
            const double dt = sample.value(u"dt"_s).toDouble();
            if (dt > 0) {
                spring.update(1, 3.7, dt);
                archive.update(springs, dt);
                scalar.update(dt);
                clock.advance(dt);
            }
            const auto expectedSpring = sample.value(u"spring"_s).toObject();
            near(spring.value, expectedSpring.value(u"value"_s).toDouble());
            near(spring.velocity, expectedSpring.value(u"velocity"_s).toDouble(), 1);
            compareArchive(archive, sample.value(u"quintic"_s).toObject());
            compareState(scalar.state(), sample.value(u"realColumn"_s).toObject());
            QCOMPARE(scalar.guided(),
                     sample.value(u"realColumn"_s).toObject().value(u"guided"_s).toBool());
            QCOMPARE(scalar.settled(),
                     sample.value(u"realColumn"_s).toObject().value(u"settled"_s).toBool());
            near(musicSelectionWave(2, sample.value(u"time"_s).toDouble()),
                 sample.value(u"wave"_s).toDouble());
            near(clock.openingTime(), sample.value(u"clock"_s).toDouble());
            ++r01Samples;
        }
        clock.startOpening();
        clock.advance(0);
        double previous = 0;
        for (const auto value : oracle.value(u"longFrames"_s).toArray()) {
            const auto sample = value.toObject();
            const double now = sample.value(u"input"_s).toDouble();
            clock.advance(now - previous);
            previous = now;
            near(clock.openingTime(), sample.value(u"output"_s).toDouble());
        }
    }
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
                     sample.value(u"spring"_s).toObject().value(u"velocity"_s).toDouble(), 1);
                QCOMPARE(scalar.guided(),
                         sample.value(u"scalar"_s).toObject().value(u"guided"_s).toBool());
                ++extendedSamples;
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
        near(split.velocity, whole.velocity, 1);
        near(split.acceleration, whole.acceleration, 2);
        QuinticTrack track;
        track.observe({ 1, .7, -.3 });
        track.retarget(8, 1.4);
        track.update(.4);
        const auto before = track.state();
        QVERIFY(track.retarget(-2, 1.4));
        near(track.state().value, before.value);
        near(track.state().velocity, before.velocity, 1);
        near(track.state().acceleration, before.acceleration, 2);
        track.update(.2);
        const auto elapsed = track.elapsed();
        QVERIFY(!track.retarget(-2, .1));
        QCOMPARE(track.elapsed(), elapsed);
        const auto beforeRebase = track.state();
        track.translate(1000);
        track.update(0);
        near(track.state().value, beforeRebase.value + 1000);
        near(track.state().velocity, beforeRebase.velocity, 1);
        near(track.state().acceleration, beforeRebase.acceleration, 2);
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
        near(scalar.state().velocity, scalarBefore.velocity, 1);
        near(scalar.state().acceleration, scalarBefore.acceleration, 2);
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
            near(after[i].velocity, before[i].velocity, 1);
            near(after[i].acceleration, before[i].acceleration, 2);
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
    void cleanupTestCase()
    {
        const QJsonObject report{ { u"r01Samples"_s, r01Samples },
                                  { u"extendedSamples"_s, extendedSamples },
                                  { u"fps"_s, QJsonArray{ 20, 30, 60, 120 } },
                                  { u"maxPositionError"_s, maxError[0] },
                                  { u"maxVelocityError"_s, maxError[1] },
                                  { u"maxAccelerationError"_s, maxError[2] },
                                  { u"tolerance"_s, 1e-6 } };
        QSaveFile output(QString::fromUtf8(MOTION_REPORT));
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(report).toJson());
        QVERIFY(output.commit());
    }
};
QTEST_GUILESS_MAIN(MotionTest)
#include "MotionTest.moc"
