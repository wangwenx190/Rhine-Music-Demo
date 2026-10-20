#include "ReadbackImage.h"
#include <rhi/qrhi.h>
#include <stdexcept>
#include <array>
#include <QFloat16>
#include <cstring>
#include <limits>
#include "ShellMaterial.h"
#include "SelectionLighting.h"
#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

using namespace Qt::StringLiterals;
using namespace yalahoo::rhi;
class RhiMaterialTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RhiMaterialTest)
public:
    RhiMaterialTest() = default;

private Q_SLOTS:
    void readbackValidationAndHdrSaturation()
    {
        QRhiReadbackResult result;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readbackImage(result, false));
        result.format = QRhiTexture::RGBA32F;
        result.pixelSize = { 1, 1 };
        result.data.resize(16);
        const std::array<float, 4> values{ -1e30f, 1e30f, .5f, 1.f };
        std::memcpy(result.data.data(), values.data(), sizeof(values));
        QCOMPARE(readbackImage(result, false).pixelColor(0, 0), QColor(0, 255, 128, 255));
        result.data.chop(1);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readbackImage(result, false));
        result.data.resize(16);
        const std::array<float, 4> invalid{ 0, 0, std::numeric_limits<float>::quiet_NaN(), 1 };
        std::memcpy(result.data.data(), invalid.data(), sizeof(invalid));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readbackImage(result, false));
    }
    void byteOrderedBgraReadback()
    {
        QRhiReadbackResult result;
        result.format = QRhiTexture::BGRA8;
        result.pixelSize = { 1, 1 };
        result.data = QByteArray::fromHex("12345678"_ba);
        QCOMPARE(readbackImage(result, false).pixelColor(0, 0), QColor(0x56, 0x34, 0x12, 0x78));
    }
    void paddedScalarReadbackKeepsLogicalRows()
    {
        QRhiReadbackResult result;
        result.format = QRhiTexture::R16F;
        result.pixelSize = { 4, 2 };
        const std::array<qfloat16, 8> values{ qfloat16(0.f),  qfloat16(.25f), qfloat16(.5f),
                                              qfloat16(0.f),  qfloat16(.75f), qfloat16(1.f),
                                              qfloat16(-1.f), qfloat16(0.f) };
        result.data.resize(sizeof(values));
        std::memcpy(result.data.data(), values.data(), sizeof(values));
        const auto image = readbackImage(result, true, false, { 3, 2 });
        QCOMPARE(image.size(), QSize(3, 2));
        QCOMPARE(image.pixelColor(0, 0), QColor(191, 191, 191, 255));
        QCOMPARE(image.pixelColor(1, 0), QColor(255, 255, 255, 255));
        QCOMPARE(image.pixelColor(2, 0), QColor(0, 0, 0, 255));
        QCOMPARE(image.pixelColor(2, 1), QColor(128, 128, 128, 255));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readbackImage(result, false, false, { 5, 2 }));
    }
    void continuousSourceAndQmlParameters()
    {
        qmlRegisterUncreatableType<yalahoo::SelectionLighting>("Yalahoo.Music", 1, 0,
                                                               "SelectionLighting", u"Injected"_s);
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
        )"_ba,
                QUrl::fromLocalFile(
                        (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/unit/parameters.qml"_s)));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> material(component.createWithInitialProperties(
                { { u"lighting"_s, QVariant::fromValue(&lighting) } }));
        QVERIFY2(material, qPrintable(component.errorString()));
        QFile file(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/materials/parameters.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()[u"cases"_s].toArray();
        QCOMPARE(cases.size(), 45);
        for (const auto &value : cases) {
            const auto c = value.toObject();
            const int surface = c[u"surface"_s].toInt();
            const double warmth = c[u"warmth"_s].toDouble(), clarity = c[u"clarity"_s].toDouble();
            const auto native = shellMaterial(surface, float(warmth), float(clarity), .25f);
            QVERIFY(material->setProperty("surface", surface));
            QVERIFY(material->setProperty("warmth", warmth));
            QVERIFY(material->setProperty("clarity", clarity));
            const char *fields[] = { "transmission", "roughness", "thickness",
                                     "attenuationDistance" };
            for (int i = 0; i < 4; ++i) {
                const double expected = c[u"transport"_s].toArray()[i].toDouble();
                QVERIFY2(std::abs(native.transport[i] - expected) < 2e-6, fields[i]);
                QVERIFY2(std::abs(material->property(fields[i]).toDouble() - expected) < 1e-6,
                         fields[i]);
            }
            for (int i = 0; i < 3; ++i) {
                QVERIFY(std::abs(native.tint[i] - c[u"tint"_s].toArray()[i].toDouble()) < 3e-5);
                QVERIFY(std::abs(native.attenuation[i]
                                 - c[u"attenuation"_s].toArray()[i].toDouble())
                        < 3e-5);
            }
            QCOMPARE(native.coat.x(), float(c[u"coat"_s].toArray()[0].toDouble()));
            QCOMPARE(native.coat.y(), float(c[u"coat"_s].toArray()[1].toDouble()));
            QCOMPARE(material->property("clearcoat").toDouble(),
                     c[u"coat"_s].toArray()[0].toDouble());
            QCOMPARE(material->property("coatRoughness").toDouble(),
                     c[u"coat"_s].toArray()[1].toDouble());
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
        QFile f((QString::fromUtf8(FIXTURE_ROOT))
                + "/tests/fixtures/render-backend/visual.json"_L1);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(f.readAll()).object()[u"cases"_s].toArray();
        int checked = 0;
        for (auto value : cases) {
            const auto c = value.toObject();
            for (int surface = 0; surface < 3; ++surface) {
                const auto p = c[u"parameters"_s].toArray()[surface].toObject();
                const auto m = shellMaterial(surface, c[u"day"_s].toBool() ? 1 : 0,
                                             float(c[u"clarity"_s].toDouble()), .25f);
                const char *fields[] = { "transmission", "roughness", "thickness",
                                         "attenuationDistance" };
                for (int n = 0; n < 4; ++n)
                    QVERIFY2(std::abs(m.transport[n] - p[QString::fromUtf8(fields[n])].toDouble())
                                     < 2e-6,
                             fields[n]);
                for (int n = 0; n < 3; ++n) {
                    QVERIFY(std::abs(m.tint[n] - p[u"color"_s].toArray()[n].toDouble()) < 3e-5);
                    QVERIFY(std::abs(m.attenuation[n] - p[u"attenuation"_s].toArray()[n].toDouble())
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
