#include "RhiFogAudit.h"
#include "OutputReference.h"
#include "RhiReviewInput.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDir>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
double viewDepth(const Input &input, const Capture &capture)
{
    const auto &image = capture.compositeDepth;
    const auto color = image.pixelColor(image.width() / 2, image.height() / 2);
    RhiCaptureHarness::require(color != QColor(Qt::white), "Fog math sample missed shell geometry");
    const double depth = (color.red() * 65536. + color.green() * 256. + color.blue()) / 16777215.;
    return input.scene.nearPlane + depth * (input.scene.farPlane - input.scene.nearPlane);
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        RhiCaptureHarness::require(QDir().mkpath(output), "Cannot create fog output");
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/rhi-shell/visual.json"_L1);
        RhiCaptureHarness::require(file.open(QIODevice::ReadOnly), "Missing fog input");
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()[u"cases"_s].toArray();
        RhiCaptureHarness driver;
        QJsonArray results, rebuilds;
        Input last;
        qint64 generation = -1;
        double maxError = 0;
        for (bool night : { false, true }) {
            auto input = fixture(cases[0].toObject());
            SelectionLighting lighting;
            lighting.setReducedMotion(true);
            lighting.setNight(night);
            input.background = lighting.backgroundLinear();
            input.exposure = float(lighting.exposure());
            input.shadows = false;
            input.rawOutput = false;
            input.captureSamples = 1;
            const auto initial = driver.capture(input, { 960, 720 });
            const double depth = viewDepth(input, initial);
            // Each range encloses the same actual geometry at 0, 1/4, 1/2, 3/4 or full fog.
            // Includes both endpoints and the explicitly disabled independent Review path.
            for (double fraction : { -.25, 0., .25, .5, .75, 1., 1.25 }) {
                input.scene.fogNear = depth - fraction * 20;
                input.scene.fogFar = input.scene.fogNear + 20;
                for (bool enabled : { false, true }) {
                    input.fogEnabled = enabled;
                    const double t = std::clamp(fraction, 0., 1.);
                    const double factor = enabled ? t * t * (3 - 2 * t) : 0;
                    const QVector3D fogged = QVector3D(.18f, .42f, .73f) * float(1 - factor)
                            + input.background * float(factor);
                    for (int probe : { 11, 12, 13 }) {
                        input.opticalProbe = probe;
                        const auto capture = driver.capture(input, { 960, 720 });
                        checkSubmittedFog(input, capture);
                        const auto actual = capture.metadata[u"opticalProbeSample"_s].toArray();
                        auto expected =
                                probe == 11 ? fogged : outputReference(fogged, input.exposure);
                        if (probe == 13)
                            for (int i = 0; i < 3; ++i)
                                expected[i] = expected[i] <= .0031308f
                                        ? expected[i] * 12.92f
                                        : float(1.055 * std::pow(expected[i], 1 / 2.4) - .055);
                        RhiCaptureHarness::require(actual.size() == 3, "Missing float fog sample");
                        double error = 0;
                        for (int i = 0; i < 3; ++i)
                            error = std::max(error, std::abs(actual[i].toDouble() - expected[i]));
                        // RGBA16F rounding plus RGB24 depth reconstruction; no image reference.
                        RhiCaptureHarness::require(error < .001,
                                                   "Fog/color order differs from source math");
                        maxError = std::max(maxError, error);
                        const auto current = capture.metadata[u"resourceGeneration"_s].toInteger();
                        if (generation >= 0)
                            RhiCaptureHarness::require(current == generation,
                                                       "Fog rebuilt render targets");
                        generation = current;
                        results.append(QJsonObject{ { u"night"_s, night },
                                                    { u"fraction"_s, fraction },
                                                    { u"enabled"_s, enabled },
                                                    { u"probe"_s, probe },
                                                    { u"error"_s, error },
                                                    { u"metadata"_s, capture.metadata } });
                    }
                }
            }
            input.scene.fogNear = depth - 5;
            input.scene.fogFar = depth + 15;
            input.opticalProbe = 0;
            input.fogEnabled = true;
            auto ordinary = driver.capture(input, { 960, 720 });
            checkSubmittedFog(input, ordinary);
            RhiCaptureHarness::require(
                    driver.surface->grab().save(output + (night ? "/night.png"_L1 : "/day.png"_L1)),
                    "Cannot save own fog output");
            last = input;
        }
        for (int cycle = 0; cycle < 10; ++cycle) {
            driver.rebuild();
            const auto capture = driver.capture(last, { 960, 720 });
            RhiCaptureHarness::require(driver.rebuilt(),
                                       "Fog lifecycle did not invalidate real scene graph");
            checkSubmittedFog(last, capture);
            RhiCaptureHarness::require(viewDepth(last, capture) < last.scene.farPlane,
                                       "Fog lifecycle lost geometry");
            rebuilds.append(capture.metadata);
        }
        QSaveFile report(output + "/runtime.json"_L1);
        RhiCaptureHarness::require(report.open(QIODevice::WriteOnly), "Cannot open fog report");
        report.write(QJsonDocument(QJsonObject{ { u"round"_s, u"R19D2"_s },
                                                { u"passed"_s, true },
                                                { u"qt"_s, QString::fromUtf8(qVersion()) },
                                                { u"rebuilds"_s, rebuilds },
                                                { u"maxError"_s, maxError },
                                                { u"cases"_s, results },
                                                { u"offscreen"_s, driver.surface->diagnostics() },
                                                { u"windowPolicy"_s,
                                                  verification::windowPolicy(*driver.window) } })
                             .toJson());
        RhiCaptureHarness::require(report.commit(), "Cannot write fog report");
        std::printf("Fog/color source math passed %lld states, max error %.9f\n", results.size(),
                    maxError);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
