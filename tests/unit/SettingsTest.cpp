#include "SettingsController.h"
#include "OfflineMetadataProvider.h"
#include "LibraryStore.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <limits>

using namespace yalahoo;
namespace {
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool put(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
}
class SettingsTest : public QObject
{
    Q_OBJECT
private slots:
    void persistenceImportAndDamage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("preferences.json");
        const auto source = directory.filePath(QStringLiteral("显式 导出.json"));
        const QByteArray bytes = R"({"theme":"night","sortMode":"artist","motionSpeed":2.5,
            "reduced":true,"renderQuality":{"scale":125,"futureQuality":7},"future":{"x":8}})";
        QVERIFY(put(source, bytes));
        {
            PreferencesStore preferences(directory.path());
            QSignalSpy imported(&preferences, &PreferencesStore::imported);
            QSignalSpy saved(&preferences, &PreferencesStore::saved);
            QVERIFY(preferences.importJson(QUrl::fromLocalFile(source)));
            QTRY_COMPARE(imported.size(), 1);
            QTRY_COMPARE(saved.size(), 1);
            QCOMPARE(preferences.values()["theme"].toString(), "night");
            for (int i = 0; i < 100; ++i)
                QVERIFY(preferences.update({ { "motionSpeed", .25 + i * .025 } }));
            QVERIFY(preferences.update({ { "renderQuality", QVariantMap{ { "scale", 150 } } } }));
            QTRY_COMPARE(saved.size(), 2);
            auto json = QJsonDocument::fromJson(read(path)).object();
            QCOMPARE(json["schemaVersion"].toInt(), 1);
            QCOMPARE(json["future"].toObject()["x"].toInt(), 8);
            QCOMPARE(json["renderQuality"].toObject()["futureQuality"].toInt(), 7);
            QCOMPARE(read(source), bytes);
            const auto before = read(path);
            QVERIFY(!preferences.update({ { "theme", "invalid" } }));
            QVERIFY(!preferences.update({ { "motionSpeed", true } }));
            QVERIFY(!preferences.update({ { "soundVolume", 2.0 } }));
            QCOMPARE(read(path), before);
            QVERIFY(preferences.importJson(QUrl::fromLocalFile(source)));
            QVERIFY(preferences.update({ { "theme", "day" } }));
            QTRY_COMPARE(saved.size(), 3);
            QCOMPARE(imported.size(), 1);
            QCOMPARE(preferences.values()["theme"].toString(), "day");
        }
        PreferencesStore restored(directory.path());
        QCOMPARE(restored.values()["theme"].toString(), "day");
        for (const auto &invalid : { QByteArray("{"), QByteArray("[]"),
                                     QByteArray("{\"schemaVersion\":2,\"theme\":\"night\"}"),
                                     QByteArray("{\"reduced\":5}") }) {
            QVERIFY(put(path, invalid));
            PreferencesStore damaged(directory.path());
            QVERIFY(!damaged.error().isEmpty());
            QVERIFY(!damaged.update({ { "theme", "day" } }));
            QCOMPARE(read(path), invalid);
        }
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
        QVERIFY(settings.update("theme", "night"));
        QVERIFY(settings.update("motionSpeed", 2.0));
        QVERIFY(settings.update("sortMode", "album"));
        QVERIFY(settings.update("rememberColumnPosition", false));
        QVERIFY(settings.update("arrayMode", "realistic"));
        QCOMPARE(quality.size(), 0);
        QTRY_COMPARE(saved.size(), 1);
        motion.clock()->setActive(true);
        for (int i = 0; i < 90; ++i)
            motion.clock()->advance(1.0 / 60);
        QCoreApplication::processEvents();
        QCOMPARE(saved.size(), 1);
        QVERIFY(settings.update("quality", "high"));
        QCOMPARE(quality.size(), 1);
        auto authored = scene.quality()->values();
        authored["scale"] = 175;
        QVERIFY(settings.update("renderQuality", authored));
        QCOMPARE(settings.values()["quality"].toString(), "custom");
        QSignalSpy reload(&settings, &SettingsController::reloadRequested);
        const auto snapshot = library.snapshot();
        const auto values = settings.values();
        settings.reload();
        QCOMPARE(reload.size(), 1);
        QCOMPARE(settings.values(), values);
        QCOMPARE(library.snapshot(), snapshot);
        QVERIFY(!motion.clock()->active());
        QVERIFY(!settings.update("theme", "invalid"));
        QVERIFY(!settings.update("motionSpeed", "2"));
        QVERIFY(!settings.update("motionSpeed", std::numeric_limits<double>::quiet_NaN()));
        QTRY_COMPARE(saved.size(), 2);
        PreferencesStore restored(directory.path());
        QCOMPARE(restored.values()["renderQuality"].toMap()["scale"].toInt(), 175);
        QCOMPARE(restored.values()["arrayMode"].toString(), "realistic");
        QVERIFY(!restored.rememberColumnPosition());
    }
    void configurationKeepsUnknownFields()
    {
        QTemporaryDir directory;
        LibraryStore store(directory.path());
        store.load();
        QTRY_VERIFY(!store.busy());
        QVERIFY(store.setMetadataConfiguration(
                { { "musicBrainzContact", "project@example.invalid" },
                  { "onlineEnabled", true },
                  { "future", 7 } }));
        QTRY_VERIFY(!store.busy());
        QVERIFY(store.setMetadataConfiguration({ { "musicBrainzContact", "新 联系" } }));
        QTRY_VERIFY(!store.busy());
        const auto configuration = store.snapshot()->fields["metadataConfiguration"].toObject();
        QCOMPARE(configuration["future"].toInt(), 7);
        QCOMPARE(configuration["musicBrainzContact"].toString(), QStringLiteral("新 联系"));
        QVERIFY(configuration["onlineEnabled"].toBool());
        LibraryStore restored(directory.path());
        restored.load();
        QTRY_VERIFY(!restored.busy());
        QCOMPARE(restored.snapshot()->fields["metadataConfiguration"].toObject(), configuration);
    }
};
QTEST_GUILESS_MAIN(SettingsTest)
#include "SettingsTest.moc"
