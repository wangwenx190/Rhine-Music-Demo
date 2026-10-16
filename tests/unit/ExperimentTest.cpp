#include "DiagnosticSamples.h"
#include "DemoLibrary.h"
#include "SettingsController.h"
#include "OfflineMetadataProvider.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <limits>
using namespace yalahoo;
class ExperimentTest : public QObject
{
    Q_OBJECT
private slots:
    void authoredLightingAndTemporaryComparison()
    {
        QTemporaryDir directory;
        PreferencesStore preferences(directory.path());
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        MotionController motion;
        SceneAdapter scene(motion.clock());
        SelectionController selection(&motion);
        SettingsController settings(&preferences, &library, &motion, &scene, &selection);
        auto *lighting = scene.lighting();
        QSignalSpy changes(lighting, &SelectionLighting::settingsChanged);
        const auto defaults = lighting->settings();
        RowLightingInput input;
        lighting->update(input, { }, { }, 0, 0);
        input.row = 18;
        lighting->update(input, { }, { }, 0, .1);
        const auto position = lighting->ribbon.row.value;
        const auto velocity = lighting->ribbon.row.velocity;
        QVERIFY(lighting->configure({ { "mode", "hybrid" },
                                      { "strength", 1.8 },
                                      { "width", 2.0 },
                                      { "offset", -2.0 },
                                      { "fill", .4 },
                                      { "transitionDuration", 2.0 } }));
        QCOMPARE(lighting->ribbon.row.value, position);
        QCOMPARE(lighting->ribbon.row.velocity, velocity);
        const auto authored = lighting->settings();
        QCOMPARE(changes.size(), 1);
        lighting->setComparing(true);
        QCOMPARE(lighting->ribbon.settings.mode, RowLightingMode::Baseline);
        QCOMPARE(lighting->settings(), authored);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(preferences.values()["lighting"].toMap(), authored);
        lighting->setComparing(false);
        QCOMPARE(lighting->ribbon.settings.mode, RowLightingMode::Hybrid);
        for (const auto &invalid : { QVariant("1"), QVariant(true),
                                     QVariant(std::numeric_limits<double>::quiet_NaN()) }) {
            QVERIFY(!lighting->configure({ { "strength", invalid }, { "width", 1.0 } }));
            QCOMPARE(lighting->settings(), authored);
        }
        QVERIFY(!lighting->configure({ { "mode", "unknown" } }));
        QVERIFY(!lighting->configure({ { "unknown", 1 } }));
        QVERIFY(lighting->configure({ { "strength", 9.0 }, { "offset", -99.0 } }));
        QCOMPARE(lighting->settings()["strength"].toDouble(), 2.0);
        QCOMPARE(lighting->settings()["offset"].toDouble(), -6.0);
        lighting->setComparing(true);
        lighting->resetSettings();
        QVERIFY(!lighting->comparing());
        QCOMPARE(lighting->settings(), defaults);
        QCOMPARE(lighting->ribbon.settings.mode, RowLightingMode::Guided);
        QVERIFY(preferences.update({ { "lighting", QVariantMap{ { "future", 7 } } } }));
        lighting->configure({ { "mode", "area" } });
        QCOMPARE(preferences.values()["lighting"].toMap()["future"].toInt(), 7);
    }
    void lightingPreferenceSurvivesRestart()
    {
        QTemporaryDir directory;
        {
            PreferencesStore preferences(directory.path());
            OfflineMetadataProvider provider;
            LibrarySession library(&provider);
            MotionController motion;
            SceneAdapter scene(motion.clock());
            SelectionController selection(&motion);
            SettingsController settings(&preferences, &library, &motion, &scene, &selection);
            scene.lighting()->configure({ { "mode", "area" }, { "width", 2.2 } });
            scene.lighting()->setComparing(true);
        }
        PreferencesStore preferences(directory.path());
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        MotionController motion;
        SceneAdapter scene(motion.clock());
        SelectionController selection(&motion);
        SettingsController settings(&preferences, &library, &motion, &scene, &selection);
        QCOMPARE(scene.lighting()->settings()["mode"].toString(), QString("area"));
        QCOMPARE(scene.lighting()->settings()["width"].toDouble(), 2.2);
        QVERIFY(!scene.lighting()->comparing());
        QCOMPARE(scene.lighting()->ribbon.settings.mode, RowLightingMode::Area);
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
        QCOMPARE(distribution["p50"].toDouble(), 50.0);
        QCOMPARE(distribution["p95"].toDouble(), 95.0);
        QCOMPARE(distribution["p99"].toDouble(), 99.0);
        for (int i = 101; i <= 1000; ++i)
            series.add(i);
        distribution = series.distribution();
        QCOMPARE(distribution["count"].toInt(), 256);
        QCOMPARE(distribution["discarded"].toInt(), 744);
        QCOMPARE(distribution["p99"].toDouble(), 998.0);
        rhi::DiagnosticSamples samples;
        QVERIFY(!samples.add({ 0, 3, 1, -1, 12, 4, 8 }));
        QVERIFY(!samples.add({ 20, 5, 2, -1, 12, 4, 8 }));
        auto batch = samples.add({ 600, 7, 4, -1, 12, 4, 8 });
        QVERIFY(batch.has_value());
        QCOMPARE(batch->submissions, 3);
        QCOMPARE(batch->intervals.distribution()["p95"].toDouble(), 580.0);
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
            QCOMPARE(album.coverUrl.scheme(), QString("qrc"));
            QVERIFY(album.coverUrl.path().startsWith("/assets/demo-covers/"));
        }
    }
};
QTEST_GUILESS_MAIN(ExperimentTest)
#include "ExperimentTest.moc"
