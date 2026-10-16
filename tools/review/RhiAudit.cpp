#include "RhiCaptureHarness.h"
#include "SceneAdapter.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <cstdio>
#include <cmath>
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
void check(bool ok, const char *reason)
{
    RhiCaptureHarness::require(ok, reason);
}
Input fixture(QSize size, bool night, bool low, const QString &cover)
{
    SceneAdapter scene;
    scene.layout()->setViewport(size.width(), size.height());
    scene.setReducedMotion(true);
    scene.lighting()->setReducedMotion(true);
    scene.lighting()->setNight(night);
    Input in;
    in.scene = scene.frame();
    in.scene.nearPlane = .1;
    in.scene.farPlane = 100;
    in.scene.fieldOfView = 32;
    in.scene.camera.position = low ? SceneVector{ 8, 6, 15 } : SceneVector{ 0, 1.85, 12 };
    in.scene.camera.aim = { 0, 1.85, 0 };
    in.scene.view.setToIdentity();
    in.scene.view.lookAt(in.scene.camera.position.qt(), in.scene.camera.aim.qt(), { 0, 1, 0 });
    in.scene.cameraRotation =
            QQuaternion::fromRotationMatrix(in.scene.view.inverted().normalMatrix());
    in.scene.projection.setToIdentity();
    in.scene.projection.perspective(32, float(size.width()) / size.height(), .1f, 100);
    in.scene.caseTransform.setToIdentity();
    in.scene.casePosition = { 0, 0, 0 };
    scene.setLightingInput({ });
    scene.updateLighting(0);
    const auto &p = scene.lighting()->palette();
    auto *l = scene.lighting();
    in.background = p.background.qt();
    in.keyColor = p.keyColor.qt();
    in.skyColor = p.skyColor.qt();
    in.groundColor = p.groundColor.qt();
    in.lightIntensities = l->lightIntensities();
    in.spotPosition = l->spotPosition();
    in.spotTarget = l->spotTarget();
    in.spotColor = l->spotColor();
    in.spotIntensity = l->spotIntensity();
    in.environmentIntensity = l->environmentIntensity();
    in.rowSettings = l->rowSettings();
    in.rowColumn = l->rowColumn();
    in.rowDetail = l->rowDetail();
    in.rowFill = l->rowFill();
    in.lightColumn = l->lightColumn();
    in.printAmbient = l->printAmbient();
    in.cover = QImage(":/assets/demo-covers/" + cover + ".png");
    in.shadows = false;
    return in;
}
double depthAt(const QImage &i, int x, int y)
{
    const auto p = i.pixelColor(x, y);
    return (p.red() * 65536 + p.green() * 256 + p.blue()) / 16777215.0;
}
double cornerError(const Input &in, const QImage &image)
{
    const auto bg = image.pixelColor(0, 0);
    double worst = 0;
    auto inside = [&](int x, int y) {
        return image.rect().contains(x, y) && image.pixelColor(x, y) != bg;
    };
    for (float x : { -2.225f, 2.225f })
        for (float y : { .175f, 3.525f }) {
            const auto c = in.scene.projection * in.scene.view * QVector4D(x, y, .07f, 1);
            const QPointF p((c.x() / c.w() + 1) * .5 * image.width(),
                            (1 - c.y() / c.w()) * .5 * image.height());
            double best = 100;
            for (int yy = int(p.y()) - 4; yy <= int(p.y()) + 4; ++yy)
                for (int xx = int(p.x()) - 4; xx <= int(p.x()) + 4; ++xx) {
                    if (inside(xx, yy)
                        && (!inside(xx - 1, yy) || !inside(xx + 1, yy) || !inside(xx, yy - 1)
                            || !inside(xx, yy + 1)))
                        best = std::min(best, std::hypot(xx + .5 - p.x(), yy + .5 - p.y()));
                }
            worst = std::max(worst, best);
        }
    return worst;
}
QPoint projected(const Input &in, QVector3D world, QSize size)
{
    const auto clip = in.scene.projection * in.scene.view * QVector4D(world, 1);
    return { qRound((clip.x() / clip.w() + 1) * .5 * size.width()),
             qRound((1 - clip.y() / clip.w()) * .5 * size.height()) };
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    yalahoo::probeOptionalGraphicsBackend();
    if (argc != 2)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        QDir().mkpath(output);
        RhiCaptureHarness driver;
        check(driver.window->devicePixelRatio() == 1, "Opaque diagnostics require actual DPR1");
        QJsonArray cases, sizes, shadows, rebuilds, inkProbes;
        double worstDepth = 0, worstNormal = 0, worstProjection = 0;
        for (bool night : { false, true })
            for (bool low : { false, true })
                for (QString cover : { "square", "portrait", "landscape" })
                    for (int channel : { 0, 1, 2 }) {
                        const QSize size(960, 720);
                        auto in = fixture(size, night, low, cover);
                        in.geometryDiagnostic = channel != 0;
                        const auto gpu = driver.capture(in, size);
                        check(gpu.metadata["uniqueReadbackTextures"].toInt() == 4
                                      && gpu.metadata["readbackFieldSlots"].toInt() == 12,
                              "Opaque diagnostic readback duplicated an aliased texture");
                        check(gpu.color.constBits() == gpu.composite.constBits()
                                      && gpu.color.constBits() == gpu.postprocessed.constBits()
                                      && gpu.normal.constBits() == gpu.compositeNormal.constBits()
                                      && gpu.depth.constBits() == gpu.compositeDepth.constBits(),
                              "Aliased diagnostic fields copied their pixel storage");
                        const auto inventory = gpu.metadata["resources"].toObject();
                        check(inventory["estimatedCurrentBytes"].toDouble() > 0
                                      && inventory["measuredVramBytes"].isNull(),
                              "Missing descriptor inventory or fabricated driver memory");
                        int attachments = 0;
                        for (const auto &entry : inventory["entries"].toArray()) {
                            const auto resource = entry.toObject();
                            const auto name = resource["name"].toString();
                            if (name != "opaque-color-resolve" && name != "opaque-normal"
                                && name != "opaque-linear-depth")
                                continue;
                            ++attachments;
                            const auto format = name == "opaque-color-resolve"
                                    ? QRhiTexture::RGBA16F
                                    : QRhiTexture::RGBA8;
                            check(resource["format"].toInt() == int(format)
                                          && resource["samples"].toInt() == 1
                                          && resource["mipLevels"].toInt() == 1
                                          && resource["width"].toInt() == size.width()
                                          && resource["height"].toInt() == size.height(),
                                  "Actual diagnostic attachment format or dimensions changed");
                        }
                        check(attachments == 3, "Missing opaque diagnostic attachments");
                        const auto actual = channel == 0 ? gpu.color
                                : channel == 1           ? gpu.normal
                                                         : gpu.depth;
                        const auto label =
                                QString("%1-%2-%3-%4")
                                        .arg(night ? "night" : "day", low ? "low" : "front", cover)
                                        .arg(channel);
                        check(actual.save(output + "/" + label + ".png"), "Cannot save own output");
                        if (channel == 1) {
                            const auto point = projected(in, { .17f, 1.85f, .07f }, size);
                            const auto observed = gpu.normal.pixelColor(point);
                            const auto expected = in.scene.view.mapVector({ 0, 0, 1 }).normalized();
                            const double error = std::max(
                                    { std::abs(observed.redF() - (expected.x() * .5 + .5)),
                                      std::abs(observed.greenF() - (expected.y() * .5 + .5)),
                                      std::abs(observed.blueF() - (expected.z() * .5 + .5)) });
                            worstNormal = std::max(worstNormal, error);
                            check(error <= 1.0 / 255, "Known plane normal is incorrect");
                            if (!low) {
                                const double projection = cornerError(in, gpu.normal);
                                worstProjection = std::max(worstProjection, projection);
                                check(projection <= 2,
                                      "Authored corner projection exceeds two pixels");
                            }
                        }
                        if (channel == 2 && !low) {
                            const double observed = depthAt(gpu.depth, 480, 360);
                            const double expected = (12 - .070 - .1) / (100 - .1);
                            const double error = std::abs(observed - expected);
                            worstDepth = std::max(worstDepth, error);
                            check(error < 2e-6, "Known authored front-plane depth is incorrect");
                        }
                        // Opaque color must contain actual content, not an empty background target.
                        check(depthAt(gpu.depth, 480, 360) < 1, "Missing opaque content");
                        cases.append(QJsonObject{ { "scene", label },
                                                  { "channel", channel },
                                                  { "metadata", gpu.metadata } });
                    }
        for (const QColor ink :
             { QColor(Qt::black), QColor(96, 160, 208), QColor(128, 128, 128), QColor(Qt::white) })
            for (double irradiance : { -1.0, 0.0, .25, 2.0 }) {
                const QSize size(960, 720);
                auto in = fixture(size, false, false, "square");
                in.lightingEnabled = irradiance >= 0;
                in.environmentIntensity = 0;
                in.lightIntensities = { 0, float(std::max(0.0, irradiance) * std::acos(-1.0)), 0 };
                in.skyColor = in.groundColor = QVector3D(1, 1, 1);
                in.spotIntensity = 0;
                in.rowSettings.setX(0);
                in.printAmbient = 1;
                in.cover = QImage(64, 64, QImage::Format_RGBA8888);
                in.cover.fill(ink);
                const auto gpu = driver.capture(in, size);
                const auto observed =
                        gpu.color.pixelColor(projected(in, { .17f, 1.85f, .027f }, size));
                const double gain = irradiance < 0 ? 1 : std::min(1.0, irradiance);
                const auto linear = [gain](double value) {
                    return gain
                            * (value <= .04045 ? value / 12.92
                                               : std::pow((value + .055) / 1.055, 2.4));
                };
                const double error = std::max({ std::abs(observed.redF() - linear(ink.redF())),
                                                std::abs(observed.greenF() - linear(ink.greenF())),
                                                std::abs(observed.blueF() - linear(ink.blueF())) });
                check(error <= 1.0 / 255, "Paper source decode/diffuse clamp is incorrect");
                inkProbes.append(QJsonObject{ { "ink", ink.name() },
                                              { "irradiance", irradiance },
                                              { "linearError", error } });
            }
        for (auto size : { QSize(1920, 1080), QSize(1280, 720), QSize(995, 858), QSize(960, 900),
                           QSize(930, 900), QSize(390, 844), QSize(320, 720), QSize(2560, 1080),
                           QSize(3440, 1440), QSize(960, 720), QSize(320, 720), QSize(960, 720) })
            sizes.append(driver.capture(fixture(size, false, true, "square"), size).metadata);
        driver.replay(30, [&](int index) {
            const QSize size(index % 2 ? 930 : 390, index % 2 ? 900 : 844);
            driver.submitBurst(fixture(size, false, true, "portrait"), size);
        });
        const QSize baseSize(960, 720);
        auto finalResize = driver.capture(fixture(baseSize, false, false, "square"), baseSize);
        auto changed = fixture(baseSize, false, false, "square");
        changed.cover.fill(Qt::red);
        driver.submitWithoutCapture(changed, baseSize);
        driver.replay(3);
        const auto red = driver.surface->grab().pixelColor(480, 360);
        changed.cover.fill(Qt::blue);
        driver.submitWithoutCapture(changed, baseSize);
        driver.replay(3);
        const auto blue = driver.surface->grab().pixelColor(480, 360);
        check(red.red() > red.blue() + 20 && blue.blue() > blue.red() + 20,
              "Same-request cover revision was ignored");
        for (bool night : { false, true })
            for (int map : { 1024, 2048, 4096 }) {
                auto in = fixture(baseSize, night, true, "square");
                in.shadowDiagnostic = true;
                in.shadowSize = map;
                in.scene.camera.position = { 6, 4, 6 };
                in.scene.view.setToIdentity();
                in.scene.view.lookAt({ 6, 4, 6 }, { 0, 1.85f, 0 }, { 0, 1, 0 });
                for (float z : { 0.0f, .62f, -.62f }) {
                    QMatrix4x4 matrix;
                    matrix.translate(0, 0, z);
                    in.entities.append(matrix);
                }
                in.shadows = true;
                const auto on = driver.capture(in, baseSize);
                int shadowed = 0;
                double minimum = 1;
                for (int y = 0; y < baseSize.height(); ++y)
                    for (int x = 0; x < baseSize.width(); ++x) {
                        if (depthAt(on.depth, x, y) >= 1)
                            continue;
                        const double value = on.color.pixelColor(x, y).redF();
                        minimum = std::min(minimum, value);
                        shadowed += value < .99;
                    }
                check(shadowed > 100 && minimum >= .675 && minimum <= .70,
                      "Source shadow strength/caster projection failed");
                in.shadows = false;
                const auto off = driver.capture(in, baseSize);
                for (int y = 0; y < baseSize.height(); ++y)
                    for (int x = 0; x < baseSize.width(); ++x)
                        if (depthAt(off.depth, x, y) < 1)
                            check(off.color.pixelColor(x, y).redF() >= 254.0 / 255,
                                  "Disabled shadow receiver is not white");
                const auto label = QString("shadow-%1-%2").arg(night ? "night" : "day").arg(map);
                check(on.color.save(output + "/" + label + ".png"), "Cannot save shadow output");
                shadows.append(QJsonObject{ { "scene", label },
                                            { "shadowedReceiverPixels", shadowed },
                                            { "minimumFactor", minimum },
                                            { "metadata", on.metadata } });
            }
        const auto stableInput = fixture(baseSize, true, true, "landscape");
        for (int cycle = 0; cycle < 10; ++cycle) {
            driver.rebuild();
            const auto restored = driver.capture(stableInput, baseSize);
            check(driver.rebuilt(), "Scene graph was not actually invalidated");
            check(depthAt(restored.depth, 480, 360) < 1, "Context rebuild lost opaque geometry");
            rebuilds.append(restored.metadata);
        }
        const QJsonObject report{ { "round", "R19C2B1" },
                                  { "configuration", "Release" },
                                  { "passed", true },
                                  { "validation", "source-math-and-own-output" },
                                  { "qt", qVersion() },
                                  { "offscreen", driver.surface->diagnostics() },
                                  { "windowPolicy", verification::windowPolicy(*driver.window) },
                                  { "cases", cases },
                                  { "resize", sizes },
                                  { "resizeBurst", 30 },
                                  { "sameRequestCoverReplacement", true },
                                  { "finalResize", finalResize.metadata },
                                  { "shadows", shadows },
                                  { "rebuilds", rebuilds },
                                  { "inkProbes", inkProbes },
                                  { "knownPlaneDepthError", worstDepth },
                                  { "knownPlaneNormalError", worstNormal },
                                  { "maxCornerProjectionPixels", worstProjection } };
        QSaveFile file(output + "/runtime.json");
        check(file.open(QIODevice::WriteOnly), "Cannot open RHI report");
        file.write(QJsonDocument(report).toJson());
        check(file.commit(), "Cannot commit RHI report");
        std::puts("Opaque source mathematics, own output and resource checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "RHI opaque: %s\n", error.what());
        return 1;
    }
}
