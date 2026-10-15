#include "ShellMaterial.h"
#include "SelectionLighting.h"
#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
using namespace yalahoo::rhi;
class RhiMaterialTest : public QObject
{
    Q_OBJECT
private slots:
    void continuousSourceAndQmlParameters()
    {
        qmlRegisterUncreatableType<yalahoo::SelectionLighting>("Yalahoo.Music", 1, 0,
                                                               "SelectionLighting", "Injected");
        yalahoo::SelectionLighting lighting;
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(
                R"(
            import QtQuick3D
            import "../../qml/Yalahoo/Music/scene" as Scene
            Scene.GlassMaterial {
                roomTexture: Texture {}
                integrationTexture: Texture {}
                ltc1Texture: Texture {}
                ltc2Texture: Texture {}
            }
        )",
                QUrl::fromLocalFile(QStringLiteral(FIXTURE_ROOT "/tests/unit/parameters.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> material(component.createWithInitialProperties(
                { { "lighting", QVariant::fromValue(&lighting) } }));
        QVERIFY2(material, qPrintable(component.errorString()));
        QFile file(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/materials/parameters.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(cases.size(), 45);
        for (const auto &value : cases) {
            const auto c = value.toObject();
            const int surface = c["surface"].toInt();
            const double warmth = c["warmth"].toDouble(), clarity = c["clarity"].toDouble();
            const auto native = shellMaterial(surface, float(warmth), float(clarity), .25f);
            QVERIFY(material->setProperty("surface", surface));
            QVERIFY(material->setProperty("warmth", warmth));
            QVERIFY(material->setProperty("clarity", clarity));
            const char *fields[] = { "transmission", "roughness", "thickness",
                                     "attenuationDistance" };
            for (int i = 0; i < 4; ++i) {
                const double expected = c["transport"].toArray()[i].toDouble();
                QVERIFY2(std::abs(native.transport[i] - expected) < 2e-6, fields[i]);
                QVERIFY2(std::abs(material->property(fields[i]).toDouble() - expected) < 1e-6,
                         fields[i]);
            }
            for (int i = 0; i < 3; ++i) {
                QVERIFY(std::abs(native.tint[i] - c["tint"].toArray()[i].toDouble()) < 3e-5);
                QVERIFY(std::abs(native.attenuation[i] - c["attenuation"].toArray()[i].toDouble())
                        < 3e-5);
            }
            QCOMPARE(native.coat.x(), float(c["coat"].toArray()[0].toDouble()));
            QCOMPARE(native.coat.y(), float(c["coat"].toArray()[1].toDouble()));
            QCOMPARE(material->property("clearcoat").toDouble(), c["coat"].toArray()[0].toDouble());
            QCOMPARE(material->property("coatRoughness").toDouble(),
                     c["coat"].toArray()[1].toDouble());
            QCOMPARE(material->property("environmentIntensity").toFloat(), native.coat.w());
        }
        lighting.setNight(true);
        lighting.setReducedMotion(true);
        QCOMPARE(material->property("environmentIntensity").toDouble(), .16);
        QCOMPARE(material->property("exposure").toDouble(), 1.08);
        QVERIFY(material->setProperty("lightingEnabled", false));
        for (double warmth : { 0.0, .5, 1.0 }) {
            QVERIFY(material->setProperty("warmth", warmth));
            QVERIFY(std::abs(material->property("environmentIntensity").toDouble()
                             - (.68 - .20 * warmth))
                    < 1e-12);
        }
        QCOMPARE(warnings.size(), 0);
    }
    void frozenParameters()
    {
        QFile f(QString(FIXTURE_ROOT) + "/tests/fixtures/render-backend/visual.json");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(f.readAll()).object()["cases"].toArray();
        int checked = 0;
        for (auto value : cases) {
            const auto c = value.toObject();
            for (int surface = 0; surface < 3; ++surface) {
                const auto p = c["parameters"].toArray()[surface].toObject();
                const auto m = shellMaterial(surface, c["day"].toBool() ? 1 : 0,
                                             float(c["clarity"].toDouble()), .25f);
                const char *fields[] = { "transmission", "roughness", "thickness",
                                         "attenuationDistance" };
                for (int n = 0; n < 4; ++n)
                    QVERIFY2(std::abs(m.transport[n] - p[fields[n]].toDouble()) < 2e-6, fields[n]);
                for (int n = 0; n < 3; ++n) {
                    QVERIFY(std::abs(m.tint[n] - p["color"].toArray()[n].toDouble()) < 3e-5);
                    QVERIFY(std::abs(m.attenuation[n] - p["attenuation"].toArray()[n].toDouble())
                            < 3e-5);
                }
                ++checked;
            }
        }
        QCOMPARE(checked, 210);
    }
    void invalidParameters()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, shellMaterial(3, 1, 0, .25f));
        QVERIFY_THROWS_EXCEPTION(
                std::invalid_argument,
                shellMaterial(0, std::numeric_limits<float>::quiet_NaN(), 0, .25f));
        const auto clamped = shellMaterial(0, 2, -1, .25f), day = shellMaterial(0, 1, 0, .25f);
        QCOMPARE(clamped.transport, day.transport);
    }
};
QTEST_MAIN(RhiMaterialTest)
#include "RhiMaterialTest.moc"
