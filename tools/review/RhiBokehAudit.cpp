#include "OffscreenRhi.h"
#include "ReviewApplication.h"
#include <QQuickWindow>
#include "Postprocess.h"
#include "PreparedAssets.h"
#include "ResourceInventory.h"
#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#if defined(Q_OS_LINUX) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#  include <QVulkanInstance>
#endif
#include <rhi/qrhi_platform.h>
#include <QOffscreenSurface>
#include <QGuiApplication>
#include <QFloat16>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
namespace {
using Pixel = std::array<double, 4>;
struct Inputs final
{
    QSize size;
    QVector<Pixel> colors;
    QVector<double> depths;
    std::unique_ptr<QRhiTexture> color, depth;
    QByteArray colorBytes;
    QImage depthImage;
    Inputs(QRhi *api, QSize extent, float near, float far, float focus)
        : size(extent), depthImage(extent, QImage::Format_RGBA8888)
    {
        color.reset(api->newTexture(QRhiTexture::RGBA16F, size));
        depth.reset(api->newTexture(QRhiTexture::RGBA8, size));
        require(color->create() && depth->create(), "Cannot create analytic Bokeh input");
        colorBytes.resize(size.width() * size.height() * 8);
        for (int y = 0; y < size.height(); ++y) {
            for (int x = 0; x < size.width(); ++x) {
                const int i = y * size.width() + x;
                // Asymmetric HDR/color discontinuities expose flipped taps and premature tone mapping.
                Pixel pixel{ .15 + (x % 17) / 8.0, .1 + (y % 13) / 9.0,
                             ((x / 7 + y / 11) % 2) ? 1.75 : .25, .3 };
                for (int c = 0; c < 4; ++c) {
                    const qfloat16 half{ float(pixel[c]) };
                    pixel[c] = float(half);
                    std::memcpy(colorBytes.data() + i * 8 + c * 2, &half, 2);
                }
                colors.append(pixel);
                const double distances[]{ near, focus - 5, focus,    focus + 5,
                                          far,  focus - 2, focus + 2 };
                const auto normalized = std::clamp(
                        (distances[x * 7 / size.width()] - near) / (far - near), 0.0, 1.0);
                const auto encoded = qRound(normalized * 16777215.0);
                depthImage.setPixelColor(
                        x, y,
                        QColor((encoded >> 16) & 255, (encoded >> 8) & 255, encoded & 255, 255));
                depths.append(double(encoded) / 16777215.0);
            }
        }
    }
    void upload(QRhiResourceUpdateBatch *batch)
    {
        batch->uploadTexture(color.get(),
                             { { 0, 0, QRhiTextureSubresourceUploadDescription(colorBytes) } });
        batch->uploadTexture(depth.get(), depthImage);
    }
    Pixel sample(double u, double v) const
    {
        const double x = u * size.width() - .5, y = v * size.height() - .5;
        const int ix = int(std::floor(x)), iy = int(std::floor(y));
        const double fx = x - ix, fy = y - iy;
        Pixel result{ };
        for (int row = 0; row < 2; ++row) {
            for (int column = 0; column < 2; ++column) {
                const auto &p = colors[std::clamp(iy + row, 0, size.height() - 1) * size.width()
                                       + std::clamp(ix + column, 0, size.width() - 1)];
                const double weight = (row ? fy : 1 - fy) * (column ? fx : 1 - fx);
                for (int c = 0; c < 4; ++c)
                    result[c] += p[c] * weight;
            }
        }
        return result;
    }
};
QRhiReadbackResult capture(QRhi *api, Postprocess &post, Inputs &input)
{
    QRhiCommandBuffer *cb = nullptr;
    require(api->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess,
            "Cannot begin offscreen Bokeh frame");
    auto *updates = api->nextResourceUpdateBatch();
    input.upload(updates);
    cb->resourceUpdate(updates);
    post.render(cb);
    require(post.uniformUploadBytes() == 80, "Bokeh uploaded unused uniform capacity");
    const auto uploaded = post.submittedBokeh();
    require(uploaded[u"width"_s].toInt() == input.size.width()
                    && uploaded[u"height"_s].toInt() == input.size.height()
                    && uploaded[u"depthWidth"_s].toInt() == input.size.width()
                    && uploaded[u"depthHeight"_s].toInt() == input.size.height(),
            "Bokeh color/depth targets differ from actual uploaded dimensions");
    QRhiReadbackResult readback;
    auto *reads = api->nextResourceUpdateBatch();
    reads->readBackTexture(QRhiReadbackDescription(post.output()), &readback);
    cb->resourceUpdate(reads);
    require(api->endOffscreenFrame() == QRhi::FrameOpSuccess,
            "Cannot complete offscreen Bokeh frame");
    // QRhi offscreen endFrame completes these explicit readbacks. This diagnostic wait is
    // outside the production renderer; no window, polling or presentation timing is involved.
    return readback;
}
QJsonObject verify(const Inputs &input, const PostSettings &settings,
                   const QRhiReadbackResult &readback, const QJsonArray &taps, QImage &preview)
{
    require(readback.pixelSize == input.size && readback.format == QRhiTexture::RGBA16F
                    && readback.data.size() == input.size.width() * input.size.height() * 8,
            "Bokeh must retain full-size linear RGBA16F output");
    double maximum = 0;
    int checked = 0, saturated = 0, sharp = 0, foreground = 0, background = 0;
    preview = QImage(input.size, QImage::Format_RGBA8888);
    // Test every output pixel, including clamp-to-edge and all depth discontinuities.
    for (int y = 0; y < input.size.height(); ++y) {
        for (int x = 0; x < input.size.width(); ++x) {
            const int i = y * input.size.width() + x;
            const double z =
                    settings.nearPlane + input.depths[i] * (settings.farPlane - settings.nearPlane);
            const double coc = std::clamp((settings.focus - z) * settings.aperture, -.011, .011);
            sharp += std::abs(coc) < 1e-7;
            saturated += std::abs(coc) >= .010999;
            foreground += coc > 1e-7;
            background += coc < -1e-7;
            Pixel expected{ };
            for (const auto &value : taps) {
                const auto tap = value.toArray();
                const auto sample =
                        input.sample((x + .5) / input.size.width() + tap[0].toDouble() * coc,
                                     (y + .5) / input.size.height()
                                             + tap[1].toDouble() * coc * input.size.width()
                                                     / input.size.height());
                for (int c = 0; c < 3; ++c)
                    expected[c] += sample[c] / 41;
            }
            Pixel actual;
            for (int c = 0; c < 4; ++c) {
                qfloat16 half;
                std::memcpy(&half, readback.data.constData() + i * 8 + c * 2, 2);
                actual[c] = float(half);
                require(std::isfinite(actual[c]), "Nonfinite Bokeh output");
                if (c < 3)
                    maximum = std::max(maximum, std::abs(expected[c] - actual[c]));
            }
            require(actual[3] == 1, "Bokeh alpha must be one");
            preview.setPixelColor(x, y,
                                  QColor::fromRgbF(std::clamp(actual[0], 0.0, 1.0),
                                                   std::clamp(actual[1], 0.0, 1.0),
                                                   std::clamp(actual[2], 0.0, 1.0)));
            ++checked;
        }
    }
    std::fprintf(stderr, "Bokeh %dx%d focus=%g aperture=%g max=%g\n", input.size.width(),
                 input.size.height(), settings.focus, settings.aperture, maximum);
    require(maximum <= 3.0 / 255, "Bokeh differs from independent source sampling");
    return { { u"maxError"_s, maximum },
             { u"checkedPixels"_s, checked },
             { u"sharp"_s, sharp },
             { u"saturated"_s, saturated },
             { u"foreground"_s, foreground },
             { u"background"_s, background },
             { u"width"_s, input.size.width() },
             { u"height"_s, input.size.height() },
             { u"focus"_s, settings.focus },
             { u"aperture"_s, settings.aperture } };
}
}

namespace {
auto runBokehAudit(int &argc, char **&argv)
{
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create Bokeh report directory");
        QFile source((QString::fromUtf8(FIXTURE_ROOT))
                     + "/tests/fixtures/postprocess/bokeh-kernel.json"_L1);
        require(source.open(QIODevice::ReadOnly), "Missing locked Bokeh kernel");
        const auto taps = QJsonDocument::fromJson(source.readAll()).object()[u"taps"_s].toArray();
        require(taps.size() == 41, "Wrong source tap count");
        preparedAssets();
        yalahoo::review::OffscreenRhi backend;
        auto *api = backend.get();
        QJsonArray results;
        int totalSharp = 0, totalForeground = 0, totalBackground = 0, totalSaturated = 0;
        for (const auto size : { QSize(129, 73), QSize(257, 193), QSize(193, 257) }) {
            Inputs input(api, size, 5, 300, 25);
            Postprocess post;
            PostSettings settings;
            settings.size = size;
            settings.bokeh = true;
            quint64 stableRevision = 0;
            for (const float focus : { 25.0f, 20.0f, 30.0f }) {
                for (const float aperture : { 0.0f, .0003f, .0008f, .0012f }) {
                    settings.focus = focus;
                    settings.aperture = aperture;
                    post.prepare(api, settings, input.color.get(), input.depth.get(),
                                 input.depth.get(), input.depth.get());
                    if (!stableRevision)
                        stableRevision = post.revision();
                    require(stableRevision == post.revision(),
                            "Focus/aperture rebuilt Bokeh resources");
                    require(post.passes() == QJsonArray{ u"bokeh"_s }
                                    && post.output() != input.color.get(),
                            "Unexpected Bokeh graph or framebuffer feedback");
                    const auto readback = capture(api, post, input);
                    QImage preview;
                    const auto result = verify(input, settings, readback, taps, preview);
                    results.append(result);
                    totalSharp += result[u"sharp"_s].toInt();
                    totalForeground += result[u"foreground"_s].toInt();
                    totalBackground += result[u"background"_s].toInt();
                    totalSaturated += result[u"saturated"_s].toInt();
                    if (focus == 25 && aperture == .0008f)
                        require(preview.save(output
                                             + u"/bokeh-%1x%2.png"_s.arg(size.width())
                                                       .arg(size.height())),
                                "Cannot save own Bokeh output");
                }
            }
            if (size == QSize(193, 257)) {
                for (int cycle = 0; cycle < 10; ++cycle) {
                    const auto before = post.revision();
                    post.reset();
                    settings.nearPlane = .1f;
                    settings.farPlane = 1000;
                    settings.focus = 25;
                    settings.aperture = .0008f;
                    Inputs restored(api, size, settings.nearPlane, settings.farPlane, 25);
                    post.prepare(api, settings, restored.color.get(), restored.depth.get(),
                                 restored.depth.get(), restored.depth.get());
                    require(post.revision() > before, "Bokeh resource reset did not occur");
                    QImage preview;
                    auto row =
                            verify(restored, settings, capture(api, post, restored), taps, preview);
                    row[u"resourceRebuild"_s] = cycle + 1;
                    results.append(row);
                    // Release bindings while their source textures still exist.
                    post.reset();
                }
                post.prepare(api, settings, input.color.get(), input.depth.get(), input.depth.get(),
                             input.depth.get());
            }
            ResourceInventory inventory;
            post.inventory(inventory);
            require(inventory.entries().size() == 2,
                    "Bokeh allocated unused ping/pong or AO resources");
            settings.bokeh = false;
            post.prepare(api, settings, input.color.get(), input.depth.get(), input.depth.get(),
                         input.depth.get());
            ResourceInventory disabled;
            post.inventory(disabled);
            require(post.output() == input.color.get() && post.passes().isEmpty()
                            && disabled.entries().isEmpty(),
                    "Disabled Bokeh retains unused render targets");
        }
        require(totalSharp > 0 && totalForeground > 0 && totalBackground > 0 && totalSaturated > 0,
                "Bokeh source probes did not cover signed, sharp and saturated CoC");
        int visibleWindows = 0;
        for (const auto *window : QGuiApplication::topLevelWindows())
            visibleWindows += window->isVisible();
        require(visibleWindows == 0 && !QGuiApplication::focusWindow(),
                "Offscreen Bokeh verification exposed or focused a window");
        QSaveFile report(output + "/runtime.json"_L1);
        require(report.open(QIODevice::WriteOnly), "Cannot open Bokeh report");
        report.write(
                QJsonDocument(
                        QJsonObject{
                                { u"passed"_s, true },
                                { u"round"_s, u"R19D4A"_s },
                                { u"backend"_s, int(api->backend()) },
                                { u"qt"_s, QString::fromUtf8(qVersion()) },
                                { u"topLevelWindows"_s, QGuiApplication::topLevelWindows().size() },
                                { u"visibleWindows"_s, visibleWindows },
                                { u"measurement"_s,
                                  u"production Postprocess in real QRhi offscreen frames; no presentation timing"_s },
                                { u"cases"_s, results } })
                        .toJson());
        require(report.commit(), "Cannot save Bokeh report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        return runBokehAudit(argc, argv);
    });
}
