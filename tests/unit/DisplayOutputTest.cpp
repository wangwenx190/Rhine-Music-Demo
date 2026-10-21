#include <QQuickWindow>
#include "DisplayOutput.h"
#include "GraphicsBackend.h"
#include "VerificationWindow.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QJsonDocument>
#include "JsonFile.h"
#include <QFile>
#include <QOffscreenSurface>
#include <QDir>
#include <QFileInfo>
#if defined(Q_OS_LINUX) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#  include <QVulkanInstance>
#  include <QVersionNumber>
#endif
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <rhi/qshader.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
class ControlledMonitor final : public DisplayMonitor
{
    Q_DISABLE_COPY_MOVE(ControlledMonitor)
public:
    ControlledMonitor() = default;
    DisplayState state;
    DisplayState query(QWindow &) override
    {
        return state;
    }
    void publish(DisplayState value)
    {
        state = value;
        if (changed)
            changed();
    }
};
QShader loadShader(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { };
    return QShader::fromSerialized(file.readAll());
}
}
class DisplayOutputTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DisplayOutputTest)
public:
    DisplayOutputTest() = default;

private Q_SLOTS:
    void transferShader()
    {
#if defined(Q_OS_LINUX) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
        QVulkanInstance instance;
#endif
        std::unique_ptr<QRhi> api;
#if defined(Q_OS_MACOS)
        QRhiMetalInitParams params;
        api.reset(QRhi::create(QRhi::Metal, &params));
#elif defined(Q_OS_WIN)
        QRhiD3D11InitParams params;
        api.reset(QRhi::create(QRhi::D3D11, &params));
#elif defined(Q_OS_LINUX) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
        instance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        instance.setApiVersion(QVersionNumber(1, 1));
        QVERIFY(instance.create());
        QRhiVulkanInitParams params;
        params.inst = &instance;
        api.reset(QRhi::create(QRhi::Vulkan, &params));
#else
        QFAIL("Vulkan development support is required; no OpenGL test fallback");
#endif
        QVERIFY(api);
        const QSize size(5, 1);
        const std::array<float, 5> levels{ 0, .04045f, .5f, 1, 2 };
        std::array<qfloat16, 20> pixels;
        for (int p = 0; p < 5; ++p)
            for (int channel = 0; channel < 4; ++channel)
                pixels[p * 4 + channel] = qfloat16(channel == 3 ? 1 : levels[p]);
        const std::unique_ptr<QRhiTexture> source(api->newTexture(QRhiTexture::RGBA16F, size, 1));
        const std::unique_ptr<QRhiTexture> output(
                api->newTexture(QRhiTexture::RGBA16F, size, 1,
                                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        QVERIFY(source->create());
        QVERIFY(output->create());
        const std::unique_ptr<QRhiTextureRenderTarget> target(
                api->newTextureRenderTarget({ output.get() }));
        const std::unique_ptr<QRhiRenderPassDescriptor> pass(
                target->newCompatibleRenderPassDescriptor());
        target->setRenderPassDescriptor(pass.get());
        QVERIFY(target->create());
        const std::unique_ptr<QRhiSampler> sampler(
                api->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        QVERIFY(sampler->create());
        const std::unique_ptr<QRhiBuffer> uniform(
                api->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 80));
        QVERIFY(uniform->create());
        const std::unique_ptr<QRhiShaderResourceBindings> bindings(
                api->newShaderResourceBindings());
        bindings->setBindings({ QRhiShaderResourceBinding::uniformBuffer(
                                        0, QRhiShaderResourceBinding::FragmentStage, uniform.get()),
                                QRhiShaderResourceBinding::sampledTexture(
                                        1, QRhiShaderResourceBinding::FragmentStage, source.get(),
                                        sampler.get()) });
        QVERIFY(bindings->create());
        const std::unique_ptr<QRhiGraphicsPipeline> pipeline(api->newGraphicsPipeline());
        pipeline->setShaderStages(
                { { QRhiShaderStage::Vertex, loadShader(u":/display/output.vert.qsb"_s) },
                  { QRhiShaderStage::Fragment,
                    loadShader(u":/assets/shaders/display-output.frag.qsb"_s) } });
        pipeline->setShaderResourceBindings(bindings.get());
        pipeline->setRenderPassDescriptor(pass.get());
        QVERIFY(pipeline->create());
        for (const auto mode : { 0, 1, 2 }) {
            // Three scalar output encodings at one tiny extent, no scene matrix.
            const float scale = mode == 0 ? 1 : mode == 1 ? 2.5f : 203;
            std::array<char, 80> constants{ };
            const float opacity = 1;
            const int hdr10 = mode == 2;
            std::memcpy(constants.data() + 64, &opacity, 4);
            std::memcpy(constants.data() + 68, &scale, 4);
            std::memcpy(constants.data() + 72, &hdr10, 4);
            QRhiCommandBuffer *commands = nullptr;
            QCOMPARE(api->beginOffscreenFrame(&commands), QRhi::FrameOpSuccess);
            auto *updates = api->nextResourceUpdateBatch();
            updates->uploadTexture(source.get(),
                                   { { 0, 0,
                                       QRhiTextureSubresourceUploadDescription(QByteArray(
                                               reinterpret_cast<const char *>(pixels.data()),
                                               sizeof(pixels))) } });
            updates->updateDynamicBuffer(uniform.get(), 0, 80, constants.data());
            commands->beginPass(target.get(), Qt::black, { 1, 0 }, updates);
            commands->setGraphicsPipeline(pipeline.get());
            commands->setShaderResources(bindings.get());
            commands->setViewport({ 0, 0, 5, 1 });
            commands->setVertexInput(0, 0, nullptr);
            commands->draw(3);
            commands->endPass();
            QRhiReadbackResult read;
            bool completed = false;
            read.completed = [&] {
                completed = true;
            };
            auto *reads = api->nextResourceUpdateBatch();
            reads->readBackTexture({ output.get() }, &read);
            commands->resourceUpdate(reads);
            QCOMPARE(api->endOffscreenFrame(), QRhi::FrameOpSuccess);
            QVERIFY(completed);
            QCOMPARE(read.format, QRhiTexture::RGBA16F);
            QCOMPARE(read.data.size(), 40);
            for (int p = 0; p < 5; ++p) {
                const double encoded = float(pixels[p * 4]);
                double expected = encoded <= .04045 ? encoded / 12.92
                                                    : std::pow((encoded + .055) / 1.055, 2.4);
                expected *= scale;
                if (hdr10) {
                    const double light =
                            std::pow(std::clamp(expected / 10000, 0.0, 1.0), 2610.0 / 16384);
                    expected = std::pow((3424.0 / 4096 + 2413.0 / 128 * light)
                                                / (1 + 2392.0 / 128 * light),
                                        2523.0 / 32);
                }
                qfloat16 value;
                std::memcpy(&value, read.data.constData() + p * 8, 2);
                QVERIFY2(std::isfinite(float(value)) && std::abs(float(value) - expected) < .015,
                         qPrintable(QString::number(float(value)) + u" expected "_s
                                    + QString::number(expected)));
            }
        }
        // One production 3D output pass proves highlights survive the actual
        // shoulder and sRGB encoding in a floating target, without a 3D scene.
        const std::unique_ptr<QRhiShaderResourceBindings> sceneBindings(
                api->newShaderResourceBindings());
        sceneBindings->setBindings(
                { QRhiShaderResourceBinding::sampledTexture(
                          0, QRhiShaderResourceBinding::FragmentStage, source.get(), sampler.get()),
                  QRhiShaderResourceBinding::uniformBuffer(
                          1, QRhiShaderResourceBinding::FragmentStage, uniform.get()) });
        QVERIFY(sceneBindings->create());
        const std::unique_ptr<QRhiGraphicsPipeline> scenePipeline(api->newGraphicsPipeline());
        scenePipeline->setShaderStages(
                { { QRhiShaderStage::Vertex, loadShader(u":/display/output.vert.qsb"_s) },
                  { QRhiShaderStage::Fragment, loadShader(u":/display/output.frag.qsb"_s) } });
        scenePipeline->setShaderResourceBindings(sceneBindings.get());
        scenePipeline->setRenderPassDescriptor(pass.get());
        QVERIFY(scenePipeline->create());
        const std::array<float, 8> sceneConstants{ 0, 0, 1, 1.05f, 4, 0, 0, 0 };
        QRhiCommandBuffer *commands = nullptr;
        QCOMPARE(api->beginOffscreenFrame(&commands), QRhi::FrameOpSuccess);
        auto *updates = api->nextResourceUpdateBatch();
        updates->updateDynamicBuffer(uniform.get(), 0, sizeof(sceneConstants),
                                     sceneConstants.data());
        commands->beginPass(target.get(), Qt::black, { 1, 0 }, updates);
        commands->setGraphicsPipeline(scenePipeline.get());
        commands->setShaderResources(sceneBindings.get());
        commands->setViewport({ 0, 0, 5, 1 });
        commands->setVertexInput(0, 0, nullptr);
        commands->draw(3);
        commands->endPass();
        QRhiReadbackResult sceneRead;
        auto *reads = api->nextResourceUpdateBatch();
        reads->readBackTexture({ output.get() }, &sceneRead);
        commands->resourceUpdate(reads);
        QCOMPARE(api->endOffscreenFrame(), QRhi::FrameOpSuccess);
        QCOMPARE(sceneRead.data.size(), 40);
        float previous = -1;
        for (int p = 0; p < 5; ++p) {
            qfloat16 value;
            std::memcpy(&value, sceneRead.data.constData() + p * 8, 2);
            const float encoded = float(value);
            QVERIFY(std::isfinite(encoded) && encoded >= previous);
            const double linear =
                    encoded <= .04045 ? encoded / 12.92 : std::pow((encoded + .055) / 1.055, 2.4);
            QVERIFY(linear <= 4.01);
            if (p == 0)
                QVERIFY(linear < .001);
            if (p == 4)
                QVERIFY(linear > 1);
            previous = encoded;
        }
    }

private:
    static auto awaitPresentedFrame(QQuickWindow *&window, DisplayOutput &output, QSignalSpy &swaps,
                                    qsizetype before)
    {
        window->update();
        return QTest::qWaitFor(
                [&] {
                    return output.ready() && swaps.size() > before && window->isExposed();
                },
                5000);
    }
private Q_SLOTS:
    void nativeOutput()
    {
        QVERIFY(qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1);
        auto controlled = std::make_unique<ControlledMonitor>();
        auto *monitor = controlled.get();
        DisplayOutput output(std::move(controlled));
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {
            for (const auto &error : errors)
                warnings.append(error.toString());
        });
        engine.setInitialProperties({ { u"output"_s, QVariant::fromValue(&output) } });
        engine.loadFromModule("Yalahoo.DisplayTest", "DisplayOutputProbe");
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n'_L1)));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto platform = makeDisplayMonitor();
        const auto system = platform->query(*window);
        monitor->state = system;
        verification::prepareWindow(*window, true);
        output.attach(*window);
        QSignalSpy swaps(window, &QQuickWindow::frameSwapped);
        QSignalSpy invalidations(window, &QQuickWindow::sceneGraphInvalidated);
        window->show();
        QVERIFY(QTest::qWaitFor(
                [&] {
                    return !swaps.isEmpty() && output.ready();
                },
                5000));
        const auto firstSwap = swaps.size();
        window->update();
        QVERIFY(QTest::qWaitFor(
                [&] {
                    return swaps.size() > firstSwap;
                },
                5000));
        const bool actualHdr = output.hdrActive();
        const auto presented = [&](qsizetype before) {
            return awaitPresentedFrame(window, output, swaps, before);
        };
        if (actualHdr) {
            const auto resources = invalidations.size();
            auto changed = system;
            changed.whiteScale *= 1.05f;
            monitor->publish(changed);
            QCOMPARE(output.whiteScale(), changed.whiteScale);
            QCOMPARE(invalidations.size(), resources);
            auto before = swaps.size();
            monitor->publish({ });
            QVERIFY(QTest::qWaitFor(
                    [&] {
                        return output.ready() && !output.hdrActive();
                    },
                    5000));
            QVERIFY(presented(before));
            before = swaps.size();
            monitor->publish(system);
            QVERIFY(QTest::qWaitFor(
                    [&] {
                        return output.ready() && output.hdrActive();
                    },
                    5000));
            QVERIFY(presented(before));
            QVERIFY(invalidations.size() > resources);
        } else {
            // A real SDR output must reject the synthetic request and restore
            // an SDR surface without retrying once per poll.
            const auto before = swaps.size();
            monitor->publish({ true, true, 203, 2, true });
            QVERIFY(QTest::qWaitFor(
                    [&] {
                        return output.ready();
                    },
                    5000));
            QVERIFY(presented(before));
            QVERIFY(!output.hdrActive());
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n'_L1)));
        const auto policy = verification::windowPolicy(*window);
        QVERIFY(policy[u"passed"_s].toBool());
        const auto reportPath = QString::fromUtf8(DISPLAY_OUTPUT_REPORT);
        QVERIFY(QDir().mkpath(QFileInfo(reportPath).absolutePath()));
        QVERIFY(yalahoo::writeJsonFile(
                reportPath,
                QJsonDocument(QJsonObject{ { u"systemHdr"_s, system.enabled },
                                           { u"actualHdr"_s, actualHdr },
                                           { u"whiteScale"_s, system.whiteScale },
                                           { u"headroom"_s, system.headroom },
                                           { u"graphicsApi"_s, int(QQuickWindow::graphicsApi()) },
                                           { u"invalidations"_s, invalidations.size() },
                                           { u"windowPolicy"_s, policy } })));
        window->hide();
        window->releaseResources();
    }
};
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (!probeOptionalGraphicsBackend())
        return 22;
    DisplayOutputTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DisplayOutputTest.moc"
