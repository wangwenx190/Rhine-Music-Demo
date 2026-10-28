#include "NumericChecks.h"
#include "RowLightController.h"
#include "FrameClock.h"
#include "SelectionLighting.h"
#include "SceneAdapter.h"
#include <QtTest>
#include <QSet>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <limits>

using namespace Qt::StringLiterals;
using yalahoo::test::errorMagnitude;
using namespace yalahoo;
class LightingTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LightingTest)
public:
    LightingTest() = default;

private Q_SLOTS:
    void unchangedPublishedLightingDoesNotNotify()
    {
        SelectionLighting light;
        QSignalSpy changes(&light, &SelectionLighting::changed);
        RowLightingInput input;
        input.origin = { 1, 2, 3 };
        input.reduced = true;
        const SceneVector aim{ 4, 5, 6 }, offset{ 7, 8, 9 };
        light.update(input, aim, offset, 0, 0);
        QCOMPARE(changes.size(), 1);
        changes.clear();
        for (int i = 0; i < 8; ++i)
            light.update(input, aim, offset, 0, 1.0 / 60);
        QCOMPARE(changes.size(), 0);
        light.update(input, aim, offset, .5, 0);
        QCOMPARE(changes.size(), 1);
        changes.clear();
        input.visible = false;
        light.update(input, aim, offset, .5, 0);
        QCOMPARE(changes.size(), 1);
        changes.clear();
        light.update(input, aim, offset, .5, 0);
        QCOMPARE(changes.size(), 0);
        input.visible = true;
        light.update(input, aim, offset, .5, 0);
        QCOMPARE(changes.size(), 1);
        changes.clear();
        light.setReducedMotion(true);
        light.setNight(true);
        QCOMPARE(changes.size(), 1);
        changes.clear();
        light.setReducedMotion(true);
        light.update(input, aim, offset, .5, 0);
        QCOMPARE(changes.size(), 0);
        QVERIFY(light.configure({ { u"strength"_s, .9 } }));
        QCOMPARE(changes.size(), 1);
        changes.clear();
        light.update(input, aim, offset, .5, 0);
        QCOMPARE(changes.size(), 0);
    }
    void floorThemeUsesLinearAuthoredColorsAndInterruptedTransition()
    {
        const auto decode = [](int value) {
            const double s = value / 255.;
            return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
        };
        for (const auto &theme : { u"day"_s, u"night"_s, u"dusk"_s }) {
            const QColor source(theme == "day"_L1             ? "#d8c9b9"
                                        : theme == "night"_L1 ? "#0b1828"
                                                              : "#a6b8c0");
            const auto actual = LightingPalette::floorColor(theme);
            QVERIFY(errorMagnitude(actual.x - decode(source.red())) < 1e-7);
            QVERIFY(errorMagnitude(actual.y - decode(source.green())) < 1e-7);
            QVERIFY(errorMagnitude(actual.z - decode(source.blue())) < 1e-7);
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
        QFile file((QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/lighting/golden.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        double trajectoryError = 0, paletteError = 0, positionError = 0, fieldError = 0;
        auto vector = [](QJsonValue v) {
            const auto a = v.toArray();
            return SceneVector{ a[0].toDouble(), a[1].toDouble(), a[2].toDouble() };
        };
        auto diff = [](SceneVector a, SceneVector b) {
            return std::max({ errorMagnitude(a.x - b.x), errorMagnitude(a.y - b.y),
                              errorMagnitude(a.z - b.z) });
        };
        const auto cases = data[u"cases"_s].toArray();
        QCOMPARE(cases.size(), 4);
        QCOMPARE(data[u"fields"_s].toArray().size(), 392);
        QSet<int> expectedFps{ 20, 30, 60, 120 };
        for (auto c : cases) {
            const auto run = c.toObject();
            const int fps = run[u"fps"_s].toInt();
            QVERIFY(expectedFps.remove(fps));
            QCOMPARE(run[u"samples"_s].toArray().size(), fps * 4 + 1);
            SelectionLighting light;
            for (auto value : c.toObject()[u"samples"_s].toArray()) {
                const auto sample = value.toObject(), i = sample[u"input"_s].toObject(),
                           p = sample[u"palette"_s].toObject();
                light.setNight(i[u"night"_s].toBool());
                light.setReducedMotion(i[u"reduced"_s].toBool());
                auto settings = light.ribbon.settings;
                settings.mode = RowLightingMode(i[u"mode"_s].toInt());
                light.setSettings(settings);
                RowLightingInput input;
                input.origin = vector(i[u"origin"_s]);
                input.row = i[u"row"_s].toDouble();
                input.lane = i[u"lane"_s].toDouble();
                input.independentColumns = i[u"independent"_s].toBool();
                input.visible = i[u"visible"_s].toBool();
                input.reduced = i[u"reduced"_s].toBool();
                input.cinematic = i[u"cinematic"_s].toBool();
                if (i.contains(u"introTime"_s))
                    input.introTime = i[u"introTime"_s].toDouble();
                light.update(input, input.origin + SceneVector{ -2.085, 2.252, 0 },
                             { -48, -17.6, 36 }, i[u"detail"_s].toDouble(), i[u"dt"_s].toDouble());
                auto scalar = [&](double actual, QJsonValue expected) {
                    trajectoryError =
                            std::max(trajectoryError, errorMagnitude(actual - expected.toDouble()));
                };
                scalar(light.ribbon.row.value, sample[u"row"_s].toArray()[0]);
                scalar(light.ribbon.row.velocity, sample[u"row"_s].toArray()[1]);
                scalar(light.ribbon.lane.value, sample[u"lane"_s].toArray()[0]);
                scalar(light.ribbon.lane.velocity, sample[u"lane"_s].toArray()[1]);
                scalar(light.rowDetail(), sample[u"detail"_s]);
                trajectoryError = std::max(trajectoryError,
                                           diff(light.ribbon.column, vector(sample[u"column"_s])));
                trajectoryError = std::max(trajectoryError,
                                           diff(light.anchorValue(), vector(sample[u"anchor"_s])));
                trajectoryError =
                        std::max(trajectoryError,
                                 diff(light.columnValue(), vector(sample[u"lightColumn"_s])));
                if (input.visible) {
                    auto actual = light.spotPosition();
                    positionError = std::max(positionError,
                                             diff({ actual.x(), actual.y(), actual.z() },
                                                  vector(sample[u"spotPosition"_s])));
                    actual = light.areaPosition();
                    positionError = std::max(positionError,
                                             diff({ actual.x(), actual.y(), actual.z() },
                                                  vector(sample[u"areaPosition"_s])));
                }
                const auto &palette = light.palette();
                for (auto pair : { std::pair{ palette.environment, p[u"environment"_s] },
                                   std::pair{ palette.key, p[u"key"_s] },
                                   std::pair{ palette.hemisphere, p[u"hemisphere"_s] },
                                   std::pair{ palette.spot, p[u"spot"_s] },
                                   std::pair{ palette.area, p[u"area"_s] },
                                   std::pair{ palette.scatter, p[u"scatter"_s] },
                                   std::pair{ palette.printAmbient, p[u"printAmbient"_s] } })
                    paletteError = std::max(paletteError,
                                            errorMagnitude(pair.first - pair.second.toDouble()));
                paletteError = std::max({ paletteError,
                                          diff(palette.scatterColor, vector(p[u"scatterColor"_s])),
                                          diff(palette.spotColor, vector(p[u"spotColor"_s])) });
            }
        }
        QVERIFY(expectedFps.isEmpty());
        QSet<QByteArray> fieldIdentities;
        for (auto value : data[u"fields"_s].toArray()) {
            const auto field = value.toObject();
            const auto identity = QJsonDocument(QJsonArray{ field[u"mode"_s], field[u"fill"_s],
                                                            field[u"dx"_s], field[u"dz"_s] })
                                          .toJson(QJsonDocument::Compact);
            QVERIFY(!fieldIdentities.contains(identity));
            fieldIdentities.insert(identity);
            QVERIFY((QList<int>{ 0, 1, 2, 3 }.contains(field[u"mode"_s].toInt(-1))));
            QVERIFY((QList<double>{ 0, 1 }.contains(field[u"fill"_s].toDouble(-1))));
            QVERIFY((QList<double>{ -54, -27, -5.2, 0, 5.2, 27, 54 }.contains(
                    field[u"dx"_s].toDouble())));
            QVERIFY((QList<double>{ -3, -.62, 0, .186, .6045, 1, 5 }.contains(
                    field[u"dz"_s].toDouble())));
            RowLightController light;
            light.settings.mode = RowLightingMode(field[u"mode"_s].toInt());
            light.settings.fill = field[u"fill"_s].toDouble();
            fieldError = std::max(fieldError,
                                  errorMagnitude(light.sample({ field[u"dx"_s].toDouble(), 0,
                                                                field[u"dz"_s].toDouble() })
                                                 - field[u"value"_s].toDouble()));
        }
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
        QVERIFY(errorMagnitude(ribbon.column.z - world) < 1e-12);
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
        QVERIFY(errorMagnitude(scene.lighting()->warmth() - expected) < 1e-12);
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
