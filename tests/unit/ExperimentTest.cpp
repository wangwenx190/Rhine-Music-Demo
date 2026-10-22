#include "../support/SettingsFixture.h"
#include "RowLightController.h"
#include "SelectionLighting.h"
#include "SceneAdapter.h"
#include "LibrarySession.h"
#include "PreferencesStore.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "DiagnosticSamples.h"
#include "DemoLibrary.h"
#include "SettingsController.h"
#include "OfflineMetadataProvider.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <limits>

using namespace Qt::StringLiterals;
using namespace yalahoo;
class ExperimentTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ExperimentTest)
public:
    ExperimentTest() = default;

private Q_SLOTS:
    void authoredLightingAndTemporaryComparison()
    {
        QTemporaryDir directory;
        test::SettingsFixture fixture(directory.path());
        auto *lighting = fixture.scene.lighting();
        QSignalSpy changes(lighting, &SelectionLighting::settingsChanged);
        const auto defaults = lighting->settings();
        RowLightingInput input;
        lighting->update(input, { }, { }, 0, 0);
        input.row = 18;
        lighting->update(input, { }, { }, 0, .1);
        const auto position = lighting->ribbon.row.value;
        const auto velocity = lighting->ribbon.row.velocity;
        QVERIFY(lighting->configure({ { u"mode"_s, u"hybrid"_s },
                                      { u"strength"_s, 1.8 },
                                      { u"width"_s, 2.0 },
                                      { u"offset"_s, -2.0 },
                                      { u"fill"_s, .4 },
                                      { u"transitionDuration"_s, 2.0 } }));
        QCOMPARE(lighting->ribbon.row.value, position);
        QCOMPARE(lighting->ribbon.row.velocity, velocity);
        const auto authored = lighting->settings();
        QCOMPARE(changes.size(), 1);
        lighting->setComparing(true);
        QCOMPARE(lighting->ribbon.settings.mode, RowLightingMode::Baseline);
        QCOMPARE(lighting->settings(), authored);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(fixture.preferences.values()[u"lighting"_s].toMap(), authored);
        lighting->setComparing(false);
        QCOMPARE(lighting->ribbon.settings.mode, RowLightingMode::Hybrid);
        for (const auto &invalid : { QVariant(u"1"_s), QVariant(true),
                                     QVariant(std::numeric_limits<double>::quiet_NaN()) }) {
            QVERIFY(!lighting->configure({ { u"strength"_s, invalid }, { u"width"_s, 1.0 } }));
            QCOMPARE(lighting->settings(), authored);
        }
        QVERIFY(!lighting->configure({ { u"mode"_s, u"unknown"_s } }));
        QVERIFY(!lighting->configure({ { u"unknown"_s, 1 } }));
        QVERIFY(lighting->configure({ { u"strength"_s, 9.0 }, { u"offset"_s, -99.0 } }));
        QCOMPARE(lighting->settings()[u"strength"_s].toDouble(), 2.0);
        QCOMPARE(lighting->settings()[u"offset"_s].toDouble(), -6.0);
        lighting->setComparing(true);
        lighting->resetSettings();
        QVERIFY(!lighting->comparing());
        QCOMPARE(lighting->settings(), defaults);
        QCOMPARE(lighting->ribbon.settings.mode, RowLightingMode::Guided);
        QVERIFY(fixture.preferences.update(
                { { u"lighting"_s, QVariantMap{ { u"future"_s, 7 } } } }));
        lighting->configure({ { u"mode"_s, u"area"_s } });
        QCOMPARE(fixture.preferences.values()[u"lighting"_s].toMap()[u"future"_s].toInt(), 7);
    }
    void lightingPreferenceSurvivesRestart()
    {
        QTemporaryDir directory;
        {
            test::SettingsFixture fixture(directory.path());
            fixture.scene.lighting()->configure({ { u"mode"_s, u"area"_s }, { u"width"_s, 2.2 } });
            fixture.scene.lighting()->setComparing(true);
        }
        test::SettingsFixture fixture(directory.path());
        QCOMPARE(fixture.scene.lighting()->settings()[u"mode"_s].toString(), u"area"_s);
        QCOMPARE(fixture.scene.lighting()->settings()[u"width"_s].toDouble(), 2.2);
        QVERIFY(!fixture.scene.lighting()->comparing());
        QCOMPARE(fixture.scene.lighting()->ribbon.settings.mode, RowLightingMode::Area);
    }
    void speedDoesNotRestartTrajectoryOrOpening()
    {
        MotionController motion;
        auto *clock = motion.clock();
        clock->setActive(true);
        clock->startOpening();
        motion.travel(7.44, 15.6, 12, 3);
        clock->advance(0);
        clock->advance(.02);
        const auto revision = motion.revision();
        for (double speed : { .25, 3.0, .5, 2.0, 1.0, .25, 3.0, 1.0 }) {
            const auto rail = motion.rail(), column = motion.column();
            const auto opening = clock->openingTime(), visual = clock->visualTime();
            clock->setSpeed(speed);
            QCOMPARE(motion.revision(), revision);
            QCOMPARE(motion.rail(), rail);
            QCOMPARE(motion.column(), column);
            QCOMPARE(clock->openingTime(), opening);
            QCOMPARE(clock->visualTime(), visual);
            clock->advance(.02);
            QVERIFY(std::abs(clock->openingTime() - opening - .02) < 1e-12);
            QVERIFY(std::abs(clock->visualTime() - visual - .02 * speed) < 1e-12);
        }
    }
    void boundedDiagnosticsAndUnclampedIntervals()
    {
        rhi::DiagnosticSeries series;
        for (int i = 1; i <= 100; ++i)
            series.add(i);
        auto distribution = series.distribution();
        QCOMPARE(distribution[u"p50"_s].toDouble(), 50.0);
        QCOMPARE(distribution[u"p95"_s].toDouble(), 95.0);
        QCOMPARE(distribution[u"p99"_s].toDouble(), 99.0);
        for (int i = 101; i <= 1000; ++i)
            series.add(i);
        distribution = series.distribution();
        QCOMPARE(distribution[u"count"_s].toInt(), 256);
        QCOMPARE(distribution[u"discarded"_s].toInt(), 744);
        QCOMPARE(distribution[u"p99"_s].toDouble(), 998.0);
        rhi::DiagnosticSamples samples;
        QVERIFY(!samples.add({ 0, 3, 1, -1, 12, 4, 8 }));
        QVERIFY(!samples.add({ 20, 5, 2, -1, 12, 4, 8 }));
        auto batch = samples.add({ 600, 7, 4, -1, 12, 4, 8 });
        QVERIFY(batch.has_value());
        QCOMPARE(batch->submissions, 3);
        QCOMPARE(batch->intervals.distribution()[u"p95"_s].toDouble(), 580.0);
        QVERIFY(batch->gpu.distribution().isEmpty());
        QCOMPARE(batch->atlasBytes, 36);
        QCOMPARE(batch->selectedBytes, 12);
        QCOMPARE(batch->uniformBytes, 24);
    }
    void anonymousDemoHasNoPlayableOrLocalSources()
    {
        const auto snapshot = demonstrationLibrary();
        QCOMPARE(snapshot.genres.size(), 9);
        QCOMPARE(snapshot.albums.size(), 27);
        QSet<QString> ids;
        for (const auto &album : snapshot.albums) {
            QVERIFY(!ids.contains(album.id));
            ids.insert(album.id);
            QVERIFY(album.tracks.isEmpty());
            QCOMPARE(album.coverUrl.scheme(), u"qrc"_s);
            QVERIFY(album.coverUrl.path().startsWith(u"/assets/demo-covers/"_s));
        }
    }
};
QTEST_GUILESS_MAIN(ExperimentTest)
#include "ExperimentTest.moc"
