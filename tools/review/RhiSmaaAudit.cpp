#include "Postprocess.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#include "SmaaReference.h"
#include <rhi/qrhi_platform.h>
#include <QOffscreenSurface>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDir>
#include <QSysInfo>
#include <cstring>
#include <cstdio>
using namespace yalahoo;
using namespace yalahoo::rhi;
using smaa_reference::Image;
using smaa_reference::Pixel;
namespace {
Image analytic(QSize size, int pattern)
{
    Image result(size);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            // Asymmetric patterns cover both directions, crossing edges, long searches,
            // local contrast suppression, clamp-to-edge, HDR and nonconstant alpha.
            bool high = false;
            switch (pattern) {
            case 0:
                high = false;
                break;
            case 1:
                high = x * 3 > y * 2 + 7;
                break;
            case 2:
                high = y * 3 > x * 2 + 11;
                break;
            case 3:
                high = (x / 7 + y / 11) % 2;
                break;
            case 4:
                high = x > 8 && x < 40 && y > 5 && y < 37;
                break;
            case 5:
                high = (x + y) % 2;
                break;
            default:
                high = x > size.width() / 2;
                break;
            }
            const double weak = pattern == 5 ? .04 : .9;
            result.at(x, y) = { .15 + high * weak, .3 + high * weak * .5,
                                1.25 + ((pattern != 0 && x > size.width() / 2) ? .5 : 0),
                                .25 + .5 * double(pattern != 0 && y > size.height() / 3) };
            if (pattern == 6) {
                // A .16 edge adjacent to .9 must be suppressed by local contrast.
                const double level = x == size.width() / 2 ? .16 : (high ? .9 : 0);
                result.at(x, y) = { .15 + level, .3, 1.25, .5 };
            }
        }
    }
    result.quantize();
    return result;
}
QByteArray encode(const Image &input, bool yUp)
{
    QByteArray bytes(input.size.width() * input.size.height() * 8, '\0');
    for (int y = 0; y < input.size.height(); ++y) {
        const int row = yUp ? y : input.size.height() - 1 - y;
        for (int x = 0; x < input.size.width(); ++x) {
            const auto &p = input.pixels[y * input.size.width() + x];
            for (int c = 0; c < 4; ++c) {
                const qfloat16 half{ float(p[c]) };
                std::memcpy(bytes.data() + (row * input.size.width() + x) * 8 + c * 2, &half, 2);
            }
        }
    }
    return bytes;
}
Image decode(const QRhiReadbackResult &readback, QSize size, bool yUp)
{
    require(readback.pixelSize == size && readback.format == QRhiTexture::RGBA16F
                    && readback.data.size() == size.width() * size.height() * 8,
            "SMAA must retain full-size linear RGBA16F targets");
    Image result(size);
    for (int y = 0; y < size.height(); ++y) {
        const int row = yUp ? y : size.height() - 1 - y;
        for (int x = 0; x < size.width(); ++x) {
            for (int c = 0; c < 4; ++c) {
                qfloat16 half;
                std::memcpy(&half, readback.data.constData() + (row * size.width() + x) * 8 + c * 2,
                            2);
                const double value = float(half);
                require(std::isfinite(value), "Nonfinite SMAA output");
                result.at(x, y)[c] = value;
            }
        }
    }
    return result;
}
double error(const Image &expected, const Image &actual)
{
    double maximum = 0;
    for (size_t i = 0; i < expected.pixels.size(); ++i)
        for (int c = 0; c < 4; ++c)
            maximum = std::max(maximum, std::abs(expected.pixels[i][c] - actual.pixels[i][c]));
    return maximum;
}
QJsonObject verify(QRhi *api, Postprocess &post, QRhiTexture *texture, const Image &input,
                   const QString &output, int index)
{
    const auto &assets = preparedAssets();
    const auto area = assets.images.value(":/rhi/postprocess/smaa-area.png");
    const auto search = assets.images.value(":/rhi/postprocess/smaa-search.png");
    require(area.size() == QSize(160, 560) && search.size() == QSize(66, 33),
            "SMAA numeric lookup dimensions changed");
    const auto expectedEdges = smaa_reference::edges(input);
    const auto expectedWeights = smaa_reference::weights(expectedEdges, area, search);
    const auto expectedOutput = smaa_reference::blend(input, expectedWeights);
    QRhiCommandBuffer *cb = nullptr;
    require(api->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess, "Cannot begin SMAA frame");
    auto *updates = api->nextResourceUpdateBatch();
    updates->uploadTexture(texture,
                           { { 0, 0,
                               QRhiTextureSubresourceUploadDescription(
                                       encode(input, api->isYUpInFramebuffer())) } });
    cb->resourceUpdate(updates);
    QElapsedTimer timer;
    timer.start();
    post.render(cb);
    const auto submitNs = timer.nsecsElapsed();
    require(post.uniformUploadBytes() == 240, "SMAA uploaded unused uniform capacity");
    std::array<QRhiReadbackResult, 3> reads;
    const std::array targets{ post.smaaEdges(), post.smaaWeights(), post.output() };
    require(targets[0] != targets[1] && targets[0] != targets[2] && targets[1] != targets[2]
                    && targets[2] != texture,
            "SMAA targets alias or feed back into input");
    auto *batch = api->nextResourceUpdateBatch();
    for (int i = 0; i < 3; ++i)
        batch->readBackTexture(QRhiReadbackDescription(targets[i]), &reads[i]);
    cb->resourceUpdate(batch);
    require(api->endOffscreenFrame() == QRhi::FrameOpSuccess, "Cannot complete SMAA frame");
    const auto actualEdges = decode(reads[0], input.size, api->isYUpInFramebuffer());
    const auto actualWeights = decode(reads[1], input.size, api->isYUpInFramebuffer());
    const auto actualOutput = decode(reads[2], input.size, api->isYUpInFramebuffer());
    const double edgeError = error(expectedEdges, actualEdges);
    const double weightError = error(expectedWeights, actualWeights);
    const double blendError = error(expectedOutput, actualOutput);
    if (weightError > 3.0 / 255) {
        int count = 0;
        for (int y = 0; y < input.size.height() && count < 10; ++y) {
            for (int x = 0; x < input.size.width() && count < 10; ++x) {
                const auto &e = expectedWeights.pixels[y * input.size.width() + x];
                const auto &a = actualWeights.pixels[y * input.size.width() + x];
                if (std::abs(e[0] - a[0]) + std::abs(e[1] - a[1]) + std::abs(e[2] - a[2])
                            + std::abs(e[3] - a[3])
                    > .02) {
                    std::fprintf(stderr, "weight %d,%d expected %g %g %g %g actual %g %g %g %g\n",
                                 x, y, e[0], e[1], e[2], e[3], a[0], a[1], a[2], a[3]);
                    ++count;
                }
            }
        }
    }
    int softened = 0;
    QImage preview(input.size, QImage::Format_RGBA8888);
    for (int y = 0; y < input.size.height(); ++y) {
        for (int x = 0; x < input.size.width(); ++x) {
            const auto &a = actualOutput.pixels[y * input.size.width() + x];
            const auto &original = input.pixels[y * input.size.width() + x];
            softened += smaa_reference::delta(a, original) > .01;
            preview.setPixelColor(x, input.size.height() - 1 - y,
                                  QColor::fromRgbF(std::clamp(a[0], 0.0, 1.0),
                                                   std::clamp(a[1], 0.0, 1.0),
                                                   std::clamp(a[2], 0.0, 1.0)));
        }
    }
    std::fprintf(stderr, "SMAA case=%d %dx%d edges=%g weights=%g blend=%g softened=%d\n", index,
                 input.size.width(), input.size.height(), edgeError, weightError, blendError,
                 softened);
    require(edgeError <= 1.0 / 255, "SMAA edges differ from source formula");
    require(weightError <= 3.0 / 255, "SMAA search/area weights differ from source formula");
    require(blendError <= 3.0 / 255, "SMAA gamma/alpha blending differs from source formula");
    if (index < 6)
        require(preview.save(output + QString("/pattern-%1.png").arg(index)),
                "Cannot save own SMAA output");
    return { { "index", index },
             { "width", input.size.width() },
             { "height", input.size.height() },
             { "edgeError", edgeError },
             { "weightError", weightError },
             { "blendError", blendError },
             { "softenedPixels", softened },
             { "checkedPixels", input.size.width() * input.size.height() },
             { "cpuSubmitNs", submitNs },
             { "uniformUploadBytes", post.uniformUploadBytes() } };
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create SMAA report directory");
        preparedAssets();
        std::unique_ptr<QOffscreenSurface> fallback;
        std::unique_ptr<QRhi> api;
        bool useOpenGl = qgetenv("QSG_RHI_BACKEND") == "opengl";
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)
        useOpenGl = true;
#endif
        if (useOpenGl) {
#if QT_CONFIG(opengl)
            QRhiGles2InitParams params;
            fallback.reset(QRhiGles2InitParams::newFallbackSurface());
            params.fallbackSurface = fallback.get();
            api.reset(QRhi::create(QRhi::OpenGLES2, &params));
#else
            require(false, "Selected Qt Kit has no OpenGL backend");
#endif
        } else {
#if defined(Q_OS_MACOS) && QT_CONFIG(metal)
            QRhiMetalInitParams params;
            api.reset(QRhi::create(QRhi::Metal, &params));
#elif defined(Q_OS_WIN)
            QRhiD3D11InitParams params;
            api.reset(QRhi::create(QRhi::D3D11, &params));
#endif
        }
        require(bool(api), "Cannot create offscreen SMAA backend");
        QJsonArray rows;
        int totalSoftened = 0, index = 0;
        for (const auto size : { QSize(64, 64), QSize(129, 73), QSize(73, 129) }) {
            std::unique_ptr<QRhiTexture> texture(api->newTexture(QRhiTexture::RGBA16F, size));
            require(texture->create(), "Cannot create analytic SMAA input");
            PostSettings settings;
            settings.size = size;
            settings.smaa = true;
            Postprocess post;
            quint64 revision = 0;
            // Return to flat after active edges to reject stale reused edge/weight targets.
            for (int sequence = 0; sequence < 8; ++sequence) {
                const int pattern = sequence % 7;
                settings.focus += 1;
                post.prepare(api.get(), settings, texture.get(), texture.get(), texture.get(),
                             texture.get());
                if (!revision)
                    revision = post.revision();
                require(post.revision() == revision,
                        "Input/focus rebuilt unrelated SMAA resources");
                require(post.passes() == QJsonArray{ "smaa-edges", "smaa-weights", "smaa-blend" },
                        "SMAA graph changed");
                auto row = verify(api.get(), post, texture.get(), analytic(size, pattern), output,
                                  index++);
                totalSoftened += row["softenedPixels"].toInt();
                rows.append(row);
            }
            ResourceInventory inventory;
            post.inventory(inventory);
            require(inventory.entries().size() == 8, "SMAA allocated unrelated resources");
            require(inventory.bytes()
                            == qint64(size.width()) * size.height() * 24 + (160 * 560 + 66 * 33) * 4
                                    + 1104 * 3,
                    "SMAA formats/samples/lookup mip allocation changed");
            if (size == QSize(73, 129)) {
                for (int cycle = 0; cycle < 10; ++cycle) {
                    const auto before = post.revision();
                    post.reset();
                    post.prepare(api.get(), settings, texture.get(), texture.get(), texture.get(),
                                 texture.get());
                    require(post.revision() > before, "SMAA reset did not rebuild resources");
                    auto row = verify(api.get(), post, texture.get(), analytic(size, cycle % 6),
                                      output, index++);
                    row["resourceRebuild"] = cycle + 1;
                    rows.append(row);
                }
            }
            settings.smaa = false;
            post.prepare(api.get(), settings, texture.get(), texture.get(), texture.get(),
                         texture.get());
            ResourceInventory disabled;
            post.inventory(disabled);
            require(post.output() == texture.get() && post.passes().isEmpty()
                            && disabled.entries().isEmpty(),
                    "Disabled SMAA retains targets or lookup textures");
        }
        require(totalSoftened > 0, "SMAA did not smooth any analytic edge");
        for (const auto *window : QGuiApplication::topLevelWindows())
            require(!window->isVisible(), "SMAA exposed a native window");
        require(!QGuiApplication::focusWindow(), "SMAA focused a window");
        QSaveFile report(output + "/runtime.json");
        require(report.open(QIODevice::WriteOnly), "Cannot open SMAA report");
        report.write(
                QJsonDocument(
                        QJsonObject{
                                { "passed", true },
                                { "round", "R19D4B1" },
                                { "qt", qVersion() },
                                { "backend", int(api->backend()) },
                                { "device", QString::fromUtf8(api->driverInfo().deviceName) },
                                { "system", QSysInfo::prettyProductName() },
                                { "cpu", QSysInfo::currentCpuArchitecture() },
                                { "framebufferYUp", api->isYUpInFramebuffer() },
                                { "topLevelWindows", QGuiApplication::topLevelWindows().size() },
                                { "visibleWindows", 0 },
                                { "measurement",
                                  "CPU command recording only; no GPU/present/VSYNC timing" },
                                { "cases", rows } })
                        .toJson());
        require(report.commit(), "Cannot save SMAA report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
