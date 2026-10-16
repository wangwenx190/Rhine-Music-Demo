#include "RhiCaptureHarness.h"
#include "RhiReviewInput.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtEndian>
#include <cstring>
#include <cstdio>
#include <cmath>
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
void check(bool ok, const char *message)
{
    RhiCaptureHarness::require(ok, message);
}
QJsonObject read(const QString &path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "Missing source review JSON");
    const auto document = QJsonDocument::fromJson(file.readAll());
    check(document.isObject(), "Invalid source review JSON");
    return document.object();
}
double depthAt(const QImage &image, QPoint point)
{
    const auto pixel = image.pixelColor(point);
    return (pixel.red() * 65536 + pixel.green() * 256 + pixel.blue()) / 16777215.0;
}
QPoint project(const Input &input, QVector3D world, QSize size)
{
    const auto clip = input.scene.projection * input.scene.view * QVector4D(world, 1);
    return { qRound((clip.x() / clip.w() + 1) * .5 * size.width()),
             qRound((1 - clip.y() / clip.w()) * .5 * size.height()) };
}
void graph(const Capture &capture)
{
    const auto state = capture.metadata["opticalGraph"].toObject();
    for (const char *field :
         { "captureBindingMatches", "captureSeparateFromComposite", "sharedOpaqueDepth",
           "depthContentsPreserved", "captureMips", "colorDepthTest", "colorDepthEqual",
           "colorCullBack", "colorBlendingDisabled", "visibilityDepthWrite",
           "visibilityDepthLessEqual" })
        check(state[field].toBool(), field);
    check(!state["colorDepthWrite"].toBool(), "Optical shading writes visibility depth");
    check(capture.metadata["opticalEvents"].toArray()
                  == QJsonArray{ "opaque", "opaque-resolved", "capture-mips", "composite-copy",
                                 "visibility", "shell-shading" },
          "Actual optical pass ordering changed");
    for (const auto &kind : capture.metadata["captureDrawKinds"].toArray())
        check(kind.toInt() >= -1 && kind.toInt() <= 2,
              "A shell entered opaque transmission capture");
    const auto sorted = capture.metadata["sortedShells"].toArray();
    for (int index = 1; index < sorted.size(); ++index) {
        const auto prior = sorted[index - 1].toObject(), next = sorted[index].toObject();
        const double a = prior["depth"].toDouble(), b = next["depth"].toDouble();
        check(std::isfinite(a) && std::isfinite(b), "Nonfinite sort depth");
        const auto idA = prior["id"].toInteger(), idB = next["id"].toInteger();
        check(a > b
                      || (a == b
                          && (idA < idB
                              || (idA == idB
                                  && prior["surface"].toInt() < next["surface"].toInt()))),
              "Shell sort is not global depth/stable ID/surface order");
    }
}
double directReference(bool multiscatter)
{
    constexpr double roughness = .4, f0 = .04;
    const double fresnel = f0 + (1 - f0) * std::exp2(-5.55473 - 6.98316);
    double result = fresnel * .25 / (std::acos(-1.0) * std::pow(roughness, 4));
    if (multiscatter) {
        QFile file(":/rhi/environment/dfg.rgba32f");
        check(file.open(QIODevice::ReadOnly), "Missing source DFG input");
        const auto bytes = file.readAll();
        check(bytes.size() == 4096, "Unexpected DFG layout");
        const auto value = [&](int x, int channel) {
            const auto bits = qFromLittleEndian<quint32>(bytes.constData()
                                                         + ((15 * 16 + x) * 4 + channel) * 4);
            float decoded;
            std::memcpy(&decoded, &bits, 4);
            return double(decoded);
        };
        const double x = roughness * 16 - .5, amount = x - std::floor(x);
        const double a = value(int(x), 0) * (1 - amount) + value(int(x) + 1, 0) * amount;
        const double b = value(int(x), 1) * (1 - amount) + value(int(x) + 1, 1) * amount;
        const double single = f0 * a + b;
        const double average = f0 + (1 - f0) * .047619;
        const double compensation = std::pow(1 - a - b, 2);
        result += single * single * average / (1 - compensation * average + 1e-6) * compensation;
    }
    return result;
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (argc != 3)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString fixturePath = QString::fromLocal8Bit(argv[1]);
        const QString output = QString::fromLocal8Bit(argv[2]);
        const auto source = read(fixturePath);
        const auto cases = source["cases"].toArray();
        check(cases.size() == 61, "Incomplete source shell states");
        QDir().mkpath(output);
        RhiCaptureHarness driver;
        check(driver.window->devicePixelRatio() == 1, "Shell diagnostics require actual DPR1");
        QJsonArray results, sorting, isolation, channels, probes, rebuilds, alpha;
        double worstProjection = 0;
        for (const auto &value : cases) {
            const auto state = value.toObject();
            const QSize size(state["width"].toInt(), state["height"].toInt());
            auto input = fixture(state);
            const auto gpu = driver.capture(input, size);
            check(gpu.metadata["msaaSamples"].toInt() == input.captureSamples,
                  "Capture MSAA silently changed");
            if (input.shells)
                graph(gpu);
            const int sourceIndex = state["sourceIndex"].toInt();
            for (const auto &point : state["projections"].toArray()) {
                const auto sample = point.toObject();
                const auto world = vector(sample["world"].toArray());
                const auto clip = input.scene.projection * input.scene.view * QVector4D(world, 1);
                const auto expected = sample["pixel"].toArray();
                const double error = std::hypot(
                        (clip.x() / clip.w() + 1) * .5 * size.width() - expected[0].toDouble(),
                        (1 - clip.y() / clip.w()) * .5 * size.height() - expected[1].toDouble());
                check(std::isfinite(error) && error <= 2, "Source projection changed");
                worstProjection = std::max(worstProjection, error);
            }
            const auto image = input.rawOutput ? gpu.composite : driver.surface->grab();
            check(!image.isNull() && image.save(output + QString("/state-%1.png").arg(sourceIndex)),
                  "Missing own shell output");
            if (sourceIndex == 22 || sourceIndex == 25 || sourceIndex >= 62) {
                auto reversed = input;
                std::reverse(reversed.entities.begin(), reversed.entities.end());
                std::reverse(reversed.entityIds.begin(), reversed.entityIds.end());
                const auto reverse = driver.capture(reversed, size);
                graph(reverse);
                check(reverse.metadata["sortedShells"] == gpu.metadata["sortedShells"],
                      "Stable shell identities changed with submission order");
                sorting.append(sourceIndex);
            }
            if (sourceIndex == 21 || sourceIndex == 25) {
                auto opaque = input;
                opaque.shells = false;
                const auto captured = driver.capture(opaque, size);
                check(captured.metadata["captureDrawKinds"] == gpu.metadata["captureDrawKinds"],
                      "Opaque command kinds depend on optical drawing");
                isolation.append(sourceIndex);
            }
            results.append(
                    QJsonObject{ { "sourceIndex", sourceIndex }, { "metadata", gpu.metadata } });
        }
        const auto original =
                read(QString(FIXTURE_ROOT) + "/tests/fixtures/render-backend/visual.json")["cases"]
                        .toArray();
        for (int index : { 58, 59 }) {
            auto input = fixture(original[index].toObject());
            input.geometryDiagnostic = true;
            input.shells = false;
            input.captureSamples = 1;
            const auto gpu = driver.capture(input, { 960, 720 });
            int pixels = 0;
            double normalError = 0;
            for (int y = 0; y < 720; ++y)
                for (int x = 0; x < 960; ++x) {
                    if (depthAt(gpu.depth, { x, y }) >= 1)
                        continue;
                    const auto color = gpu.normal.pixelColor(x, y);
                    const QVector3D normal(2 * color.redF() - 1, 2 * color.greenF() - 1,
                                           2 * color.blueF() - 1);
                    normalError = std::max(normalError, std::abs(double(normal.length()) - 1));
                    ++pixels;
                }
            check(pixels > 100 && normalError <= std::sqrt(3.0) / 255 + 1e-6,
                  "Neighbor normal/depth target is invalid");
            channels.append(QJsonObject{ { "sourceIndex", index },
                                         { "normalError", normalError },
                                         { "foregroundPixels", pixels } });
        }
        auto tied = fixture(cases[18].toObject());
        tied.entities = { QMatrix4x4(), QMatrix4x4() };
        tied.entities[0].translate(-.3f, 0, 0);
        tied.entities[1].translate(.3f, 0, 0);
        tied.entityIds = { 20, 10 };
        const auto tie = driver.capture(tied, { 960, 720 });
        std::reverse(tied.entities.begin(), tied.entities.end());
        std::reverse(tied.entityIds.begin(), tied.entityIds.end());
        const auto reversedTie = driver.capture(tied, { 960, 720 });
        check(tie.metadata["sortedShells"] == reversedTie.metadata["sortedShells"],
              "Equal-depth identity tie is unstable");
        auto duplicate = tied;
        duplicate.entityIds = { 10, 10 };
        bool rejected = false;
        try {
            driver.item->submit(duplicate, { 960, 720 });
        } catch (const std::exception &) {
            rejected = true;
        }
        check(rejected, "Duplicate stable identity accepted");
        auto base = fixture(cases[0].toObject());
        base.shadows = false;
        base.entities = { QMatrix4x4() };
        base.entityIds = { 1 };
        base.scene.caseTransform.setToIdentity();
        base.scene.camera.position = { 0, 1.85, 9 };
        base.scene.view.setToIdentity();
        base.scene.view.lookAt({ 0, 1.85f, 9 }, { 0, 1.85f, 0 }, { 0, 1, 0 });
        base.scene.projection.setToIdentity();
        base.scene.projection.perspective(32, 960.0f / 720, 5, 300);
        base.captureSamples = 1;
        base.lightIntensities = { 1, 0, 0 };
        base.keyColor = { 1, 1, 1 };
        base.spotIntensity = 0;
        auto boundary = base;
        boundary.shells = false;
        boundary.entities.clear();
        boundary.entityIds.clear();
        boundary.showPaper = true;
        boundary.lightingEnabled = false;
        boundary.background = { .8f, .8f, .8f };
        boundary.cover = QImage(64, 64, QImage::Format_RGBA8888);
        boundary.cover.fill(QColor(210, 40, 80));
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                boundary.cover.setPixelColor(x, y, Qt::transparent);
        const double inkRed = std::pow((210.0 / 255 + .055) / 1.055, 2.4);
        for (int samples : { 1, 4 }) {
            boundary.captureSamples = samples;
            const auto gpu = driver.capture(boundary, { 960, 720 });
            const auto center = project(boundary, { .17f, 1.85f, .027f }, { 960, 720 });
            const double centerError = std::abs(gpu.color.pixelColor(center).redF() - inkRed);
            const float span = 3.06f * (1 - 2.0f / 64);
            const auto hole =
                    project(boundary,
                            { .17f + (.0625f - .5f) * span, 1.85f + (.5f - .0625f) * span, .027f },
                            { 960, 720 });
            check(centerError <= 1.0 / 255 && depthAt(gpu.depth, hole) == 1,
                  "Paper MSAA center or transparent depth is incorrect");
            int partial = 0, fringe = 0;
            for (int y = 0; y < 720; ++y)
                for (int x = 0; x < 960; ++x) {
                    const double red = gpu.color.pixelColor(x, y).redF();
                    partial += red > inkRed + 1.0 / 255 && red < .8 - 1.0 / 255;
                    // Preserve the original one-code output bound in the actual RGBA8 diagnostic domain.
                    fringe += gpu.color.pixelColor(x, y).red() < qRound(inkRed * 255) - 1;
                }
            std::fprintf(stderr, "Alpha samples=%d partial=%d fringe=%d centerError=%g\n", samples,
                         partial, fringe, centerError);
            gpu.color.save(output + QString("/alpha-%1.png").arg(samples));
            check(fringe == 0 && (samples == 1 || partial > 100),
                  "Paper resolve lost partial coverage or introduced dark alpha fringe");
            alpha.append(QJsonObject{ { "samples", samples },
                                      { "centerError", centerError },
                                      { "transparentDepthUntouched", true },
                                      { "partialCoveragePixels", partial },
                                      { "darkFringePixels", fringe } });
        }
        qint64 opticalGeneration = -1;
        for (int probe = 1; probe <= 9; ++probe) {
            base.opticalProbe = probe;
            const int rotations = probe == 9 ? 2 : 1;
            for (int rotation = 0; rotation < rotations; ++rotation) {
                auto input = base;
                input.scene.view.rotate(rotation * 45.0f, { 0, 0, 1 });
                const auto gpu = driver.capture(input, { 960, 720 });
                const auto generation = gpu.metadata["resourceGeneration"].toInteger();
                if (opticalGeneration < 0)
                    opticalGeneration = generation;
                check(generation == opticalGeneration,
                      "Optical values or camera roll rebuilt unrelated render targets");
                const auto values = gpu.metadata["opticalProbeSample"].toArray();
                check(values.size() == 3, "Missing actual GPU optical math sample");
                double expected = 0;
                if (probe == 1) {
                    const double dot = 1.0 / 3;
                    expected = 4 * (2.0 / 3) * std::acos(dot)
                            / (2 * std::acos(-1.0) * std::sqrt(1 - dot * dot));
                } else if (probe == 3 || probe == 4 || probe == 6)
                    expected = directReference(probe != 3);
                else if (probe == 8)
                    expected = 1 / std::acos(-1.0);
                else if (probe == 9)
                    expected = 32 * .01 / std::sqrt(1.0001) * (rotation ? std::sqrt(.5) : 1);
                // Binary16 target rounding and source LTC polynomial approximation have distinct bounds.
                const double tolerance =
                        probe == 1 ? .001 : std::max(1e-6, std::abs(expected) * .001);
                double error = 0;
                for (const auto &sample : values)
                    error = std::max(error, std::abs(sample.toDouble() - expected));
                check(error <= tolerance, "Production GLSL optical math differs from source limit");
                probes.append(QJsonObject{ { "probe", probe },
                                           { "rotation", rotation * 45 },
                                           { "value", values },
                                           { "expected", expected },
                                           { "error", error },
                                           { "tolerance", tolerance } });
            }
        }
        base.opticalProbe = 0;
        for (int cycle = 0; cycle < 10; ++cycle) {
            driver.rebuild();
            const auto gpu = driver.capture(base, { 960, 720 });
            graph(gpu);
            check(driver.rebuilt(), "Shell graph was not actually rebuilt");
            check(depthAt(gpu.compositeDepth, { 480, 360 }) < 1, "Shell rebuild lost geometry");
            rebuilds.append(gpu.metadata);
        }
        check(sorting.size() == 6 && isolation.size() == 2, "Missing source shell subcases");
        const QJsonObject report{ { "round", "R19C2B2" },
                                  { "configuration", "Release" },
                                  { "passed", true },
                                  { "validation", "source-optics-graph-and-own-output" },
                                  { "qt", qVersion() },
                                  { "fixture", fixturePath },
                                  { "baseline", source["baseline"] },
                                  { "cases", results },
                                  { "sorting", sorting },
                                  { "captureIsolation", isolation },
                                  { "neighborChannels", channels },
                                  { "equalDepthTie", true },
                                  { "duplicateIdRejected", rejected },
                                  { "alphaResolve", alpha },
                                  { "opticalMath", probes },
                                  { "rebuilds", rebuilds },
                                  { "maxProjectionPixels", worstProjection },
                                  { "offscreen", driver.surface->diagnostics() },
                                  { "windowPolicy", verification::windowPolicy(*driver.window) } };
        QSaveFile file(output + "/runtime.json");
        check(file.open(QIODevice::WriteOnly), "Cannot open shell report");
        file.write(QJsonDocument(report).toJson());
        check(file.commit(), "Cannot commit shell report");
        std::puts("Optical source math, actual capture graph and stable ordering passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "RHI shells: %s\n", error.what());
        return 1;
    }
}
