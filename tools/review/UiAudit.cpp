#include "Localization.h"
#include "ReadbackImage.h"
#include "CoverCache.h"
#include <QScopeGuard>
#include "OpeningAudit.h"
#include "StartupAudit.h"
#include "ExperimentAudit.h"
#include "ControlsAudit.h"
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
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <atomic>
#include <QMutex>
#include <QMutexLocker>
#include <rhi/qrhi.h>

using namespace Qt::StringLiterals;

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
    return { { u"x"_s, point.x() },
             { u"y"_s, point.y() },
             { u"width"_s, item->width() },
             { u"height"_s, item->height() },
             { u"visible"_s, item->isVisible() },
             { u"baseline"_s, item->baselineOffset() },
             { u"ascent"_s,
               item->property("cssAscent").isValid() ? item->property("cssAscent").toDouble()
                                                     : fontMetrics.ascent() },
             { u"descent"_s,
               item->property("cssDescent").isValid() ? item->property("cssDescent").toDouble()
                                                      : fontMetrics.descent() },
             { u"implicitHeight"_s, item->implicitHeight() } };
}
}
int main(int argc, char **argv)
{
    QElapsedTimer bootstrapTimer;
    bootstrapTimer.start();
    yalahoo::verification::prepareApplication();
    if (!yalahoo::selectGraphicsBackend())
        return 2;
    yalahoo::initializeIdentity();
    QGuiApplication app(argc, argv);
    const yalahoo::Localization localization(QLocale(QLocale::Chinese, QLocale::China));
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
    const auto applicationNs = bootstrapTimer.nsecsElapsed();
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    if (argc == 2 && QString::fromLocal8Bit(argv[1]) == "--native-dpr"_L1) {
        std::printf("%.9f\n", app.primaryScreen()->devicePixelRatio());
        return 0;
    }
    if (argc < 2 || !yalahoo::initializeAssetFonts())
        return 2;
    const auto fontsNs = bootstrapTimer.nsecsElapsed();
    qputenv("QCPAINTER_DEBUG_COLLECT", "0"_ba);
    qInstallMessageHandler(message);
    if (app.arguments().contains(u"--controls"_s))
        return argc == 3 ? reviewControls(QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--experiments"_s))
        return argc == 3 ? reviewExperiments(QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--settings"_s))
        return argc == 3 ? reviewLibrary(app, QString::fromLocal8Bit(argv[2]), true, true) : 2;
    if (app.arguments().contains(u"--playback"_s))
        return argc == 3 ? reviewLibrary(app, QString::fromLocal8Bit(argv[2]), true) : 2;
    if (app.arguments().contains(u"--library"_s))
        return argc == 3 ? reviewLibrary(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--gui"_s)) {
        const bool narrow = argc == 4 && QString::fromLocal8Bit(argv[3]) == "narrow"_L1;
        const bool representative =
                argc == 4 && QString::fromLocal8Bit(argv[3]) == "--representative"_L1;
        return argc == 3 || narrow || representative
                ? reviewGui(QString::fromLocal8Bit(argv[2]), narrow, representative)
                : 2;
    }
    if (app.arguments().contains(u"--startup"_s)) {
        bool validSeconds = true;
        const double seconds =
                argc >= 4 ? QString::fromLocal8Bit(argv[3]).toDouble(&validSeconds) : 0;
        const bool observeOnly = argc == 6 && QString::fromLocal8Bit(argv[5]) == "observe"_L1;
        return (argc == 3 || argc == 4 || argc == 5 || observeOnly) && validSeconds
                ? reviewStartup(
                          app, QString::fromLocal8Bit(argv[2]), seconds,
                          { { u"applicationSetupMs"_s, applicationNs / 1e6 },
                            { u"fontAndArgumentSetupMs"_s, (fontsNs - applicationNs) / 1e6 },
                            { u"scope"_s,
                              u"Review main entry through QGuiApplication and embedded font/argument setup; excludes pre-main loader, anonymous fixture generation, model/cover/render preparation and native presentation."_s } },
                          argc >= 5 ? QString::fromLocal8Bit(argv[4]) : u"matrix"_s, observeOnly)
                : 2;
    }
    if (app.arguments().contains(u"--opening"_s)) {
        const bool representative =
                argc == 4 && QString::fromLocal8Bit(argv[3]) == "--representative"_L1;
        return argc == 3 || representative
                ? reviewOpening(app, QString::fromLocal8Bit(argv[2]), representative)
                : 2;
    }
    if (app.arguments().contains(u"--overview"_s))
        return argc == 3 ? reviewOverviewVisual(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--presentation"_s))
        return argc == 3 ? reviewPresentation(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--ruler"_s))
        return argc == 3 ? reviewRuler(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--titles"_s)) {
        const auto arguments = app.arguments();
        if ((arguments.size() != 3 && arguments.size() != 4) || arguments[1] != "--titles"_L1)
            return 2;
        return reviewTitle(app, arguments[2], arguments.size() == 4 ? arguments[3] : QString());
    }
    const QString output = QString::fromLocal8Bit(argv[1]);
    QDir().mkpath(output);
    yalahoo::LayoutMetrics metrics;
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/UiReview.qml"_s));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { u"metrics"_s, QVariant::fromValue(&metrics) } }));
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
    if (app.arguments().contains(u"--representative"_s))
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
                    QImage frame;
                    try {
                        frame = yalahoo::rhi::readbackImage(readback, flip);
                    } catch (const std::runtime_error &error) {
                        qWarning("UI readback: %s", error.what());
                    }
                    const QMutexLocker lock(&captureMutex);
                    capturedFrame = std::move(frame);
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
                const QString file = u"browse-%1.png"_s.arg(index, 2, 10, '0'_L1);
                const auto info = bounds(root, u"browseInfo"_s);
                const auto play = bounds(root, u"playButton"_s);
                const auto stop = bounds(root, u"stopButton"_s);
                const auto header = bounds(root, u"archiveHeader"_s);
                const bool valid = !image.isNull() && image.save(output + '/'_L1 + file)
                        && std::abs(info[u"x"_s].toDouble() - metrics.informationX()) < 0.01
                        && play[u"x"_s].toDouble() + play[u"width"_s].toDouble() <= size.width()
                        && stop[u"x"_s].toDouble() + stop[u"width"_s].toDouble() <= size.width()
                        && yalahoo::verification::windowPolicySatisfied(window);
                passed &= valid;
                captures.append(
                        QJsonObject{ { u"width"_s, size.width() },
                                     { u"height"_s, size.height() },
                                     { u"night"_s, index % 2 == 1 },
                                     { u"dpr"_s, window.devicePixelRatio() },
                                     { u"file"_s, file },
                                     { u"passed"_s, valid },
                                     { u"header"_s, header },
                                     { u"browse"_s, info },
                                     { u"footer"_s, bounds(root, u"archiveFooter"_s) },
                                     { u"play"_s, play },
                                     { u"stop"_s, stop },
                                     { u"title"_s, bounds(root, u"albumTitle"_s) },
                                     { u"artist"_s, bounds(root, u"albumArtist"_s) },
                                     { u"meta"_s, bounds(root, u"albumMeta"_s) },
                                     { u"open"_s, bounds(root, u"openAlbumButton"_s) },
                                     { u"openCaption"_s, bounds(root, u"openAlbumCaption"_s) },
                                     { u"settingsSvg"_s, bounds(root, u"settingsIcon"_s) },
                                     { u"brand"_s, bounds(root, u"brandContainer"_s) },
                                     { u"number"_s, bounds(root, u"staticOrdinal"_s) },
                                     { u"total"_s, bounds(root, u"staticTotal"_s) } });
                root->setProperty(
                        "playingTitle",
                        u"\u64ad\u653e\u4e2d\u7684\u957f\u6b4c\u66f2\u6807\u9898\u4fdd\u6301\u6309\u94ae\u4f4d\u7f6e"_s
                                .repeated(5));
                QTimer::singleShot(150, &app, [&, play, stop, size] {
                    const auto afterPlay = bounds(root, u"playButton"_s);
                    const auto afterStop = bounds(root, u"stopButton"_s);
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
                    lifecycle.append(
                            QJsonObject{ { u"operation"_s, u"playing-title-fixed-buttons"_s },
                                         { u"width"_s, size.width() },
                                         { u"height"_s, size.height() },
                                         { u"passed"_s, fixed && sourceCap } });
                    root->setProperty("playingTitle", u""_s);
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
                const QString file = u"probe-%1.png"_s.arg(index);
                passed &= !image.isNull() && image.save(output + '/'_L1 + file);
                const bool matchingRevision = completedCaptureRevision == requestedCaptureRevision;
                passed &= matchingRevision;
                if (image.isNull() || !matchingRevision) {
                    qWarning() << "Canvas readback failed" << index << window.isExposed()
                               << completedCaptureRevision << requestedCaptureRevision;
                    app.exit(6);
                    return;
                }
                QJsonObject record{ { u"mode"_s, index % 3 },
                                    { u"night"_s, index / 3 == 1 },
                                    { u"dpr"_s, window.devicePixelRatio() },
                                    { u"file"_s, file } };
                record.insert(u"captureRevisionMatched"_s, matchingRevision);
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
                record.insert(u"visibleTextPixels"_s, textPixels);
                passed &= textPixels > 100;
                record.insert(u"guiStateSetNs"_s, guiStateNs);
                record.insert(u"quickSyncCpuNs"_s, qint64(syncNs - modeSyncStart));
                record.insert(u"quickRenderCpuNs"_s, qint64(renderNs - modeRenderStart));
                record.insert(u"gpuNanoseconds"_s, QJsonValue::Null);
                const QString painterName = index % 3 == 0 ? u"canvasCpp"_s : u"canvasJs"_s;
                if (auto *item = root->findChild<QObject *>(painterName))
                    record.insert(u"moduleDebug"_s,
                                  QJsonObject::fromVariantMap(item->property("debug").toMap()));
                record.insert(u"attachmentBytesUpperBound"_s,
                              qint64(360 * 160 * window.devicePixelRatio()
                                     * window.devicePixelRatio() * 12));
                if (auto *js = root->findChild<QObject *>("canvasJs")) {
                    record.insert(u"scriptMilliseconds"_s,
                                  js->property("scriptMilliseconds").toDouble());
                    record.insert(u"paintCount"_s, js->property("paintCount").toInt());
                }
                record.insert(u"painterPaints"_s, qint64(canvasProbeStats().paints.load()));
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
            lifecycle.append(
                    QJsonObject{ { u"operation"_s, u"stationary"_s }, { u"passed"_s, stable } });
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
            lifecycle.append(
                    QJsonObject{ { u"operation"_s, u"hidden"_s }, { u"passed"_s, stable } });
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
                const QString file = u"restored-%1.png"_s.arg(lifecycleMode);
                const bool saved = restored.save(output + '/'_L1 + file);
                const bool valid = completedCaptureRevision == requestedCaptureRevision
                        && restored.size() == expectedSize && saved
                        && yalahoo::verification::windowPolicySatisfied(window);
                passed &= valid;
                lifecycle.append(QJsonObject{ { u"operation"_s, u"restore-resize-content"_s },
                                              { u"mode"_s, lifecycleMode },
                                              { u"passed"_s, valid },
                                              { u"file"_s, file } });
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
                        u"\u957f\u6807\u9898\u4e2d\u6587\u4e0e\u7ec4\u5408\u5b57 e\u0301\u3001\u4e0b\u4f38 gyp \U0001f3b5 "_s
                                .repeated(20));
                root->setProperty(
                        "playingTitle",
                        u"\u5f53\u524d\u64ad\u653e\u6b4c\u66f2\u4fdd\u6301\u56fa\u5b9a\u6309\u94ae\u4f4d\u7f6e"_s
                                .repeated(5));
            }
            wait(200);
            return;
        }
        const bool released =
                canvasProbeStats().renderers == 0 && !root->findChild<QObject *>("canvasJs");
        if (lifecycleMode == 0) {
            passed &= released;
            lifecycle.append(QJsonObject{ { u"operation"_s, u"cpp-ten-rebuilds-release"_s },
                                          { u"passed"_s, released } });
            wait(0);
            return;
        }
        auto *shortInfo = root->findChild<QQuickItem *>("browseInfo");
        const bool scrollable =
                shortInfo && shortInfo->property("contentHeight").toDouble() > shortInfo->height();
        passed &= scrollable;
        lifecycle.append(QJsonObject{ { u"operation"_s, u"short-portrait-scroll"_s },
                                      { u"passed"_s, scrollable } });
        for (const auto &[name, action] :
             { std::pair{ "playButton", "play-pause" }, std::pair{ "stopButton", "stop" },
               std::pair{ "openAlbumButton", "open" } }) {
            auto *button = findItem(root, QString::fromUtf8(name));
            const int before = root->property("actionCount").toInt();
            if (button) {
                button->forceActiveFocus(Qt::TabFocusReason);
                QTest::keyClick(&window, Qt::Key_Space);
            }
            const bool routed = button && button->hasActiveFocus()
                    && root->property("actionCount").toInt() == before + 1
                    && root->property("lastAction").toString() == QString::fromUtf8(action);
            passed &= routed;
            lifecycle.append(QJsonObject{ { u"operation"_s, QString::fromUtf8(name) },
                                          { u"passed"_s, routed } });
        }
        for (const auto &name : { "dayButton", "nightButton" }) {
            auto *button = findItem(root, QString::fromUtf8(name));
            if (button) {
                button->forceActiveFocus(Qt::TabFocusReason);
                QTest::keyClick(&window, Qt::Key_Space);
            }
            const bool routed = button
                    && root->property("night").toBool()
                            == (QString::fromUtf8(name) == "nightButton"_L1);
            passed &= routed;
            lifecycle.append(QJsonObject{ { u"operation"_s, QString::fromUtf8(name) },
                                          { u"passed"_s, routed } });
        }
        QJsonArray capturedDiagnostics;
        {
            const QMutexLocker lock(&diagnosticsMutex);
            capturedDiagnostics = diagnostics;
        }
        passed &= released && yalahoo::verification::windowPolicySatisfied(window) && swapped > 0
                && capturedDiagnostics.isEmpty();
        lifecycle.append(
                QJsonObject{ { u"operation"_s, u"ten-rebuilds-release"_s },
                             { u"passed"_s, released },
                             { u"liveRenderers"_s, qint64(canvasProbeStats().renderers.load()) } });
        QJsonObject report{
            { u"passed"_s, passed },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"offscreen"_s, OffscreenSurface::Enabled },
            { u"offscreenResourcePlan"_s, surface.diagnostics() },
            { u"graphicsApi"_s, window.rendererInterface()->graphicsApi() },
            { u"windowPolicy"_s, yalahoo::verification::windowPolicy(window) },
            { u"captureMethod"_s,
              OffscreenSurface::Enabled
                      ? u"QQuickRenderControl Metal texture QRhi readback; native window stays hidden"_s
                      : u"UI grabWindow; Canvas actual submitted swapchain QRhi readback, no forced Canvas redraw"_s },
            { u"browse"_s, captures },
            { u"probes"_s, probes },
            { u"lifecycle"_s, lifecycle },
            { u"diagnostics"_s, capturedDiagnostics },
            { u"resourcePlan"_s,
              QJsonObject{ { u"probeSize"_s, QJsonArray{ 360, 160 } },
                           { u"sampleCount"_s, 1 },
                           { u"colorFormat"_s, u"RGBA8"_s },
                           { u"mips"_s, false },
                           { u"resizeSimultaneousAttachmentBytesUpperBound"_s,
                             qint64((360 * 160 + 420 * 200) * window.devicePixelRatio()
                                    * window.devicePixelRatio() * 12) },
                           { u"fontAtlasAndInternalBufferBytes"_s, QJsonValue::Null },
                           { u"qtMainOutputAndSwapchainBytes"_s, QJsonValue::Null } } },
            { u"metrics"_s,
              QJsonObject{
                      { u"cppPaints"_s, qint64(canvasProbeStats().paints.load()) },
                      { u"cppPaintCpuNs"_s, qint64(canvasProbeStats().paintNanoseconds.load()) },
                      { u"cppSyncCpuNs"_s, qint64(canvasProbeStats().syncNanoseconds.load()) },
                      { u"cppSyncCount"_s, qint64(canvasProbeStats().synchronizations.load()) },
                      { u"gpuNanoseconds"_s, QJsonValue::Null },
                      { u"imageBytes"_s, qint64(canvasProbeStats().imageBytes.load()) },
                      { u"imageCount"_s, qint64(canvasProbeStats().imageCount.load()) } } }
        };
        QSaveFile file(output + "/report.json"_L1);
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
