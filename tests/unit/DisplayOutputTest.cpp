#include "DisplayState.h"
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
#include "OffscreenRhi.h"
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
    explicit ControlledMonitor(std::unique_ptr<DisplayMonitor> platform = { })
        : m_platform(std::move(platform))
    {
        if (m_platform)
            m_platform->changed = [this] {
                if (changed)
                    changed();
            };
    }
    ~ControlledMonitor() override
    {
        if (m_platform)
            m_platform->changed = { };
    }
    DisplayState state;
    DisplayState query(QWindow &window) override
    {
        if (m_platform)
            state = m_platform->query(window);
        return state;
    }
    bool pending() const override
    {
        return m_platform && m_platform->pending();
    }
    bool freeze(QWindow &window)
    {
        query(window);
        if (pending())
            return false;
        if (m_platform)
            m_platform->changed = { };
        m_platform.reset();
        return true;
    }
    void publish(DisplayState value)
    {
        state = value;
        if (changed)
            changed();
    }

private:
    std::unique_ptr<DisplayMonitor> m_platform;
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
    void asynchronousMonitorDoesNotFreezeProvisionalSdr()
    {
        class DeferredMonitor final : public DisplayMonitor
        {
            Q_DISABLE_COPY_MOVE(DeferredMonitor)
        public:
            DeferredMonitor() = default;
            int step = 0;
            DisplayState query(QWindow &) override
            {
                return step < 2 ? DisplayState() : DisplayState{ true, true, 203, 4, true };
            }
            bool pending() const override
            {
                return step < 2;
            }
            void completeStep()
            {
                ++step;
                if (changed)
                    changed();
            }
        };
        QWindow window;
        auto platform = std::make_unique<DeferredMonitor>();
        auto *deferred = platform.get();
        ControlledMonitor monitor(std::move(platform));
        int notifications = 0;
        monitor.changed = [&] {
            ++notifications;
            monitor.query(window);
        };
        QVERIFY(!monitor.freeze(window));
        deferred->completeStep(); // Registry discovery still needs the output description.
        QVERIFY(!monitor.freeze(window));
        deferred->completeStep();
        QCOMPARE(notifications, 2);
        QVERIFY(monitor.freeze(window));
        QVERIFY(monitor.state.enabled);
        QCOMPARE(monitor.state.whiteScale, 203.f);
        monitor.publish({ });
        QVERIFY(!monitor.query(window).enabled);
        ControlledMonitor sdr;
        QVERIFY(sdr.freeze(window));
        QVERIFY(!sdr.state.enabled);
    }
    void transferShader()
    {
        review::OffscreenRhi context;
        auto *api = context.get();
        const auto selected = QQuickWindow::graphicsApi();
        const auto expected = selected == QSGRendererInterface::OpenGL ? QRhi::OpenGLES2
                : selected == QSGRendererInterface::Metal              ? QRhi::Metal
                : selected == QSGRendererInterface::Direct3D11         ? QRhi::D3D11
                : selected == QSGRendererInterface::Direct3D12         ? QRhi::D3D12
                                                                       : QRhi::Vulkan;
        QCOMPARE(api->backend(), expected);
        qInfo() << "Transfer shader backend:" << api->backendName();
        const QSize size(8, 1);
        const std::array<std::array<float, 3>, 8> samples{ { { 0, 0, 0 },
                                                             { .04045f, .04045f, .04045f },
                                                             { .5f, .5f, .5f },
                                                             { 1, 1, 1 },
                                                             { 2, 2, 2 },
                                                             { 1, 0, 0 },
                                                             { 0, 1, 0 },
                                                             { 0, 0, 1 } } };
        std::array<qfloat16, 32> pixels;
        for (int p = 0; p < size.width(); ++p)
            for (int channel = 0; channel < 4; ++channel)
                pixels[p * 4 + channel] = qfloat16(channel == 3 ? 1 : samples[p][channel]);
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
                { { QRhiShaderStage::Vertex, loadShader(u":/display/DisplayTransfer.vert.qsb"_s) },
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
            commands->setViewport({ 0, 0, float(size.width()), 1 });
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
            QCOMPARE(read.data.size(), 64);
            // Independent D65 Rec.709 -> Rec.2020 reference checks every output channel.
            constexpr double gamut[3][3]{ { .627404, .329283, .043313 },
                                          { .069097, .919540, .011362 },
                                          { .016391, .088013, .895595 } };
            for (int p = 0; p < size.width(); ++p) {
                std::array<double, 3> linear;
                for (int channel = 0; channel < 3; ++channel) {
                    const double encoded = float(pixels[p * 4 + channel]);
                    linear[channel] = encoded <= .04045 ? encoded / 12.92
                                                        : std::pow((encoded + .055) / 1.055, 2.4);
                }
                for (int channel = 0; channel < 3; ++channel) {
                    double expected = linear[channel];
                    if (hdr10)
                        expected = gamut[channel][0] * linear[0] + gamut[channel][1] * linear[1]
                                + gamut[channel][2] * linear[2];
                    expected *= scale;
                    if (hdr10) {
                        const double light =
                                std::pow(std::clamp(expected / 10000, 0.0, 1.0), 2610.0 / 16384);
                        expected = std::pow((3424.0 / 4096 + 2413.0 / 128 * light)
                                                    / (1 + 2392.0 / 128 * light),
                                            2523.0 / 32);
                    }
                    qfloat16 value;
                    std::memcpy(&value, read.data.constData() + p * 8 + channel * 2, 2);
                    QVERIFY2(std::isfinite(float(value))
                                     && std::abs(float(value) - expected) < .015,
                             qPrintable(QString::number(float(value)) + u" expected "_s
                                        + QString::number(expected)));
                }
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
        commands->setViewport({ 0, 0, float(size.width()), 1 });
        commands->setVertexInput(0, 0, nullptr);
        commands->draw(3);
        commands->endPass();
        QRhiReadbackResult sceneRead;
        auto *reads = api->nextResourceUpdateBatch();
        reads->readBackTexture({ output.get() }, &sceneRead);
        commands->resourceUpdate(reads);
        QCOMPARE(api->endOffscreenFrame(), QRhi::FrameOpSuccess);
        QCOMPARE(sceneRead.data.size(), 64);
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
        auto controlled = std::make_unique<ControlledMonitor>(makeDisplayMonitor());
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
        QVERIFY2(QTest::qWaitFor(
                         [&] {
                             return monitor->freeze(*window);
                         },
                         5000),
                 "Native display description is still pending");
        const auto system = monitor->state;
        // Publish the final description, then wait for its corresponding output configuration.
        monitor->publish(system);
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
    // The controlled monitor regression has no native graphics dependency.
    const bool controlledOnly =
            argc == 2 && QByteArray(argv[1]) == "asynchronousMonitorDoesNotFreezeProvisionalSdr"_ba;
    if (!controlledOnly && !selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (!controlledOnly && !probeOptionalGraphicsBackend())
        return 22;
    DisplayOutputTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DisplayOutputTest.moc"
