#include "OpeningAudit.h"
#include "LibraryAudit.h"
#include "OffscreenSurface.h"
#include "AssetCatalog.h"
#include "AppIdentity.h"
#include "CanvasProbe.h"
#include "RulerAudit.h"
#include "TitleAudit.h"
#include "PresentationAudit.h"
#include "OverviewAudit.h"
#include "GraphicsBackend.h"
#include "LayoutMetrics.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QScreen>
#include <QFontMetricsF>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <atomic>
#include <QMutex>
#include <QMutexLocker>
#include <rhi/qrhi.h>

namespace {
QJsonArray diagnostics;
QMutex diagnosticsMutex;
void message(QtMsgType type, const QMessageLogContext &, const QString &text)
{
    const QMutexLocker lock(&diagnosticsMutex);
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
        diagnostics.append(text);
    std::fprintf(stderr, "%s\n", text.toUtf8().constData());
}
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems()) {
        if (auto *found = findItem(child, name))
            return found;
    }
    return nullptr;
}
QJsonObject bounds(QQuickItem *root, const QString &name)
{
    auto *item = root->findChild<QQuickItem *>(name);
    if (!item)
        return { };
    const auto point = item->mapToScene({ 0, 0 });
    const auto font = item->property("font").value<QFont>();
    const QFontMetricsF fontMetrics(font);
    return { { "x", point.x() },
             { "y", point.y() },
             { "width", item->width() },
             { "height", item->height() },
             { "visible", item->isVisible() },
             { "baseline", item->baselineOffset() },
             { "ascent",
               item->property("cssAscent").isValid() ? item->property("cssAscent").toDouble()
                                                     : fontMetrics.ascent() },
             { "descent",
               item->property("cssDescent").isValid() ? item->property("cssDescent").toDouble()
                                                      : fontMetrics.descent() },
             { "implicitHeight", item->implicitHeight() } };
}
}
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    if (!yalahoo::selectGraphicsBackend())
        return 2;
    yalahoo::initializeIdentity();
    QGuiApplication app(argc, argv);
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    if (argc == 2 && QString::fromLocal8Bit(argv[1]) == "--native-dpr") {
        std::printf("%.9f\n", app.primaryScreen()->devicePixelRatio());
        return 0;
    }
    if (argc < 2 || !yalahoo::initializeAssetFonts())
        return 2;
    qputenv("QCPAINTER_DEBUG_COLLECT", "0");
    qInstallMessageHandler(message);
    if (app.arguments().contains("--playback"))
        return argc == 3 ? reviewLibrary(app, QString::fromLocal8Bit(argv[2]), true) : 2;
    if (app.arguments().contains("--library"))
        return argc == 3 ? reviewLibrary(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains("--opening"))
        return argc == 3 ? reviewOpening(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains("--overview"))
        return argc == 3 ? reviewOverviewVisual(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains("--presentation"))
        return argc == 3 ? reviewPresentation(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains("--ruler"))
        return argc == 3 ? reviewRuler(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains("--titles")) {
        const auto arguments = app.arguments();
        if ((arguments.size() != 3 && arguments.size() != 4) || arguments[1] != "--titles")
            return 2;
        return reviewTitle(app, arguments[2], arguments.size() == 4 ? arguments[3] : QString());
    }
    const QString output = QString::fromLocal8Bit(argv[1]);
    QDir().mkpath(output);
    yalahoo::LayoutMetrics metrics;
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/UiReview.qml"));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "metrics", QVariant::fromValue(&metrics) } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    std::atomic<qint64> syncStart{ 0 }, renderStart{ 0 }, syncNs{ 0 }, renderNs{ 0 };
    QElapsedTimer elapsed;
    elapsed.start();
    std::atomic<bool> captureRequested{ false }, captureInFlight{ false };
    std::atomic<int> requestedCaptureRevision{ 0 }, completedCaptureRevision{ 0 };
    std::atomic<int> drainFramesRemaining{ 0 };
    QRhiReadbackResult readback;
    QMutex captureMutex;
    QImage capturedFrame;
    QQuickRenderControl renderControl;
    QQuickWindow window(OffscreenSurface::Enabled ? &renderControl : nullptr);
    OffscreenSurface surface(window, renderControl);
    root->setProperty("offscreenRendering", OffscreenSurface::Enabled);
    yalahoo::verification::prepareWindow(window, true);
    window.resize(1280, 720);
    root->setParentItem(window.contentItem());
    root->setSize(window.size());
    QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 }, { 995, 858 },   { 960, 900 },  { 930, 900 },
                        { 390, 844 },   { 320, 720 },  { 2560, 1080 }, { 3440, 1440 } };
    QJsonArray captures, probes, lifecycle;
    if (app.arguments().contains("--representative"))
        sizes = { { 1280, 720 }, { 930, 900 }, { 390, 844 } };
    quint64 swapped = 0;
    QObject::connect(
            &window, &QQuickWindow::beforeSynchronizing, &window,
            [&] {
                syncStart = elapsed.nsecsElapsed();
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::afterSynchronizing, &window,
            [&] {
                syncNs += elapsed.nsecsElapsed() - syncStart;
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::beforeRendering, &window,
            [&] {
                renderStart = elapsed.nsecsElapsed();
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [&] {
                renderNs += elapsed.nsecsElapsed() - renderStart;
            },
            Qt::DirectConnection);
    surface.connectFrames(&app, [&] {
        ++swapped;
        if (captureInFlight && drainFramesRemaining > 0) {
            --drainFramesRemaining;
            root->setProperty("captureRevision", root->property("captureRevision").toInt() + 1);
            surface.requestFrame();
        }
    });
    QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [&] {
                if (!captureRequested.load() || captureInFlight.load())
                    return;
                auto *rhi = surface.rhi();
                auto *commandBuffer = surface.commandBuffer();
                if (!rhi || !commandBuffer)
                    return;
                captureRequested = false;
                captureInFlight = true;
                drainFramesRemaining = rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames) + 1;
                const int captureRevision = requestedCaptureRevision;
                const bool flip = rhi->isYUpInFramebuffer();
                readback.completed = [&, flip, captureRevision] {
                    const auto format = readback.format == QRhiTexture::BGRA8
                            ? QImage::Format_ARGB32
                            : QImage::Format_RGBA8888;
                    QImage frame(reinterpret_cast<const uchar *>(readback.data.constData()),
                                 readback.pixelSize.width(), readback.pixelSize.height(), format);
                    const QMutexLocker lock(&captureMutex);
                    capturedFrame = flip ? frame.flipped(Qt::Vertical) : frame.copy();
                    completedCaptureRevision = captureRevision;
                    captureInFlight = false;
                };
                auto *batch = rhi->nextResourceUpdateBatch();
                batch->readBackTexture(surface.readbackDescription(), &readback);
                commandBuffer->resourceUpdate(batch);
            },
            Qt::DirectConnection);
    bool passed = true;
    int step = 0;
    quint64 stationaryPaints = 0;
    qint64 modeSyncStart = 0, modeRenderStart = 0, guiStateNs = 0;
    std::function<void()> next;
    auto wait = [&](int ms) {
        QTimer::singleShot(ms, &app, [&] {
            next();
        });
    };
    next = [&] {
        if (step < sizes.size() * 2) {
            const int index = step++;
            const auto size = sizes[index / 2];
            metrics.setViewport(size.width(), size.height());
            window.resize(size);
            root->setSize(size);
            root->setProperty("night", index % 2 == 1);
            QTimer::singleShot(150, &app, [&, index, size] {
                const auto image = surface.grab();
                const QString file = QString("browse-%1.png").arg(index, 2, 10, QChar('0'));
                const auto info = bounds(root, "browseInfo");
                const auto play = bounds(root, "playButton");
                const auto stop = bounds(root, "stopButton");
                const auto header = bounds(root, "archiveHeader");
                const bool valid = !image.isNull() && image.save(output + '/' + file)
                        && std::abs(info["x"].toDouble() - metrics.informationX()) < 0.01
                        && play["x"].toDouble() + play["width"].toDouble() <= size.width()
                        && stop["x"].toDouble() + stop["width"].toDouble() <= size.width()
                        && yalahoo::verification::windowPolicySatisfied(window);
                passed &= valid;
                captures.append(QJsonObject{ { "width", size.width() },
                                             { "height", size.height() },
                                             { "night", index % 2 == 1 },
                                             { "dpr", window.devicePixelRatio() },
                                             { "file", file },
                                             { "passed", valid },
                                             { "header", header },
                                             { "browse", info },
                                             { "footer", bounds(root, "archiveFooter") },
                                             { "play", play },
                                             { "stop", stop },
                                             { "title", bounds(root, "albumTitle") },
                                             { "artist", bounds(root, "albumArtist") },
                                             { "meta", bounds(root, "albumMeta") },
                                             { "open", bounds(root, "openAlbumButton") },
                                             { "openCaption", bounds(root, "openAlbumCaption") },
                                             { "settingsSvg", bounds(root, "settingsIcon") },
                                             { "brand", bounds(root, "brandContainer") },
                                             { "number", bounds(root, "staticOrdinal") },
                                             { "total", bounds(root, "staticTotal") } });
                root->setProperty("playingTitle",
                                  QStringLiteral("播放中的长歌曲标题保持按钮位置").repeated(5));
                QTimer::singleShot(150, &app, [&, play, stop, size] {
                    const auto afterPlay = bounds(root, "playButton");
                    const auto afterStop = bounds(root, "stopButton");
                    const bool fixed = play == afterPlay && stop == afterStop;
                    auto *transport = root->findChild<QObject *>("minimalTransport");
                    const double cap = size.width() / double(size.height()) < 1.05
                            ? std::clamp(size.width() - 320, 0, 280)
                            : size.width() <= 700  ? std::clamp(size.width() - 340, 0, 100)
                            : size.width() <= 1100 ? std::clamp(size.width() - 610, 0, 180)
                                                   : std::clamp(size.width() - 760, 0, 240);
                    const bool sourceCap =
                            transport && transport->property("songCap").toDouble() == cap;
                    passed &= fixed && sourceCap;
                    lifecycle.append(QJsonObject{ { "operation", "playing-title-fixed-buttons" },
                                                  { "width", size.width() },
                                                  { "height", size.height() },
                                                  { "passed", fixed && sourceCap } });
                    root->setProperty("playingTitle", "");
                    wait(0);
                });
            });
            return;
        }
        const int probeStart = sizes.size() * 2;
        if (step < probeStart + 6) {
            const int index = step++ - probeStart;
            ++requestedCaptureRevision;
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            QElapsedTimer guiTimer;
            guiTimer.start();
            root->setProperty("probes", true);
            root->setProperty("probeMode", index % 3);
            root->setProperty("night", index / 3 == 1);
            metrics.setViewport(360, 160);
            window.resize(362, 162);
            root->setSize({ 362, 162 });
            guiStateNs = guiTimer.nsecsElapsed();
            // Canvas2D's available/onPaint work is queued after item creation.
            // Read the settled submitted texture, rather than the creation frame.
            QTimer::singleShot(300, &app, [&] {
                captureRequested = true;
                root->setProperty("captureRevision", root->property("captureRevision").toInt() + 1);
                surface.requestFrame();
            });
            QTimer::singleShot(1200, &app, [&, index] {
                QImage image;
                {
                    const QMutexLocker lock(&captureMutex);
                    image = capturedFrame.copy(QRect(0, 0, qRound(360 * window.devicePixelRatio()),
                                                     qRound(160 * window.devicePixelRatio())));
                }
                const QString file = QString("probe-%1.png").arg(index);
                passed &= !image.isNull() && image.save(output + '/' + file);
                const bool matchingRevision = completedCaptureRevision == requestedCaptureRevision;
                passed &= matchingRevision;
                if (image.isNull() || !matchingRevision) {
                    qWarning() << "Canvas readback failed" << index << window.isExposed()
                               << completedCaptureRevision << requestedCaptureRevision;
                    app.exit(6);
                    return;
                }
                QJsonObject record{ { "mode", index % 3 },
                                    { "night", index / 3 == 1 },
                                    { "dpr", window.devicePixelRatio() },
                                    { "file", file } };
                record.insert("captureRevisionMatched", matchingRevision);
                int textPixels = 0;
                const auto dpr = window.devicePixelRatio();
                const auto background = image.pixelColor(image.width() - 1, image.height() - 1);
                for (int y = int(96 * dpr); y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        const auto color = image.pixelColor(x, y);
                        if (std::abs(color.red() - background.red())
                                    + std::abs(color.green() - background.green())
                                    + std::abs(color.blue() - background.blue())
                            > 90)
                            ++textPixels;
                    }
                }
                record.insert("visibleTextPixels", textPixels);
                passed &= textPixels > 100;
                record.insert("guiStateSetNs", guiStateNs);
                record.insert("quickSyncCpuNs", qint64(syncNs - modeSyncStart));
                record.insert("quickRenderCpuNs", qint64(renderNs - modeRenderStart));
                record.insert("gpuNanoseconds", QJsonValue::Null);
                const QString painterName = index % 3 == 0 ? "canvasCpp" : "canvasJs";
                if (auto *item = root->findChild<QObject *>(painterName))
                    record.insert("moduleDebug",
                                  QJsonObject::fromVariantMap(item->property("debug").toMap()));
                record.insert("attachmentBytesUpperBound",
                              qint64(360 * 160 * window.devicePixelRatio()
                                     * window.devicePixelRatio() * 12));
                if (auto *js = root->findChild<QObject *>("canvasJs")) {
                    record.insert("scriptMilliseconds",
                                  js->property("scriptMilliseconds").toDouble());
                    record.insert("paintCount", js->property("paintCount").toInt());
                }
                record.insert("painterPaints", qint64(canvasProbeStats().paints.load()));
                probes.append(record);
                wait(0);
            });
            return;
        }
        const int lifecycleStep = step++ - probeStart - 6;
        const int phase = lifecycleStep % 26;
        const int lifecycleMode = lifecycleStep / 26;
        if (phase == 0) {
            root->findChild<QQuickItem *>("probeLoader")->setSize({ 360, 160 });
            root->setProperty("probes", true);
            root->setProperty("probeMode", lifecycleMode);
            wait(200);
            return;
        }
        if (phase == 1) {
            stationaryPaints = lifecycleMode == 0
                    ? canvasProbeStats().paints.load()
                    : root->findChild<QObject *>("canvasJs")->property("paintCount").toULongLong();
            wait(400);
            return;
        }
        if (phase == 2) {
            const bool stable = lifecycleMode == 0
                    ? canvasProbeStats().paints == stationaryPaints
                    : root->findChild<QObject *>("canvasJs")->property("paintCount").toULongLong()
                            == stationaryPaints;
            passed &= stable;
            lifecycle.append(QJsonObject{ { "operation", "stationary" }, { "passed", stable } });
            surface.hide();
            stationaryPaints = lifecycleMode == 0
                    ? canvasProbeStats().paints.load()
                    : root->findChild<QObject *>("canvasJs")->property("paintCount").toULongLong();
            wait(250);
            return;
        }
        if (phase == 3) {
            const bool stable = lifecycleMode == 0
                    ? canvasProbeStats().paints == stationaryPaints
                    : root->findChild<QObject *>("canvasJs")->property("paintCount").toULongLong()
                            == stationaryPaints;
            passed &= stable;
            lifecycle.append(QJsonObject{ { "operation", "hidden" }, { "passed", stable } });
            window.releaseResources();
            surface.show();
            window.resize(420, 200);
            root->findChild<QQuickItem *>("probeLoader")->setSize({ 420, 200 });
            root->setSize({ 420, 200 });
            ++requestedCaptureRevision;
            QTimer::singleShot(150, &app, [&] {
                captureRequested = true;
                root->setProperty("captureRevision", root->property("captureRevision").toInt() + 1);
                surface.requestFrame();
            });
            wait(700);
            return;
        }
        if (phase >= 4 && phase < 24) {
            if (phase == 4) {
                QImage restored;
                {
                    const QMutexLocker lock(&captureMutex);
                    restored = capturedFrame;
                }
                const auto expectedSize = QSize(qRound(420 * window.devicePixelRatio()),
                                                qRound(200 * window.devicePixelRatio()));
                const QString file = QString("restored-%1.png").arg(lifecycleMode);
                const bool saved = restored.save(output + '/' + file);
                const bool valid = completedCaptureRevision == requestedCaptureRevision
                        && restored.size() == expectedSize && saved
                        && yalahoo::verification::windowPolicySatisfied(window);
                passed &= valid;
                lifecycle.append(QJsonObject{ { "operation", "restore-resize-content" },
                                              { "mode", lifecycleMode },
                                              { "passed", valid },
                                              { "file", file } });
            }
            root->setProperty("probes", phase % 2 != 0);
            wait(80);
            return;
        }
        if (phase == 24) {
            root->setProperty("probes", false);
            if (lifecycleMode == 1) {
                metrics.setViewport(320, 480);
                window.resize(320, 480);
                root->setSize({ 320, 480 });
                root->setProperty(
                        "titleText",
                        QStringLiteral("长标题中文与组合字 é、下伸 gyp 🎵 ").repeated(20));
                root->setProperty("playingTitle",
                                  QStringLiteral("当前播放歌曲保持固定按钮位置").repeated(5));
            }
            wait(200);
            return;
        }
        const bool released =
                canvasProbeStats().renderers == 0 && !root->findChild<QObject *>("canvasJs");
        if (lifecycleMode == 0) {
            passed &= released;
            lifecycle.append(QJsonObject{ { "operation", "cpp-ten-rebuilds-release" },
                                          { "passed", released } });
            wait(0);
            return;
        }
        auto *shortInfo = root->findChild<QQuickItem *>("browseInfo");
        const bool scrollable =
                shortInfo && shortInfo->property("contentHeight").toDouble() > shortInfo->height();
        passed &= scrollable;
        lifecycle.append(
                QJsonObject{ { "operation", "short-portrait-scroll" }, { "passed", scrollable } });
        for (const auto &[name, action] :
             { std::pair{ "playButton", "play-pause" }, std::pair{ "stopButton", "stop" },
               std::pair{ "openAlbumButton", "open" } }) {
            auto *button = findItem(root, name);
            const int before = root->property("actionCount").toInt();
            if (button) {
                button->forceActiveFocus(Qt::TabFocusReason);
                QTest::keyClick(&window, Qt::Key_Space);
            }
            const bool routed = button && button->hasActiveFocus()
                    && root->property("actionCount").toInt() == before + 1
                    && root->property("lastAction").toString() == action;
            passed &= routed;
            lifecycle.append(QJsonObject{ { "operation", name }, { "passed", routed } });
        }
        for (const auto &name : { "dayButton", "nightButton" }) {
            auto *button = findItem(root, name);
            if (button) {
                button->forceActiveFocus(Qt::TabFocusReason);
                QTest::keyClick(&window, Qt::Key_Space);
            }
            const bool routed =
                    button && root->property("night").toBool() == (QString(name) == "nightButton");
            passed &= routed;
            lifecycle.append(QJsonObject{ { "operation", name }, { "passed", routed } });
        }
        QJsonArray capturedDiagnostics;
        {
            const QMutexLocker lock(&diagnosticsMutex);
            capturedDiagnostics = diagnostics;
        }
        passed &= released && yalahoo::verification::windowPolicySatisfied(window) && swapped > 0
                && capturedDiagnostics.isEmpty();
        lifecycle.append(
                QJsonObject{ { "operation", "ten-rebuilds-release" },
                             { "passed", released },
                             { "liveRenderers", qint64(canvasProbeStats().renderers.load()) } });
        QJsonObject report{
            { "passed", passed },
            { "qtVersion", qVersion() },
            { "offscreen", OffscreenSurface::Enabled },
            { "offscreenResourcePlan", surface.diagnostics() },
            { "graphicsApi", window.rendererInterface()->graphicsApi() },
            { "windowPolicy", yalahoo::verification::windowPolicy(window) },
            { "captureMethod",
              OffscreenSurface::Enabled
                      ? "QQuickRenderControl Metal texture QRhi readback; native window stays hidden"
                      : "UI grabWindow; Canvas actual submitted swapchain QRhi readback, no forced Canvas redraw" },
            { "browse", captures },
            { "probes", probes },
            { "lifecycle", lifecycle },
            { "diagnostics", capturedDiagnostics },
            { "resourcePlan",
              QJsonObject{ { "probeSize", QJsonArray{ 360, 160 } },
                           { "sampleCount", 1 },
                           { "colorFormat", "RGBA8" },
                           { "mips", false },
                           { "resizeSimultaneousAttachmentBytesUpperBound",
                             qint64((360 * 160 + 420 * 200) * window.devicePixelRatio()
                                    * window.devicePixelRatio() * 12) },
                           { "fontAtlasAndInternalBufferBytes", QJsonValue::Null },
                           { "qtMainOutputAndSwapchainBytes", QJsonValue::Null } } },
            { "metrics",
              QJsonObject{ { "cppPaints", qint64(canvasProbeStats().paints.load()) },
                           { "cppPaintCpuNs", qint64(canvasProbeStats().paintNanoseconds.load()) },
                           { "cppSyncCpuNs", qint64(canvasProbeStats().syncNanoseconds.load()) },
                           { "cppSyncCount", qint64(canvasProbeStats().synchronizations.load()) },
                           { "gpuNanoseconds", QJsonValue::Null },
                           { "imageBytes", qint64(canvasProbeStats().imageBytes.load()) },
                           { "imageCount", qint64(canvasProbeStats().imageCount.load()) } } }
        };
        QSaveFile file(output + "/report.json");
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(5);
            return;
        }
        file.write(QJsonDocument(report).toJson());
        const bool saved = file.commit();
        app.exit(passed && saved ? 0 : 6);
    };
    surface.show();
    QTimer::singleShot(0, &app, [&] {
        next();
    });
    QTimer::singleShot(60000, &app, [&] {
        app.exit(7);
    });
    return app.exec();
}
