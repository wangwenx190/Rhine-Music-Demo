#include "Require.h"
#include "ReviewApplication.h"
#include "RhiCaptureHarness.h"
#include "SceneAdapter.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include "JsonFile.h"
#include <cstdio>
#include <cmath>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;

namespace {
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
    in.setLighting(*scene.lighting());
    in.cover = QImage(":/assets/demo-covers/"_L1 + cover + ".png"_L1);
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

auto runOpaqueChecks(int &argc, char **&argv)
{
    if (argc != 2)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        QDir().mkpath(output);
        RhiCaptureHarness driver;
        require(driver.window->devicePixelRatio() == 1, "Opaque diagnostics require actual DPR1");
        QJsonArray cases, rebuilds, inkProbes;
        double worstDepth = 0, worstNormal = 0, worstProjection = 0;
        for (bool night : { false, true })
            for (bool low : { false, true })
                for (const auto &cover : { u"square"_s, u"portrait"_s, u"landscape"_s })
                    for (bool diagnostic : { false, true }) {
                        const QSize size(960, 720);
                        auto in = fixture(size, night, low, cover);
                        in.geometryDiagnostic = diagnostic;
                        const auto gpu = driver.capture(in, size);
                        require(gpu.metadata[u"uniqueReadbackTextures"_s].toInt() == 4
                                        && gpu.metadata[u"readbackFieldSlots"_s].toInt() == 12,
                                "Opaque diagnostic readback duplicated an aliased texture");
                        require(gpu.color.constBits() == gpu.composite.constBits()
                                        && gpu.color.constBits() == gpu.postprocessed.constBits()
                                        && gpu.normal.constBits() == gpu.compositeNormal.constBits()
                                        && gpu.depth.constBits() == gpu.compositeDepth.constBits(),
                                "Aliased diagnostic fields copied their pixel storage");
                        const auto inventory = gpu.metadata[u"resources"_s].toObject();
                        require(inventory[u"estimatedCurrentBytes"_s].toDouble() > 0
                                        && inventory[u"measuredVramBytes"_s].isNull(),
                                "Missing descriptor inventory or fabricated driver memory");
                        int attachments = 0;
                        for (const auto &entry : inventory[u"entries"_s].toArray()) {
                            const auto resource = entry.toObject();
                            const auto name = resource[u"name"_s].toString();
                            if (name != "opaque-color-resolve"_L1 && name != "opaque-normal"_L1
                                && name != "opaque-linear-depth"_L1)
                                continue;
                            ++attachments;
                            const auto format = name == "opaque-color-resolve"_L1
                                    ? QRhiTexture::RGBA16F
                                    : QRhiTexture::RGBA8;
                            require(resource[u"format"_s].toInt() == int(format)
                                            && resource[u"samples"_s].toInt() == 1
                                            && resource[u"mipLevels"_s].toInt() == 1
                                            && resource[u"width"_s].toInt() == size.width()
                                            && resource[u"height"_s].toInt() == size.height(),
                                    "Actual diagnostic attachment format or dimensions changed");
                        }
                        require(attachments == 3, "Missing opaque diagnostic attachments");
                        const auto actual = diagnostic ? gpu.normal : gpu.color;
                        const auto label = u"%1-%2-%3-%4"_s
                                                   .arg(night ? "night"_L1 : "day"_L1,
                                                        low ? "low"_L1 : "front"_L1, cover)
                                                   .arg(diagnostic ? "geometry"_L1 : "color"_L1);
                        require(actual.save(output + "/"_L1 + label + ".png"_L1),
                                "Cannot save own output");
                        if (diagnostic) {
                            require(gpu.depth.save(output + "/"_L1 + label + "-depth.png"_L1),
                                    "Cannot save own depth output");
                            const auto point = projected(in, { .17f, 1.85f, .07f }, size);
                            const auto observed = gpu.normal.pixelColor(point);
                            const auto expected = in.scene.view.mapVector({ 0, 0, 1 }).normalized();
                            const double error = std::max(
                                    { std::abs(observed.redF() - (expected.x() * .5 + .5)),
                                      std::abs(observed.greenF() - (expected.y() * .5 + .5)),
                                      std::abs(observed.blueF() - (expected.z() * .5 + .5)) });
                            worstNormal = std::max(worstNormal, error);
                            require(error <= 1.0 / 255, "Known plane normal is incorrect");
                            if (!low) {
                                const double projection = cornerError(in, gpu.normal);
                                worstProjection = std::max(worstProjection, projection);
                                require(projection <= 2,
                                        "Authored corner projection exceeds two pixels");
                            }
                        }
                        if (diagnostic && !low) {
                            const double observed = depthAt(gpu.depth, 480, 360);
                            const double expected = (12 - .070 - .1) / (100 - .1);
                            const double error = std::abs(observed - expected);
                            worstDepth = std::max(worstDepth, error);
                            require(error < 2e-6, "Known authored front-plane depth is incorrect");
                        }
                        // Opaque color must contain actual content, not an empty background target.
                        require(depthAt(gpu.depth, 480, 360) < 1, "Missing opaque content");
                        cases.append(QJsonObject{ { u"scene"_s, label },
                                                  { u"geometryDiagnostic"_s, diagnostic },
                                                  { u"metadata"_s, gpu.metadata } });
                    }
        for (const QColor ink :
             { QColor(Qt::black), QColor(96, 160, 208), QColor(128, 128, 128), QColor(Qt::white) })
            for (double irradiance : { -1.0, 0.0, .25, 2.0 }) {
                const QSize size(960, 720);
                auto in = fixture(size, false, false, u"square"_s);
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
                require(error <= 1.0 / 255, "Paper source decode/diffuse clamp is incorrect");
                inkProbes.append(QJsonObject{ { u"ink"_s, ink.name() },
                                              { u"irradiance"_s, irradiance },
                                              { u"linearError"_s, error } });
            }
        const QSize baseSize(960, 720);
        auto changed = fixture(baseSize, false, false, u"square"_s);
        changed.cover.fill(Qt::red);
        driver.submitWithoutCapture(changed, baseSize);
        driver.replay(3);
        const auto red = driver.surface->grab().pixelColor(480, 360);
        changed.cover.fill(Qt::blue);
        driver.submitWithoutCapture(changed, baseSize);
        driver.replay(3);
        const auto blue = driver.surface->grab().pixelColor(480, 360);
        require(red.red() > red.blue() + 20 && blue.blue() > blue.red() + 20,
                "Same-request cover revision was ignored");
        const auto stableInput = fixture(baseSize, true, true, u"landscape"_s);
        {
            driver.rebuild();
            const auto restored = driver.capture(stableInput, baseSize);
            require(driver.rebuilt(), "Scene graph was not actually invalidated");
            require(depthAt(restored.depth, 480, 360) < 1, "Context rebuild lost opaque geometry");
            rebuilds.append(restored.metadata);
        }
        const QJsonObject report{ { u"configuration"_s, u"Release"_s },
                                  { u"passed"_s, true },
                                  { u"validation"_s, u"source-math-and-own-output"_s },
                                  { u"qt"_s, QString::fromUtf8(qVersion()) },
                                  { u"offscreen"_s, driver.surface->diagnostics() },
                                  { u"windowPolicy"_s, verification::windowPolicy(*driver.window) },
                                  { u"cases"_s, cases },
                                  { u"sameRequestCoverReplacement"_s, true },
                                  { u"rebuilds"_s, rebuilds },
                                  { u"inkProbes"_s, inkProbes },
                                  { u"knownPlaneDepthError"_s, worstDepth },
                                  { u"knownPlaneNormalError"_s, worstNormal },
                                  { u"maxCornerProjectionPixels"_s, worstProjection } };
        require(yalahoo::writeJsonFile(output + "/runtime.json"_L1, QJsonDocument(report)),
                "Cannot commit RHI report");
        std::puts("Opaque source mathematics, own output and resource checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "RHI opaque: %s\n", error.what());
        return 1;
    }
}
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        return runOpaqueChecks(argc, argv);
    });
}
