#include "ShellMaterial.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
using namespace yalahoo::rhi;
class RhiMaterialTest : public QObject
{
    Q_OBJECT
private slots:
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
QTEST_GUILESS_MAIN(RhiMaterialTest)
#include "RhiMaterialTest.moc"
