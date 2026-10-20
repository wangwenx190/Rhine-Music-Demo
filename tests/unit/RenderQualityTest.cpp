#include "RenderQuality.h"
#include "SceneAdapter.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace Qt::StringLiterals;
using namespace yalahoo;
class RenderQualityTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RenderQualityTest)
public:
    RenderQualityTest() = default;

private:
    QJsonObject load(const char *name)
    {
        QFile f((QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/occlusion/"_s)
                + QString::fromUtf8(name));
        if (!f.open(QIODevice::ReadOnly))
            return { };
        return QJsonDocument::fromJson(f.readAll()).object();
    }
private Q_SLOTS:
    void nativeOutputDimensions()
    {
        RenderQuality quality;
        for (const QSize logical : { QSize(320, 720), QSize(995, 858), QSize(1920, 1080),
                                     QSize(3440, 1440), QSize(7680, 4320) }) {
            for (double dpr : { 1.0, 1.25, 1.5, 2.0, 3.0 }) {
                quality.resize(logical.width(), logical.height(), dpr);
                QCOMPARE(quality.targetSize(), logical * dpr);
            }
        }
        quality.resize(640, 480, 2);
        QCOMPARE(quality.targetSize(), QSize(1280, 960));
        quality.resize(640, 480, 1.5);
        QCOMPARE(quality.targetSize(), QSize(960, 720));
        QCOMPARE(quality.aoSamples(), 64);
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
    void effectChangesKeepOutputAndScene()
    {
        SceneAdapter scene;
        scene.setReducedMotion(true);
        scene.setDetail(1);
        scene.quality()->resize(1920, 1080, 2);
        const auto revision = scene.frameRevision();
        const auto position = scene.cameraPosition();
        QSignalSpy changes(scene.quality(), &RenderQuality::changed);
        scene.quality()->configure({ { u"depthOfField"_s, 100 } });
        QCOMPARE(changes.size(), 0);
        for (double amount : { 0.0, 75.0, 150.0, 300.0 }) {
            scene.quality()->configure({ { u"depthOfField"_s, amount } });
            QCOMPARE(scene.quality()->targetSize(), QSize(3840, 2160));
            QCOMPARE(scene.frameRevision(), revision);
            QCOMPARE(scene.cameraPosition(), position);
        }
        const auto values = scene.quality()->values();
        scene.quality()->configure({ { u"depthOfField"_s, u"100"_s } });
        scene.quality()->configure(
                { { u"depthOfField"_s, std::numeric_limits<double>::infinity() } });
        QCOMPARE(scene.quality()->values(), values);
        scene.quality()->resize(-1, 10, 2);
        QCOMPARE(scene.quality()->targetSize(), QSize(3840, 2160));
        QVERIFY_EXCEPTION_THROWN(scene.quality()->resize(1e100, 10, 2), std::runtime_error);
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
