#include "CaptureMailbox.h"
#include "OpeningLayoutCases.h"
#include "RequiredOutput.h"
#include "TransferChecks.h"
#include <QTemporaryDir>
#include <QDir>
#include "FrameUniforms.h"
#include "RenderTarget.h"
#include <rhi/qrhi_platform.h>
#include <cmath>
#include "ReviewSupport.h"
#include "VerificationWindow.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QQuickItem>
#include <QVector2D>
#include <QtTest>

using namespace Qt::StringLiterals;
using namespace yalahoo;
using namespace yalahoo::review;

class RenderSupportTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RenderSupportTest)
public:
    RenderSupportTest() = default;

private Q_SLOTS:
    void openingMatrixUsesActualNativeDpr_data()
    {
        QTest::addColumn<double>("dpr");
        QTest::newRow("100-percent") << 1.;
        QTest::newRow("125-percent") << 1.25;
        QTest::newRow("150-percent") << 1.5;
        QTest::newRow("175-percent") << 1.75;
        QTest::newRow("200-percent") << 2.;
    }
    void openingMatrixUsesActualNativeDpr()
    {
        QFETCH(double, dpr);
        for (bool representative : { false, true }) {
            const auto native = openingLayoutCases(false, dpr, representative);
            QCOMPARE(native.size(), representative ? 11 : 18);
            int desktopDay = 0, desktopNight = 0;
            for (const auto &entry : native) {
                QCOMPARE(entry.dpr, dpr);
                if (entry.size == QSize(1280, 720))
                    entry.night ? ++desktopNight : ++desktopDay;
            }
            QCOMPARE(desktopDay, 1);
            QCOMPARE(desktopNight, 1);
            const auto controlled = openingLayoutCases(true, dpr, representative);
            QCOMPARE(controlled.size(), representative ? 19 : 54);
            for (const auto &entry : controlled)
                QVERIFY(entry.dpr == 1 || entry.dpr == 1.5 || entry.dpr == 2);
        }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, openingLayoutCases(false, 0, false));
    }
    void requiredImageOutputRejectsMissingAndUnwritableImages()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto output = directory.filePath(u"shape.png"_s);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, saveRequiredImage({ }, output));
        QImage image(8, 8, QImage::Format_RGBA8888);
        image.fill(Qt::red);
        QVERIFY(QDir().mkdir(output));
        try {
            saveRequiredImage(image, output);
            QFAIL("A directory occupying the output path must fail");
        } catch (const std::runtime_error &error) {
            QVERIFY(QString::fromUtf8(error.what()).contains(output));
        }
        QVERIFY(QDir().rmdir(output));
        saveRequiredImage(image, output);
        const QImage stored(output);
        QCOMPARE(stored.size(), image.size());
        QCOMPARE(stored.pixelColor(0, 0), QColor(Qt::red));
    }
    void nativeTransferProofFollowsActualBackend()
    {
        QJsonObject metadata{ { u"atlasUploadBytes"_s, 16777216 },
                              { u"nativeAtlasTransfers"_s,
                                QJsonObject{ { u"available"_s, false } } } };
        QVERIFY_THROWS_EXCEPTION(
                std::runtime_error,
                checkAtlasTransfer(QSGRendererInterface::Direct3D11, metadata, 16777216));
        for (const auto backend : { QSGRendererInterface::Direct3D12, QSGRendererInterface::OpenGL,
                                    QSGRendererInterface::Vulkan, QSGRendererInterface::Metal }) {
            checkAtlasTransfer(backend, metadata, 16777216);
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, checkAtlasTransfer(backend, metadata, 1));
        }
        metadata[u"nativeAtlasTransfers"_s] =
                QJsonObject{ { u"available"_s, true }, { u"destinationBytes"_s, 16777216 } };
        metadata[u"atlasUploadBytes"_s] = 1;
        checkAtlasTransfer(QSGRendererInterface::Direct3D11, metadata, 16777216);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 checkAtlasTransfer(QSGRendererInterface::Direct3D11, metadata, 1));
    }
    void outputTargetUsesOnlyBorrowedColorAndTracksNativeRebuilds()
    {
        QRhiNullInitParams parameters;
        const std::unique_ptr<QRhi> api(QRhi::create(QRhi::Null, &parameters));
        QVERIFY(api);
        const std::unique_ptr<QRhiTexture> color(
                api->newTexture(QRhiTexture::RGBA8, { 16, 12 }, 1, QRhiTexture::RenderTarget));
        QVERIFY(color->create());
        rhi::OutputTarget output;
        output.create(api.get(), color.get(), nullptr, nullptr);
        auto *pass = output.pass.get();
        QCOMPARE(output.rt->pixelSize(), QSize(16, 12));
        QVERIFY(!output.rt->description().depthStencilBuffer());
        QVERIFY(!output.rt->description().depthTexture());
        QCOMPARE(output.rt->description().colorAttachmentAt(0)->texture(), color.get());
        color->setPixelSize({ 23, 17 });
        QVERIFY(color->create());
        output.create(api.get(), color.get(), nullptr, nullptr);
        QCOMPARE(output.rt->pixelSize(), QSize(23, 17));
        QCOMPARE(output.pass.get(), pass);
        color->setFormat(QRhiTexture::RGBA16F);
        QVERIFY(color->create());
        output.create(api.get(), color.get(), nullptr, nullptr);
        QCOMPARE(output.rt->description().colorAttachmentAt(0)->texture()->format(),
                 QRhiTexture::RGBA16F);
        const std::unique_ptr<QRhiRenderBuffer> multisample(api->newRenderBuffer(
                QRhiRenderBuffer::Color, { 23, 17 }, 4, { }, QRhiTexture::RGBA16F));
        QVERIFY(multisample->create());
        output.create(api.get(), nullptr, multisample.get(), color.get());
        QCOMPARE(output.rt->description().colorAttachmentAt(0)->renderBuffer(), multisample.get());
        QCOMPARE(output.rt->description().colorAttachmentAt(0)->resolveTexture(), color.get());
        QVERIFY(!output.rt->description().depthStencilBuffer());
        output.reset();
        QVERIFY(!output.rt);
        QVERIFY(!output.pass);
    }

    void incompatibleSharedTargetIsRejected()
    {
        QRhiNullInitParams parameters;
        const std::unique_ptr<QRhi> api(QRhi::create(QRhi::Null, &parameters));
        QVERIFY(api);
        rhi::Target prepass, composite;
        prepass.create(api.get(), { 16, 16 }, true, 1, false, QRhiTexture::RGBA8);
        QVERIFY(!prepass.color);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 composite.create(api.get(), { 16, 16 }, true, 1, false,
                                                  QRhiTexture::RGBA8, &prepass));
    }

    void frameUniformsKeepDisabledRibbonFiniteAndInvalidateLightCache()
    {
        rhi::FrameUniforms uniforms;
        rhi::Input input;
        const QVector<QMatrix4x4> entities{ QMatrix4x4() };
        const QVector<bool> visible{ true };
        uniforms.prepare(input, { }, false, true, { 320, 240 }, 2048, entities, visible, false);
        auto upload = uniforms.uniform(input, { }, rhi::PaperKind, 0, { });
        for (float value : upload.originLight0)
            QVERIFY(std::isfinite(value));
        QCOMPARE(upload.originLight0[0], 0.f);
        input.rowSettings = { 1, .8f, 1, 0 };
        input.printAmbient = .2f;
        input.rowDetail = .25f;
        uniforms.prepare(input, { }, false, true, { 320, 240 }, 2048, entities, visible, false);
        upload = uniforms.uniform(input, { }, rhi::PaperKind, 0, { });
        QCOMPARE(upload.originLight0[0], 1.f);
        QVERIFY(std::abs(upload.originLight0[1] - .34375f) < 1e-6f);
        QCOMPARE(upload.originLight0[2], 1.f);
        QCOMPARE(upload.originLight0[3], 1.f);
        input.rowDetail = 1;
        uniforms.prepare(input, { }, false, true, { 320, 240 }, 2048, entities, visible, false);
        upload = uniforms.uniform(input, { }, rhi::PaperKind, 0, { });
        QCOMPARE(upload.originLight0[1], 1.f);
    }

    void dispatchUsesStageIdentityAndPreservesUnknownState()
    {
        struct Context final
        {
            int calls = 0;
        } context;
        static constexpr StageRoute<Context> routes[] = { { 17,
                                                            [](Context &c) {
                                                                c.calls += 1;
                                                            } },
                                                          { 42, [](Context &c) {
                                                               c.calls += 10;
                                                           } } };
        QVERIFY(dispatchStage(routes, 42, context));
        QCOMPARE(context.calls, 10);
        QVERIFY(!dispatchStage(routes, -1, context));
        QCOMPARE(context.calls, 10);
        QVERIFY(dispatchStage(routes, 17, context));
        QCOMPARE(context.calls, 11);
    }
    void captureRejectsStaleAndUnwatchedCompletion()
    {
        QObject receiver;
        QEventLoop loop;
        auto mailbox = std::make_shared<CaptureMailbox>();
        mailbox->loop = &loop;
        mailbox->request = 2;
        rhi::Capture capture;
        capture.request = 1;
        queueCapture(mailbox, &receiver, capture);
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
        capture.request = 2;
        queueCapture(mailbox, &receiver, capture);
        // A new request supersedes even a result already queued to the GUI.
        mailbox->request = 3;
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
        capture.request = 3;
        queueCapture(mailbox, &receiver, capture);
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(mailbox->capture);
        QCOMPARE(mailbox->capture->request, quint64(3));
        mailbox->capture.reset();
        mailbox->loop = nullptr;
        queueCapture(mailbox, &receiver, capture);
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
    }
    void captureDoesNotOutliveReceiver()
    {
        auto mailbox = std::make_shared<CaptureMailbox>();
        QEventLoop loop;
        mailbox->loop = &loop;
        QPointer<QObject> guard;
        {
            QObject receiver;
            guard = &receiver;
            queueCapture(mailbox, guard, { });
        }
        QVERIFY(guard.isNull());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
        queueCapture(mailbox, guard, { });
        QVERIFY(!mailbox->capture);
    }
    void lightingSnapshotFollowsThemeWithoutChangingSubmission()
    {
        SelectionLighting lighting;
        lighting.setReducedMotion(true);
        rhi::Input input;
        input.request = 17;
        input.setLighting(lighting);
        QCOMPARE(input.warmth, 1.f);
        QCOMPARE(input.lightIntensities, QVector3D(.45f, .32f, .045f));
        QCOMPARE(input.exposure, 1.05f);
        QCOMPARE(input.scatterStrength, 1.f);
        lighting.setNight(true);
        input.setLighting(lighting);
        QCOMPARE(input.warmth, 0.f);
        QCOMPARE(input.environmentIntensity, .16f);
        QCOMPARE(input.lightIntensities, QVector3D(.30f, .21f, .045f));
        QCOMPARE(input.exposure, 1.08f);
        QCOMPARE(input.printAmbient, .12f);
        QCOMPARE(input.scatterStrength, .72f);
        QCOMPARE(input.spotIntensity, 130.f * 64);
        QCOMPARE(input.request, quint64(17));
    }
    void backdropGeometry_data()
    {
        QTest::addColumn<QString>("componentName");
        QTest::addColumn<double>("radius");
        QTest::addColumn<QSize>("target");
        QTest::newRow("opening") << u"OpeningBackdrop"_s << 30.0 << QSize(260, 100);
        QTest::newRow("overview") << u"OverviewBackdrop"_s << 36.0 << QSize(268, 129);
        QTest::newRow("detail") << u"DetailTabsBackdrop"_s << 54.0 << QSize(308, 148);
    }
    void backdropGeometry()
    {
        QFETCH(QString, componentName);
        QFETCH(double, radius);
        QFETCH(QSize, target);
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(QString::fromUtf8(FIXTURE_ROOT)
                                                    + u"/qml/Yalahoo/Music/components/"_s
                                                    + componentName + u".qml"_s));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickItem source;
        QVariantMap properties{ { u"width"_s, 200.0 },
                                { u"height"_s, 40.0 },
                                { u"sceneSource"_s, QVariant::fromValue(&source) },
                                { u"sceneX"_s, 100.0 },
                                { u"sceneY"_s, 80.0 } };
        if (componentName == "OpeningBackdrop"_L1) {
            properties.insert(u"viewport"_s, QSize(1280, 720));
            properties.insert(u"pageColor"_s, QColor(Qt::white));
            properties.insert(u"surfaceColor"_s, QColor(Qt::gray));
            properties.insert(u"paperAlpha"_s, .5);
        } else if (componentName == "OverviewBackdrop"_L1)
            properties.insert(u"labelWidth"_s, 120.0);
        else {
            properties.insert(u"contentSource"_s, QVariant::fromValue(&source));
            properties.insert(u"contentY"_s, 20.0);
        }
        std::unique_ptr<QObject> root(component.createWithInitialProperties(properties));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *crop = root->property("sceneTexture").value<QObject *>();
        auto *intermediate = root->property("horizontalTexture").value<QObject *>();
        QVERIFY(crop);
        QVERIFY(intermediate);
        QCOMPARE(crop->property("sourceRect").toRectF(),
                 QRectF(100 - radius, 80 - radius, target.width(), target.height()));
        QCOMPARE(crop->property("textureSize").toSize(), target);
        QCOMPARE(intermediate->property("textureSize").toSize(), target);
        QCOMPARE(intermediate->property("sourceItem").value<QObject *>(),
                 root->property("horizontalPass").value<QObject *>());
        auto *horizontal = root->property("horizontalPass").value<QQuickItem *>();
        QVERIFY(horizontal);
        QCOMPARE(horizontal->size(), QSizeF(target));
        QCOMPARE(horizontal->property("sampleStep").value<QVector2D>(),
                 QVector2D(1.0f / target.width(), 0));
        QObject *overlay = nullptr;
        if (componentName == "DetailTabsBackdrop"_L1) {
            overlay = horizontal->property("overlay").value<QObject *>();
            QVERIFY(overlay);
            QCOMPARE(overlay->property("textureSize").toSize(), target);
        }
        QVERIFY(root->setProperty("sceneX", 110.0));
        QCOMPARE(crop->property("sourceRect").toRectF().x(), 110 - radius);
        if (componentName == "OverviewBackdrop"_L1) {
            root->setProperty("labelWidth", 130.0);
            QCOMPARE(crop->property("textureSize").toSize(), QSize(278, 129));
        } else {
            root->setProperty("width", 210.0);
            QCOMPARE(crop->property("textureSize").toSize(),
                     QSize(target.width() + 10, target.height()));
            if (overlay)
                QCOMPARE(overlay->property("textureSize").toSize(),
                         QSize(target.width() + 10, target.height()));
        }
        QVERIFY(warnings.isEmpty());
    }
};
int main(int argc, char **argv)
{
    verification::prepareApplication();
    QGuiApplication app(argc, argv);
    RenderSupportTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "RenderSupportTest.moc"
