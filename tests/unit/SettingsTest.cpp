#include "../support/SettingsFixture.h"
#include "../support/FileFixture.h"
#include "RenderQuality.h"
#include "SceneAdapter.h"
#include "LibrarySession.h"
#include "PreferencesStore.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "SettingsController.h"
#include "OfflineMetadataProvider.h"
#include "LibraryStore.h"
#include "NumericValue.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <limits>

using namespace Qt::StringLiterals;

using namespace yalahoo;
class SettingsTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsTest)
public:
    SettingsTest() = default;

private Q_SLOTS:
    void numericStorageIsAcceptedWithoutTextConversions()
    {
        for (const auto &value :
             { QVariant::fromValue(char(2)), QVariant::fromValue(short(2)), QVariant::fromValue(2u),
               QVariant::fromValue(2L), QVariant::fromValue(2LL), QVariant::fromValue(2ULL),
               QVariant::fromValue(2.f), QVariant(2.) }) {
            const auto number = finiteNumber(value);
            QVERIFY(number.has_value());
            QCOMPARE(*number, 2.);
        }
        for (const auto &value : { QVariant(), QVariant(true), QVariant(u"2"_s), QVariant("2"_ba) })
            QVERIFY(!finiteNumber(value));
    }
    void invalidSpeedConversionKeepsRuntimeAndPreferences()
    {
        QTemporaryDir directory;
        test::SettingsFixture fixture(directory.path());
        QVERIFY(fixture.settings.update(u"motionSpeed"_s, 2.0));
        const auto before = fixture.preferences.values();
        QSignalSpy changes(&fixture.settings, &SettingsController::changed);
        for (const auto &invalid : { QVariant("2"_ba), QVariant("invalid"_ba),
                                     QVariant(std::numeric_limits<double>::infinity()),
                                     QVariant(-std::numeric_limits<double>::infinity()),
                                     QVariant(std::numeric_limits<double>::quiet_NaN()) })
            QVERIFY(!fixture.settings.update(u"motionSpeed"_s, invalid));
        QCOMPARE(fixture.motion.clock()->speed(), 2.0);
        QCOMPARE(fixture.preferences.values(), before);
        QVERIFY(changes.isEmpty());
    }
    void singleInstancePreference()
    {
        QTemporaryDir directory;
        {
            test::SettingsFixture fixture(directory.path());
            QVERIFY(fixture.settings.values().value(u"singleInstance"_s).toBool());
            QVERIFY(!fixture.settings.update(u"singleInstance"_s, u"false"_s));
            QVERIFY(!fixture.preferences.update({ { u"singleInstance"_s, 1 } }));
            QVERIFY(fixture.settings.update(u"singleInstance"_s, false));
            QVERIFY(!fixture.settings.values().value(u"singleInstance"_s).toBool());
        }
        PreferencesStore restored(directory.path());
        QVERIFY(!restored.values().value(u"singleInstance"_s).toBool());
    }
    void qualityFieldKeepsOtherRuntimeValues()
    {
        QTemporaryDir directory;
        test::SettingsFixture fixture(directory.path());
        const auto before = fixture.scene.quality()->values();
        QVERIFY(fixture.settings.updateQuality(u"depthOfField"_s, 75));
        auto expected = before;
        expected.insert(u"depthOfField"_s, 75.);
        QCOMPARE(fixture.scene.quality()->values(), expected);
        QVERIFY(!fixture.settings.updateQuality(u"unknown"_s, 1));
        for (const auto &invalid : { QVariant("75"_ba), QVariant("invalid"_ba),
                                     QVariant(std::numeric_limits<double>::infinity()),
                                     QVariant(-std::numeric_limits<double>::infinity()),
                                     QVariant(std::numeric_limits<double>::quiet_NaN()) })
            QVERIFY(!fixture.settings.updateQuality(u"depthOfField"_s, invalid));
        QCOMPARE(fixture.scene.quality()->values(), expected);
    }

    void oversizedQualityImportKeepsPreferencesAndRuntime()
    {
        QTemporaryDir directory;
        test::SettingsFixture fixture(directory.path());
        QVERIFY(fixture.settings.updateQuality(u"depthOfField"_s, 75));
        const auto before = fixture.preferences.values();
        QVERIFY(!fixture.settings.updateQuality(u"depthOfField"_s, 1e308));
        QCOMPARE(fixture.scene.quality()->depthOfField(), 75.);
        QCOMPARE(fixture.preferences.values(), before);
        const auto source = directory.filePath(u"oversized.json"_s);
        QVERIFY(test::writeFile(
                source, R"({"schemaVersion":1,"renderQuality":{"depthOfField":1e308}})"_ba));
        QSignalSpy errors(&fixture.preferences, &PreferencesStore::saveError);
        QVERIFY(fixture.preferences.importJson(QUrl::fromLocalFile(source)));
        QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(fixture.preferences.values(), before);
        QCOMPARE(fixture.scene.quality()->depthOfField(), 75.);
    }
    void persistenceImportAndDamage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(u"preferences.json"_s);
        const auto source = directory.filePath(u"\u663e\u5f0f \u5bfc\u51fa.json"_s);
        const QByteArray bytes = R"({"theme":"night","sortMode":"artist","motionSpeed":2.5,
            "reduced":true,"renderQuality":{"depthOfField":125,"futureQuality":7},"future":{"x":8}})"_ba;
        QVERIFY(test::writeFile(source, bytes));
        {
            PreferencesStore preferences(directory.path());
            QSignalSpy imported(&preferences, &PreferencesStore::imported);
            QSignalSpy saved(&preferences, &PreferencesStore::saved);
            QVERIFY(preferences.importJson(QUrl::fromLocalFile(source)));
            QTRY_COMPARE(imported.size(), 1);
            QTRY_COMPARE(saved.size(), 1);
            QCOMPARE(preferences.values()[u"theme"_s].toString(), u"night"_s);
            for (int i = 0; i < 100; ++i)
                QVERIFY(preferences.update({ { u"motionSpeed"_s, .25 + i * .025 } }));
            QVERIFY(preferences.update(
                    { { u"renderQuality"_s, QVariantMap{ { u"depthOfField"_s, 150 } } } }));
            QTRY_COMPARE(saved.size(), 2);
            auto json = QJsonDocument::fromJson(yalahoo::test::readFile(path)).object();
            QCOMPARE(json[u"schemaVersion"_s].toInt(), 1);
            QCOMPARE(json[u"future"_s].toObject()[u"x"_s].toInt(), 8);
            QCOMPARE(json[u"renderQuality"_s].toObject()[u"futureQuality"_s].toInt(), 7);
            QCOMPARE(yalahoo::test::readFile(source), bytes);
            const auto before = yalahoo::test::readFile(path);
            QVERIFY(!preferences.update({ { u"theme"_s, u"invalid"_s } }));
            QVERIFY(!preferences.update({ { u"motionSpeed"_s, true } }));
            QVERIFY(!preferences.update({ { u"soundVolume"_s, 2.0 } }));
            QCOMPARE(yalahoo::test::readFile(path), before);
            QVERIFY(preferences.importJson(QUrl::fromLocalFile(source)));
            QVERIFY(preferences.update({ { u"theme"_s, u"day"_s } }));
            QTRY_COMPARE(saved.size(), 3);
            QCOMPARE(imported.size(), 1);
            QCOMPARE(preferences.values()[u"theme"_s].toString(), u"day"_s);
        }
        PreferencesStore restored(directory.path());
        QCOMPARE(restored.values()[u"theme"_s].toString(), u"day"_s);
        for (const auto &invalid :
             { "{"_ba, "[]"_ba, "{\"schemaVersion\":2,\"theme\":\"night\"}"_ba,
               "{\"reduced\":5}"_ba }) {
            QVERIFY(test::writeFile(path, invalid));
            PreferencesStore damaged(directory.path());
            QVERIFY(!damaged.error().isEmpty());
            QVERIFY(!damaged.update({ { u"theme"_s, u"day"_s } }));
            QCOMPARE(yalahoo::test::readFile(path), invalid);
        }
    }
    void saveErrorCallbackCanSubmitNewPreferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.filePath(u"blocked"_s);
        QVERIFY(test::writeFile(root, "block directory creation"_ba));
        PreferencesStore preferences(root);
        QSignalSpy saved(&preferences, &PreferencesStore::saved);
        QSignalSpy errors(&preferences, &PreferencesStore::saveError);
        connect(&preferences, &PreferencesStore::saveError, &preferences, [&] {
            if (errors.size() != 1)
                return;
            QVERIFY(QFile::remove(root));
            QVERIFY(preferences.update({ { u"motionSpeed"_s, 2.0 } }));
        });
        QVERIFY(preferences.update({ { u"theme"_s, u"night"_s } }));
        QTRY_COMPARE(errors.size(), 1);
        QTRY_COMPARE(saved.size(), 1);
        QVERIFY(preferences.error().isEmpty());
        const auto stored =
                QJsonDocument::fromJson(test::readFile(QDir(root).filePath(u"preferences.json"_s)))
                        .object();
        QCOMPARE(stored[u"theme"_s].toString(), u"night"_s);
        QCOMPARE(stored[u"motionSpeed"_s].toDouble(), 2.0);
    }
    void fieldRoutingAndVisualReload()
    {
        QTemporaryDir directory;
        PreferencesStore preferences(directory.path());
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        MotionController motion;
        SceneAdapter scene(motion.clock());
        SelectionController selection(&motion);
        scene.setSelection(&selection);
        SettingsController settings(&preferences, &library, &motion, &scene, &selection);
        QSignalSpy quality(scene.quality(), &RenderQuality::changed);
        QSignalSpy saved(&preferences, &PreferencesStore::saved);
        QVERIFY(settings.update(u"theme"_s, u"night"_s));
        QVERIFY(settings.update(u"motionSpeed"_s, 2.0));
        QVERIFY(settings.update(u"sortMode"_s, u"album"_s));
        QVERIFY(settings.update(u"rememberColumnPosition"_s, false));
        QVERIFY(settings.update(u"arrayMode"_s, u"realistic"_s));
        QCOMPARE(quality.size(), 0);
        QTRY_COMPARE(saved.size(), 1);
        motion.clock()->setActive(true);
        for (int i = 0; i < 90; ++i)
            motion.clock()->advance(1.0 / 60);
        QCoreApplication::processEvents();
        QCOMPARE(saved.size(), 1);
        auto authored = scene.quality()->values();
        authored[u"depthOfField"_s] = 175;
        QVERIFY(settings.update(u"renderQuality"_s, authored));
        QSignalSpy reload(&settings, &SettingsController::reloadRequested);
        const auto snapshot = library.snapshot();
        const auto values = settings.values();
        settings.reload();
        QCOMPARE(reload.size(), 1);
        QCOMPARE(settings.values(), values);
        QCOMPARE(library.snapshot(), snapshot);
        QVERIFY(!motion.clock()->active());
        QVERIFY(!settings.update(u"theme"_s, u"invalid"_s));
        QVERIFY(!settings.update(u"motionSpeed"_s, u"2"_s));
        QVERIFY(!settings.update(u"motionSpeed"_s, std::numeric_limits<double>::quiet_NaN()));
        QTRY_COMPARE(saved.size(), 2);
        PreferencesStore restored(directory.path());
        QCOMPARE(restored.values()[u"renderQuality"_s].toMap()[u"depthOfField"_s].toInt(), 175);
        QCOMPARE(restored.values()[u"arrayMode"_s].toString(), u"realistic"_s);
        QVERIFY(!restored.rememberColumnPosition());
    }
    void configurationKeepsUnknownFields()
    {
        QTemporaryDir directory;
        LibraryStore store(directory.path());
        store.load();
        QTRY_VERIFY(!store.busy());
        QVERIFY(store.setMetadataConfiguration(
                { { u"musicBrainzContact"_s, u"project@example.invalid"_s },
                  { u"onlineEnabled"_s, true },
                  { u"future"_s, 7 } }));
        QTRY_VERIFY(!store.busy());
        QVERIFY(store.setMetadataConfiguration(
                { { u"musicBrainzContact"_s, u"\u65b0 \u8054\u7cfb"_s } }));
        QTRY_VERIFY(!store.busy());
        const auto configuration = store.snapshot()->fields[u"metadataConfiguration"_s].toObject();
        QCOMPARE(configuration[u"future"_s].toInt(), 7);
        QCOMPARE(configuration[u"musicBrainzContact"_s].toString(), u"\u65b0 \u8054\u7cfb"_s);
        QVERIFY(configuration[u"onlineEnabled"_s].toBool());
        LibraryStore restored(directory.path());
        restored.load();
        QTRY_VERIFY(!restored.busy());
        QCOMPARE(restored.snapshot()->fields[u"metadataConfiguration"_s].toObject(), configuration);
    }
};
QTEST_GUILESS_MAIN(SettingsTest)
#include "SettingsTest.moc"
