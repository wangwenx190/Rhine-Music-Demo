#include "RenderQuality.h"
#include "SceneAdapter.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace yalahoo;
class RenderQualityTest : public QObject
{
    Q_OBJECT
    QJsonObject load(const char *name)
    {
        QFile f((QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/occlusion/"_s)
                + QString::fromUtf8(name));
        if (!f.open(QIODevice::ReadOnly))
            return { };
        return QJsonDocument::fromJson(f.readAll()).object();
    }
private Q_SLOTS:
    void nativeDiagnosticDprProtection()
    {
        RenderQuality quality;
        const auto preferences = quality.values();
        quality.resize(944, 900, 1, 1.5, 16384, true);
        QCOMPARE(quality.targetSize(), QSize(1416, 1350));
        QVERIFY(!quality.limited());
        quality.resize(3440, 1440, 1, 1.5, 16384, true);
        QVERIFY(quality.limited());
        QVERIFY(qint64(quality.targetSize().width()) * quality.targetSize().height() <= 8294400);
        QCOMPARE(quality.values(), preferences);
        quality.resize(944, 900, 1, 1.5, 16384, true);
        QCOMPARE(quality.targetSize(), QSize(1416, 1350));
        QVERIFY(!quality.limited());
    }
    void frozenQuality()
    {
        const auto oracle = load("quality.json");
        QVERIFY(!oracle.isEmpty());
        RenderQuality q;
        for (auto value : oracle[u"normalized"_s].toArray()) {
            auto c = value.toObject();
            q.configure(c[u"input"_s].toObject().toVariantMap());
            const auto expected = c[u"expected"_s].toObject().toVariantMap();
            for (auto i = expected.begin(); i != expected.end(); ++i)
                QCOMPARE(q.values()[i.key()], i.value());
            QCOMPARE(q.preset(), c[u"preset"_s].toString());
        }
        for (auto value : oracle[u"dimensions"_s].toArray()) {
            const auto c = value.toObject(), e = c[u"expected"_s].toObject();
            q.setPreset(c[u"preset"_s].toString());
            q.resize(c[u"width"_s].toDouble(), c[u"height"_s].toDouble(),
                     c[u"stageScale"_s].toDouble(), c[u"dpr"_s].toDouble(),
                     c[u"maximum"_s].toInt());
            QCOMPARE(q.targetSize(), QSize(e[u"width"_s].toInt(), e[u"height"_s].toInt()));
            QCOMPARE(q.limited(), e[u"limited"_s].toBool());
            QVERIFY(qint64(q.targetSize().width()) * q.targetSize().height() <= 8294400);
            QVERIFY(q.targetSize().width() <= c[u"maximum"_s].toInt());
            QVERIFY(q.targetSize().height() <= c[u"maximum"_s].toInt());
            QCOMPARE(q.preset(), c[u"preset"_s].toString());
        }
    }
    void seededKernel()
    {
        const auto oracle = load("kernel.json");
        QVERIFY(!oracle.isEmpty());
        for (int count : { 16, 32, 64 }) {
            quint32 state = 417;
            auto random = [&] {
                state = state * 1664525u + 1013904223u;
                return double(state) / 4294967296.0;
            };
            const auto samples = oracle[u"kernels"_s]
                                         .toObject()[QString::number(count)]
                                         .toObject()[u"samples"_s]
                                         .toArray();
            QCOMPARE(samples.size(), count);
            for (int i = 0; i < count; ++i) {
                double x = 2 * random() - 1, y = 2 * random() - 1, z = random(),
                       length = std::sqrt(x * x + y * y + z * z),
                       scale = .1 + .9 * std::pow(double(i) / count, 2);
                const auto s = samples[i].toArray();
                QVERIFY(std::abs(x / length * scale - s[0].toDouble()) < 1e-12);
                QVERIFY(std::abs(y / length * scale - s[1].toDouble()) < 1e-12);
                QVERIFY(std::abs(z / length * scale - s[2].toDouble()) < 1e-12);
            }
        }
    }
    void invalidAndIdempotent()
    {
        RenderQuality q;
        QSignalSpy changes(&q, &RenderQuality::changed);
        q.setPreset(u"original"_s);
        QCOMPARE(changes.size(), 0);
        const auto size = q.targetSize();
        q.resize(-1, 10, 1, 1, 4096);
        QCOMPARE(q.targetSize(), size);
        q.setPreset(u"ultra"_s);
        q.resize(8000, 8000, 1, 3, 16384);
        QVERIFY(q.limited());
        QCOMPARE(q.values()[u"scale"_s].toInt(), 150);
        auto values = q.values();
        values[u"aoResolution"_s] = .5;
        q.configure(values);
        QCOMPARE(q.aoSize(), QSize(q.targetSize().width() / 2, q.targetSize().height() / 2));
    }
    void preservesSceneState()
    {
        SceneAdapter scene;
        scene.setReducedMotion(true);
        scene.setDetail(1);
        const auto revision = scene.frameRevision();
        const auto position = scene.cameraPosition();
        const auto *lighting = scene.lighting();
        for (auto preset : { "performance", "high", "ultra", "original" }) {
            scene.quality()->setPreset(QString::fromUtf8(preset));
            QCOMPARE(scene.frameRevision(), revision);
            QCOMPARE(scene.cameraPosition(), position);
            QCOMPARE(scene.detail(), 1.0);
            QCOMPARE(scene.lighting(), lighting);
        }
    }
    void focalPlaneFollowsCamera()
    {
        SceneAdapter scene;
        scene.layout()->setViewport(960, 720);
        scene.observe({ { 0, 1.85, 12 }, { 0, 1.85, 0 }, 7.33 }, 0);
        QCOMPARE(scene.focalDepth({ 0, 0, 0 }), 12.0);
        QCOMPARE(scene.focalDepth({ 0, 0, 4 }), 8.0);
        QCOMPARE(scene.focalDepth({ 0, 0, -4 }), 16.0);
        scene.observe({ { 6, 3.8, 6 }, { 0, 1.85, 0 }, 7.33 }, 0);
        const auto direction = QVector3D(-6, -1.95f, -6).normalized();
        QVERIFY(std::abs(scene.focalDepth({ 0, 0, 0 })
                         - QVector3D::dotProduct(QVector3D(-6, -1.8f, -6), direction))
                < 1e-5);
        // Rebase both entity and camera: optical depth is translation invariant.
        const auto before = scene.focalDepth({ 0, 0, 0 });
        scene.observe({ { 106, 23.8, 36 }, { 100, 21.85, 30 }, 7.33 }, 0);
        QVERIFY(std::abs(scene.focalDepth({ 100, 20, 30 }) - before) < 1e-4);
    }
};
QTEST_GUILESS_MAIN(RenderQualityTest)
#include "RenderQualityTest.moc"
