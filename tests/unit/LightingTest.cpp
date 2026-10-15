#include "SelectionLighting.h"
#include "SceneAdapter.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <limits>
using namespace yalahoo;
class LightingTest : public QObject
{
    Q_OBJECT
private slots:
    void floorThemeUsesLinearAuthoredColorsAndInterruptedTransition()
    {
        const auto decode = [](int value) {
            const double s = value / 255.;
            return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
        };
        for (const auto &theme :
             { QStringLiteral("day"), QStringLiteral("night"), QStringLiteral("dusk") }) {
            const QColor source(theme == "day"             ? "#d8c9b9"
                                        : theme == "night" ? "#0b1828"
                                                           : "#a6b8c0");
            const auto actual = LightingPalette::floorColor(theme);
            QVERIFY(std::abs(actual.x - decode(source.red())) < 1e-7);
            QVERIFY(std::abs(actual.y - decode(source.green())) < 1e-7);
            QVERIFY(std::abs(actual.z - decode(source.blue())) < 1e-7);
        }
        SelectionLighting light;
        const auto day = light.palette().floor;
        light.setNight(true);
        light.advanceTheme(.325);
        const auto middle = light.palette().floor;
        const auto night = LightingPalette::floorColor(u"night");
        const auto expected = day * (1 - SelectionLighting::themeProgress(.5))
                + night * SelectionLighting::themeProgress(.5);
        QVERIFY((middle - expected).length() < 1e-12);
        light.setNight(false);
        QVERIFY((light.palette().floor - middle).length() < 1e-12);
        light.advanceTheme(.325);
        const auto reverse = middle * (1 - SelectionLighting::themeProgress(.5))
                + day * SelectionLighting::themeProgress(.5);
        QVERIFY((light.palette().floor - reverse).length() < 1e-12);
        light.setReducedMotion(true);
        QVERIFY((light.palette().floor - day).length() < 1e-12);
    }
    void frozenGolden()
    {
        QFile file(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/lighting/golden.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        double trajectoryError = 0, paletteError = 0, positionError = 0, fieldError = 0;
        int samples = 0;
        auto vector = [](QJsonValue v) {
            const auto a = v.toArray();
            return SceneVector{ a[0].toDouble(), a[1].toDouble(), a[2].toDouble() };
        };
        auto diff = [](SceneVector a, SceneVector b) {
            return std::max({ std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z) });
        };
        for (auto c : data["cases"].toArray()) {
            SelectionLighting light;
            for (auto value : c.toObject()["samples"].toArray()) {
                const auto sample = value.toObject(), i = sample["input"].toObject(),
                           p = sample["palette"].toObject();
                light.setNight(i["night"].toBool());
                light.setReducedMotion(i["reduced"].toBool());
                auto settings = light.ribbon.settings;
                settings.mode = RowLightingMode(i["mode"].toInt());
                light.setSettings(settings);
                RowLightingInput input;
                input.origin = vector(i["origin"]);
                input.row = i["row"].toDouble();
                input.lane = i["lane"].toDouble();
                input.independentColumns = i["independent"].toBool();
                input.visible = i["visible"].toBool();
                input.reduced = i["reduced"].toBool();
                input.cinematic = i["cinematic"].toBool();
                if (i.contains("introTime"))
                    input.introTime = i["introTime"].toDouble();
                light.update(input, input.origin + SceneVector{ -2.085, 2.252, 0 },
                             { -48, -17.6, 36 }, i["detail"].toDouble(), i["dt"].toDouble());
                auto scalar = [&](double actual, QJsonValue expected) {
                    trajectoryError =
                            std::max(trajectoryError, std::abs(actual - expected.toDouble()));
                };
                scalar(light.ribbon.row.value, sample["row"].toArray()[0]);
                scalar(light.ribbon.row.velocity, sample["row"].toArray()[1]);
                scalar(light.ribbon.lane.value, sample["lane"].toArray()[0]);
                scalar(light.ribbon.lane.velocity, sample["lane"].toArray()[1]);
                scalar(light.rowDetail(), sample["detail"]);
                trajectoryError = std::max(trajectoryError,
                                           diff(light.ribbon.column, vector(sample["column"])));
                trajectoryError = std::max(trajectoryError,
                                           diff(light.anchorValue(), vector(sample["anchor"])));
                trajectoryError = std::max(
                        trajectoryError, diff(light.columnValue(), vector(sample["lightColumn"])));
                if (input.visible) {
                    auto actual = light.spotPosition();
                    positionError = std::max(positionError,
                                             diff({ actual.x(), actual.y(), actual.z() },
                                                  vector(sample["spotPosition"])));
                    actual = light.areaPosition();
                    positionError = std::max(positionError,
                                             diff({ actual.x(), actual.y(), actual.z() },
                                                  vector(sample["areaPosition"])));
                }
                const auto &palette = light.palette();
                for (auto pair :
                     { std::pair{ palette.environment, p["environment"] },
                       std::pair{ palette.key, p["key"] },
                       std::pair{ palette.hemisphere, p["hemisphere"] },
                       std::pair{ palette.spot, p["spot"] }, std::pair{ palette.area, p["area"] },
                       std::pair{ palette.scatter, p["scatter"] },
                       std::pair{ palette.printAmbient, p["printAmbient"] } })
                    paletteError =
                            std::max(paletteError, std::abs(pair.first - pair.second.toDouble()));
                paletteError = std::max({ paletteError,
                                          diff(palette.scatterColor, vector(p["scatterColor"])),
                                          diff(palette.spotColor, vector(p["spotColor"])) });
                ++samples;
            }
        }
        for (auto value : data["fields"].toArray()) {
            const auto field = value.toObject();
            RowLightController light;
            light.settings.mode = RowLightingMode(field["mode"].toInt());
            light.settings.fill = field["fill"].toDouble();
            fieldError = std::max(
                    fieldError,
                    std::abs(light.sample({ field["dx"].toDouble(), 0, field["dz"].toDouble() })
                             - field["value"].toDouble()));
        }
        QSaveFile report(QStringLiteral(LIGHTING_REPORT));
        QVERIFY(report.open(QIODevice::WriteOnly));
        report.write(QJsonDocument(QJsonObject{ { "samples", samples },
                                                { "fields", data["fields"].toArray().size() },
                                                { "trajectoryError", trajectoryError },
                                                { "paletteError", paletteError },
                                                { "gpuUniformPositionError", positionError },
                                                { "fieldError", fieldError } })
                             .toJson());
        QVERIFY(report.commit());
        QVERIFY2(trajectoryError <= 1e-6, qPrintable(QString::number(trajectoryError, 'g', 12)));
        QVERIFY2(paletteError <= 1e-6, qPrintable(QString::number(paletteError, 'g', 12)));
        QVERIFY(positionError <= 1e-4);
        QVERIFY(fieldError <= 1e-12);
    }
    void interruptionAndOwnership()
    {
        SelectionLighting light;
        light.setNight(true);
        light.advanceTheme(.2);
        const auto interrupted = light.palette();
        light.setNight(false);
        QCOMPARE(light.palette().warmth, interrupted.warmth);
        light.advanceTheme(.65);
        QCOMPARE(light.warmth(), 1.0);
        light.setNight(true);
        light.setReducedMotion(true);
        QCOMPARE(light.warmth(), 0.0);
        RowLightController ribbon;
        RowLightingInput input;
        input.origin = { 0, 0, 0 };
        ribbon.update(input, 0);
        input.row = 18;
        for (int n = 0; n < 20; ++n)
            ribbon.update(input, 1.0 / 120);
        const double before = ribbon.row.value;
        input.row = 4;
        ribbon.update(input, 0);
        QCOMPARE(ribbon.row.value, before);
        input.independentColumns = true;
        input.origin.z = -9;
        const double world = ribbon.column.z;
        ribbon.update(input, 0);
        QVERIFY(std::abs(ribbon.column.z - world) < 1e-12);
        // Identical print/shell ownership origins give exactly the same row mask; y/local/camera are excluded.
        QCOMPARE(ribbon.sample({ 2, 0, 1 }), ribbon.sample({ 2, 999, 1 }));
        FrameClock clock;
        SceneAdapter scene(&clock);
        clock.setActive(true);
        clock.advance(0);
        scene.lighting()->setNight(true);
        clock.setSpeed(3);
        clock.advance(1.0 / 60);
        const double expected = 1 - SelectionLighting::themeProgress((3.0 / 60) / .65);
        QVERIFY(std::abs(scene.lighting()->warmth() - expected) < 1e-12);
        clock.setActive(false);
        const auto warmth = scene.lighting()->warmth();
        clock.advance(1);
        QCOMPARE(scene.lighting()->warmth(), warmth);
        scene.setNavigation(0, 10, true);
        const auto real = scene.target();
        scene.setNavigation(0, 0, true);
        QCOMPARE(scene.target().position.y, real.position.y);
    }
    void settingsBounds()
    {
        SelectionLighting light;
        auto s = light.ribbon.settings;
        s.width = 0;
        s.transitionDuration = 10;
        s.strength = std::numeric_limits<double>::quiet_NaN();
        light.setSettings(s);
        QCOMPARE(light.ribbon.settings.width, .6);
        QCOMPARE(light.ribbon.settings.transitionDuration, 2.4);
        QCOMPARE(light.ribbon.settings.strength, 1.3);
    }
};
QTEST_GUILESS_MAIN(LightingTest)
#include "LightingTest.moc"
