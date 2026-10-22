#include "LayoutMetrics.h"
#include "RenderQuality.h"
#include "SceneAdapter.h"
#include <QtTest>
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
        for (const auto &invalid :
             { QVariant(u"100"_s), QVariant(true), QVariant("100"_ba), QVariant("invalid"_ba),
               QVariant(std::numeric_limits<double>::infinity()),
               QVariant(-std::numeric_limits<double>::infinity()),
               QVariant(std::numeric_limits<double>::quiet_NaN()) })
            scene.quality()->configure({ { u"depthOfField"_s, invalid } });
        QCOMPARE(scene.quality()->values(), values);
        scene.quality()->resize(-1, 10, 2);
        QCOMPARE(scene.quality()->targetSize(), QSize(3840, 2160));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, scene.quality()->resize(1e100, 10, 2));
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
