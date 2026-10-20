#include "RhiCaptureHarness.h"
#include "RhiReviewInput.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDir>
#include <cstdio>
#include <QtEndian>
#include <cstring>
#include <cmath>
#include <algorithm>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
QVector<float> values(const QString &name)
{
    QFile file(":/rhi/environment/"_L1 + name + ".rgba32f"_L1);
    RhiCaptureHarness::require(file.open(QIODevice::ReadOnly), "Missing AO source values");
    const auto bytes = file.readAll();
    RhiCaptureHarness::require(bytes.size() % 4 == 0, "Malformed AO source floats");
    QVector<float> result;
    for (int offset = 0; offset < bytes.size(); offset += 4) {
        const auto bits = qFromLittleEndian<quint32>(bytes.constData() + offset);
        float value;
        std::memcpy(&value, &bits, 4);
        result.append(value);
    }
    return result;
}
float depth(const QImage &image, int x, int y)
{
    const auto p = image.pixelColor(x, y);
    return float(p.red() * 65536 + p.green() * 256 + p.blue()) / 16777215.0f;
}
QJsonObject sourceMath(const Input &input, const Capture &capture)
{
    const auto &image = capture.rawAo;
    const int width = image.width(), height = image.height(), count = input.aoSamples;
    const auto kernel = values("ssao-kernel-"_L1 + QString::number(count));
    const auto noise = values("ssao-noise-"_L1 + QString::number(count));
    RhiCaptureHarness::require(kernel.size() == count * 4 && noise.size() == 64,
                               "AO source kernel/noise dimensions differ");
    int checked = 0, contacts = 0;
    double maximum = 0, blurError = 0, compositeError = 0;
    for (int row = 1; row < 10; ++row)
        for (int column = 1; column < 10; ++column) {
            const int x = column * width / 10, y = row * height / 10;
            const auto encoded = capture.aoDepth.pixelColor(x, y);
            float expected = 1;
            if (encoded != QColor(Qt::white)) {
                const float near = float(input.scene.nearPlane), far = float(input.scene.farPlane);
                const float z = -(near + depth(capture.aoDepth, x, y) * (far - near));
                const QVector3D position(
                        (2 * (x + .5f) / width - 1) * -z / input.scene.projection(0, 0),
                        (1 - 2 * (y + .5f) / height) * -z / input.scene.projection(1, 1), z);
                const auto n = capture.aoNormal.pixelColor(x, y);
                const QVector3D normal(2 * n.redF() - 1, 2 * n.greenF() - 1, 2 * n.blueF() - 1);
                const float scalar = noise[(((height - y - 1) % 4) * 4 + x % 4) * 4];
                const QVector3D random(scalar, scalar, scalar);
                const auto tangent =
                        (random - normal * QVector3D::dotProduct(random, normal)).normalized();
                const auto bitangent = QVector3D::crossProduct(normal, tangent);
                int occlusion = 0;
                for (int sample = 0; sample < count; ++sample) {
                    const auto point = position
                            + (tangent * kernel[sample * 4] + bitangent * kernel[sample * 4 + 1]
                               + normal * kernel[sample * 4 + 2])
                                    * input.aoRadius;
                    const auto clip = input.scene.projection * QVector4D(point, 1);
                    const float u = clip.x() / clip.w() * .5f + .5f;
                    const float v = clip.y() / clip.w() * .5f + .5f;
                    const int sx = std::clamp(int(std::floor(u * width)), 0, width - 1);
                    const int sy = std::clamp(int(std::floor((1 - v) * height)), 0, height - 1);
                    const float delta =
                            (-point.z() - near) / (far - near) - depth(capture.aoDepth, sx, sy);
                    occlusion += delta > .001f && delta < .035f;
                }
                expected = 1 - float(occlusion) / count;
                contacts += occlusion > 0;
            }
            const double error = std::abs(image.pixelColor(x, y).redF() - expected);
            maximum = std::max(maximum, error);
            if (error > 1.0 / 255)
                std::fprintf(stderr, "AO math (%d,%d) expected %.9f actual %.9f\n", x, y, expected,
                             image.pixelColor(x, y).redF());
            RhiCaptureHarness::require(error <= 1.0 / 255, "Source SSAO numerical sample differs");
            double blur = 0;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const auto raw = capture.rawAo.pixelColor(std::clamp(x + dx, 0, width - 1),
                                                              std::clamp(y + dy, 0, height - 1));
                    // Source N/count levels are exactly representable in binary16; RGBA8
                    // display quantization uniquely identifies each level for count<=64.
                    blur += std::round(raw.redF() * count) / count / 25;
                }
            const auto blurred = capture.blurredAo.pixelColor(x, y);
            blurError = std::max(blurError, std::abs(blurred.redF() - blur));
            const auto color = capture.color.pixelColor(x, y);
            const auto output = capture.postprocessed.pixelColor(x, y);
            for (int channel = 0; channel < 3; ++channel) {
                const float incoming[] = { color.redF(), color.greenF(), color.blueF() };
                const float outgoing[] = { output.redF(), output.greenF(), output.blueF() };
                compositeError =
                        std::max(compositeError,
                                 std::abs(outgoing[channel]
                                          - incoming[channel] * (.78 + .22 * blurred.redF())));
            }
            ++checked;
        }
    RhiCaptureHarness::require(blurError <= 1.0 / 255, "Source 5x5 AO blur differs");
    RhiCaptureHarness::require(compositeError <= 2.0 / 255, "Source .22 AO composite differs");
    return { { u"blurError"_s, blurError },
             { u"compositeError"_s, compositeError },
             { u"points"_s, checked },
             { u"contacts"_s, contacts },
             { u"maximumError"_s, maximum } };
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
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        RhiCaptureHarness::require(QDir().mkpath(output), "Cannot create AO output");
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/rhi-shell/visual.json"_L1);
        RhiCaptureHarness::require(file.open(QIODevice::ReadOnly), "Missing source input");
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()[u"cases"_s].toArray();
        auto input = fixture(cases[0].toObject());
        input.poolFrame = true;
        input.entities.clear();
        input.entityIds.clear();
        input.shells = false;
        input.shadows = false;
        input.showPaper = false;
        input.postprocess = true;
        input.bokeh = false;
        input.smaa = false;
        RhiCaptureHarness driver;
        QJsonArray results;
        for (int count : { 16, 32, 64 }) {
            input.aoSamples = count;
            for (QSize size : { QSize(128, 17), QSize(129, 17), QSize(213, 159) }) {
                std::fprintf(stderr, "AO samples=%d target=%dx%d\n", count, size.width(),
                             size.height());
                const auto capture = driver.capture(input, size);
                const QSize aoSize = size;
                int scalarTargets = 0;
                for (const auto &entry :
                     capture.metadata[u"resources"_s].toObject()[u"entries"_s].toArray()) {
                    const auto resource = entry.toObject();
                    if (resource[u"name"_s] == u"post-ao-raw"_s
                        || resource[u"name"_s] == u"post-ao-blur"_s) {
                        RhiCaptureHarness::require(
                                resource[u"format"_s].toInt() == int(QRhiTexture::R16F)
                                        && resource[u"width"_s].toInt() == aoSize.width()
                                        && resource[u"height"_s].toInt() == aoSize.height(),
                                "Product scalar AO attachment changed");
                        ++scalarTargets;
                    }
                }
                RhiCaptureHarness::require(scalarTargets == 2, "Missing scalar AO attachments");
                for (const auto *image : { &capture.rawAo, &capture.blurredAo }) {
                    RhiCaptureHarness::require(image->size() == aoSize,
                                               "Scalar AO readback size changed");
                    for (int y = 0; y < image->height(); ++y)
                        for (int x = 0; x < image->width(); ++x)
                            RhiCaptureHarness::require(image->pixelColor(x, y) == QColor(Qt::white),
                                                       "Background source AO must be exactly one");
                }
                const auto replay = driver.capture(input, size);
                RhiCaptureHarness::require(
                        replay.metadata[u"alignedReadbackRevision"_s]
                                == capture.metadata[u"alignedReadbackRevision"_s],
                        "Identical scalar capture rebuilt alignment resources");
                RhiCaptureHarness::require(replay.metadata[u"postUniformUploadBytes"_s].toInteger()
                                                   == 240,
                                           "Steady AO passes reupload constant kernels");
                for (const auto &entry :
                     replay.metadata[u"resources"_s].toObject()[u"entries"_s].toArray())
                    RhiCaptureHarness::require(entry.toObject()[u"name"_s] != u"ao-kernel"_s,
                                               "Unused GPU kernel texture remains");
                results.append(capture.metadata);
            }
        }
        QFile contactFile((QString::fromUtf8(FIXTURE_ROOT))
                          + "/tests/fixtures/occlusion/visual.json"_L1);
        RhiCaptureHarness::require(contactFile.open(QIODevice::ReadOnly),
                                   "Missing source AO contact states");
        const auto contactCases =
                QJsonDocument::fromJson(contactFile.readAll()).object()[u"cases"_s].toArray();
        QJsonArray mathematics;
        for (int count : { 16, 32, 64 }) {
            auto contact = fixture(contactCases[80].toObject());
            contact.postprocess = true;
            contact.aoSamples = count;
            contact.aoRadius = .38f;
            contact.shells = false;
            contact.captureSamples = 1;
            const auto capture = driver.capture(contact, { 960, 720 });
            auto math = sourceMath(contact, capture);
            math[u"samples"_s] = count;
            mathematics.append(math);
            RhiCaptureHarness::require(math[u"contacts"_s].toInt() > 0,
                                       "Source contact probes missed AO");
        }
        for (int cycle = 0; cycle < 10; ++cycle) {
            driver.rebuild();
            const auto restored = driver.capture(input, { 213, 159 });
            RhiCaptureHarness::require(
                    driver.rebuilt() && restored.rawAo.pixelColor(100, 100) == QColor(Qt::white),
                    "AO readback cache did not survive real graph rebuild");
        }
        QSaveFile report(output + "/runtime.json"_L1);
        RhiCaptureHarness::require(report.open(QIODevice::WriteOnly), "Cannot write AO report");
        report.write(QJsonDocument(QJsonObject{ { u"round"_s, u"R19D1"_s },
                                                { u"passed"_s, true },
                                                { u"cases"_s, results },
                                                { u"sourceMath"_s, mathematics },
                                                { u"windowPolicy"_s,
                                                  verification::windowPolicy(*driver.window) },
                                                { u"offscreen"_s, driver.surface->diagnostics() } })
                             .toJson());
        RhiCaptureHarness::require(report.commit(), "Cannot save AO report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
