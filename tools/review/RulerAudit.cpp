#include "RulerAudit.h"
#include "RulerController.h"
#include "RulerInk.h"
#include "OrdinalMotion.h"
#include "OffscreenSurface.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMutex>
#include <QMutexLocker>
#include <rhi/qrhi.h>
#include <functional>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace {
double stateError(const QVariantMap &actual, const QJsonObject &expected)
{
    double error = 0;
    for (const auto &key : { "width", "scroll", "scrollVelocity", "selected" })
        error = std::max(error, std::abs(actual.value(key).toDouble() - expected[key].toDouble()));
    const auto a = actual.value("slots").toList();
    const auto b = expected["slots"].toArray();
    if (a.size() != 16 || b.size() != 16)
        return 1e9;
    for (int i = 0; i < 16; ++i) {
        const auto slot = a[i].toMap();
        const auto reference = b[i].toObject();
        for (const auto &key : { "x", "ordinal", "height", "alpha" })
            error = std::max(error,
                             std::abs(slot.value(key).toDouble() - reference[key].toDouble()));
        for (const auto &key : { "enabled", "active" })
            if (slot.value(key).toBool() != reference[key].toBool())
                return 1e9;
    }
    return error;
}

}
int reviewRuler(QGuiApplication &app, const QString &output)
{
    const QString web = QStringLiteral(FIXTURE_ROOT "/verification/windows/navigation/web");
    QFile input(web + "/report.json");
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    const auto source = QJsonDocument::fromJson(input.readAll()).object();
    const auto cases = source["captures"].toArray();
    if (cases.isEmpty())
        return 3;
    QDir().mkpath(output);
    yalahoo::FrameClock clock;
    clock.setActive(true);
    clock.advance(0);
    QQmlEngine engine;
    std::unique_ptr<QQuickRenderControl> renderControl;
    std::unique_ptr<QQuickWindow> window;
    std::unique_ptr<OffscreenSurface> surface;
    quint64 windowEpoch = 0;
    const bool quitOnClose = app.quitOnLastWindowClosed();
    app.setQuitOnLastWindowClosed(false);
    const auto makeWindow = [&] {
        ++windowEpoch;
        renderControl = std::make_unique<QQuickRenderControl>();
        window = std::make_unique<QQuickWindow>(OffscreenSurface::Enabled ? renderControl.get()
                                                                          : nullptr);
        surface = std::make_unique<OffscreenSurface>(*window, *renderControl);
        yalahoo::verification::prepareWindow(*window, true);
        window->setPersistentSceneGraph(false);
        window->setPersistentGraphics(false);
        window->resize(136, 100);
    };
    makeWindow();
    QJsonObject finalPolicy, finalSurface;
    qreal finalDpr = window->devicePixelRatio();
    qint64 peakSurfaceBytes = 0;
    const auto retainSurfacePlan = [&] {
        auto plan = surface->diagnostics();
        peakSurfaceBytes =
                qMax(peakSurfaceBytes, plan["attachmentPeakBytesUpperBound"].toInteger());
        plan.insert("attachmentPeakBytesUpperBound", peakSurfaceBytes);
        finalSurface = plan;
    };
    bool hasDensity = false;
    for (const auto &value : cases)
        hasDensity |=
                std::abs(value.toObject()["dpr"].toDouble() - window->devicePixelRatio()) <= .001;
    if (!hasDensity) {
        qWarning() << "Frozen ruler states have no matching DPR" << window->devicePixelRatio();
        return 3;
    }
    std::unique_ptr<yalahoo::RulerController> controller;
    std::unique_ptr<QObject> object;
    QQuickItem *root = nullptr;
    QJsonArray captures;
    QJsonArray lifecycleChecks;
    QJsonArray invariants;
    int lifecycle = -1, lifecycleMode = 0;
    double maximumStateError = 0;
    bool passed = true;
    int initialized = 0, invalidated = 0;
    bool stationaryPending = false;
    int stationaryWarmup = 2;
    double ordinalError = 0;
    yalahoo::OrdinalTrack ordinal;
    const auto ordinalSamples = source["ordinalSamples"].toArray();
    if (ordinalSamples.isEmpty())
        return 3;
    for (const auto &value : ordinalSamples) {
        const auto sample = value.toObject();
        const auto op = sample["op"].toObject();
        if (op.contains("dt"))
            ordinal.advance(op["dt"].toDouble());
        else
            ordinal.retarget(op["target"].toDouble(), op["instant"].toBool());
        ordinalError =
                std::max({ ordinalError, std::abs(ordinal.value - sample["value"].toDouble()),
                           std::abs(ordinal.velocity - sample["velocity"].toDouble()) });
    }
    passed &= ordinalError <= 1e-6;
    int caseIndex = 0, stateIndex = 0, mode = 0, drain = 0;
    quint64 stationaryPaints = 0;
    qint64 modeSyncStart = 0, modeRenderStart = 0;
    QElapsedTimer elapsed;
    elapsed.start();
    std::atomic<qint64> syncStart{ 0 }, renderStart{ 0 }, syncNs{ 0 }, renderNs{ 0 };
    std::atomic<bool> requested{ false }, inFlight{ false };
    QRhiReadbackResult readback;
    QImage image;
    QMutex imageMutex;
    std::function<void()> next, consumeImage, requestLifecycle;
    QObject reviewLifetime;
    const auto bindWindow = [&] {
        const auto epoch = windowEpoch;
        QObject::connect(window.get(), &QQuickWindow::sceneGraphInitialized, &reviewLifetime, [&] {
            ++initialized;
        });
        QObject::connect(window.get(), &QQuickWindow::sceneGraphInvalidated, &reviewLifetime, [&] {
            ++invalidated;
        });
        QObject::connect(
                window.get(), &QQuickWindow::beforeSynchronizing, window.get(),
                [&] {
                    syncStart = elapsed.nsecsElapsed();
                },
                Qt::DirectConnection);
        QObject::connect(
                window.get(), &QQuickWindow::afterSynchronizing, window.get(),
                [&] {
                    syncNs += elapsed.nsecsElapsed() - syncStart;
                },
                Qt::DirectConnection);
        QObject::connect(
                window.get(), &QQuickWindow::beforeRendering, window.get(),
                [&] {
                    renderStart = elapsed.nsecsElapsed();
                },
                Qt::DirectConnection);
        QObject::connect(
                window.get(), &QQuickWindow::afterRendering, window.get(),
                [&, epoch, targetSurface = surface.get()] {
                    renderNs += elapsed.nsecsElapsed() - renderStart;
                    if (!requested || inFlight)
                        return;
                    auto *rhi = targetSurface->rhi();
                    auto *commandBuffer = targetSurface->commandBuffer();
                    if (!rhi || !commandBuffer)
                        return;
                    requested = false;
                    inFlight = true;
                    const bool flip = rhi->isYUpInFramebuffer();
                    QMetaObject::invokeMethod(
                            &reviewLifetime,
                            [&, epoch,
                             frames = rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames) + 1] {
                                if (epoch == windowEpoch)
                                    drain = frames;
                            },
                            Qt::QueuedConnection);
                    readback.completed = [&, epoch, flip] {
                        const auto format = readback.format == QRhiTexture::BGRA8
                                ? QImage::Format_ARGB32
                                : QImage::Format_RGBA8888;
                        QImage pixels(reinterpret_cast<const uchar *>(readback.data.constData()),
                                      readback.pixelSize.width(), readback.pixelSize.height(),
                                      format);
                        {
                            const QMutexLocker lock(&imageMutex);
                            image = flip ? pixels.flipped(Qt::Vertical) : pixels.copy();
                        }
                        inFlight = false;
                        // RenderControl can complete readback inside endFrame().
                        // Retire windows only after the current RHI frame has returned.
                        QMetaObject::invokeMethod(
                                &reviewLifetime,
                                [&, epoch] {
                                    if (epoch == windowEpoch)
                                        consumeImage();
                                },
                                Qt::QueuedConnection);
                    };
                    auto *batch = rhi->nextResourceUpdateBatch();
                    batch->readBackTexture(targetSurface->readbackDescription(), &readback);
                    commandBuffer->resourceUpdate(batch);
                },
                Qt::DirectConnection);
        surface->connectFrames(&reviewLifetime, [&, epoch] {
            if (epoch != windowEpoch)
                return;
            if (inFlight && drain-- > 0 && root) {
                root->setProperty("marker", root->property("marker").toInt() + 1);
                surface->requestFrame();
            }
        });
    };
    bindWindow();
    const auto finish = [&] {
        passed &= finalPolicy["passed"].toBool() && initialized >= 11 && invalidated >= 10;
        QSaveFile file(output + "/report.json");
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(8);
            return;
        }
        const auto &stats = yalahoo::rulerInkStats();
        file.write(
                QJsonDocument(
                        QJsonObject{
                                { "passed", passed },
                                { "offscreen", OffscreenSurface::Enabled },
                                { "offscreenResourcePlan", finalSurface },
                                { "maximumStateError", maximumStateError },
                                { "ordinalError", ordinalError },
                                { "initialized", initialized },
                                { "invalidated", invalidated },
                                { "captures", captures },
                                { "lifecycle", lifecycleChecks },
                                { "invariants", invariants },
                                { "dpr", finalDpr },
                                { "qtVersion", qVersion() },
                                { "windowPolicy", finalPolicy },
                                { "canvasPaints", qint64(stats.paints) },
                                { "canvasSyncs", qint64(stats.syncs) },
                                { "canvasPaintCpuNs", qint64(stats.paintNs) },
                                { "canvasSyncCpuNs", qint64(stats.syncNs) },
                                { "quickSyncCpuNs", qint64(syncNs) },
                                { "quickRenderCpuNs", qint64(renderNs) },
                                { "gpuNanoseconds", QJsonValue::Null },
                                { "measurementScope",
                                  "Local CPU diagnostics include setup and discrete readback submission frames; GPU completion and display latency are unavailable." } })
                        .toJson());
        app.exit(file.commit() && passed ? 0 : 9);
    };
    consumeImage = [&] {
        QImage captured;
        {
            const QMutexLocker lock(&imageMutex);
            captured = image;
        }
        if (stationaryPending) {
            if (stationaryWarmup-- > 0) {
                QTimer::singleShot(60, &reviewLifetime, [&] {
                    requested = true;
                    root->setProperty("marker", root->property("marker").toInt() + 1);
                    surface->requestFrame();
                });
                return;
            }
            stationaryPending = false;
            stationaryPaints = yalahoo::rulerInkStats().paints;
            controller->advance(.05);
            QTimer::singleShot(120, &reviewLifetime, [&] {
                const quint64 paints = yalahoo::rulerInkStats().paints;
                invariants.append(QJsonObject{ { "name", "stationary-paint" },
                                               { "before", qint64(stationaryPaints) },
                                               { "after", qint64(paints) },
                                               { "passed", stationaryPaints == paints } });
                passed &= stationaryPaints == paints;
                lifecycle = 0;
                controller->configure(37, 5, 5, true, true);
                for (int frame = 0; frame < 240; ++frame)
                    controller->advance(1.0 / 120);
                root->setProperty("overflow", true);
                requestLifecycle();
            });
            return;
        }
        if (lifecycle >= 0) {
            auto *digits = root->findChild<yalahoo::OrdinalMotion *>();
            QString text;
            if (digits) {
                for (const auto &face : digits->faces())
                    text += QString::number(face.toMap()["upper"].toInt());
            }
            const QString expected =
                    QString::number(root->property("ordinal").toInt()).rightJustified(3, '0');
            const bool digitState = digits && digits->active() && text == expected;
            passed &= digitState;
            if (lifecycle == 0)
                passed &= captured.save(output + QString("/digits-%1.png").arg(lifecycleMode));
            const double dpr = window->devicePixelRatio();
            captured = captured.copy(0, 0, qRound(134 * dpr), qRound(34 * dpr));
            const bool valid = !captured.isNull()
                    && captured.size() == QSize(qRound(134 * dpr), qRound(34 * dpr))
                    && captured.save(
                            output
                            + QString("/restored-%1-%2.png").arg(lifecycle).arg(lifecycleMode))
                    && yalahoo::verification::windowPolicySatisfied(*window)
                    && yalahoo::rulerInkStats().renderers <= 1;
            passed &= valid;
            lifecycleChecks.append(
                    QJsonObject{ { "cycle", lifecycle },
                                 { "digits", text },
                                 { "digitState", digitState },
                                 { "mode", lifecycleMode },
                                 { "passed", valid },
                                 { "renderers", int(yalahoo::rulerInkStats().renderers) } });
            if (++lifecycleMode < 3) {
                requestLifecycle();
                return;
            }
            lifecycleMode = 0;
            if (++lifecycle <= 10) {
                controller->setActive(false);
                const bool night = root->property("night").toBool();
                root->setParentItem(nullptr);
                object.reset();
                root = nullptr;
                const auto restoreWindow = [&, night] {
                    surface->hide();
                    window->releaseResources();
                    retainSurfacePlan();
                    auto oldSurface = std::move(surface);
                    window.reset();
                    oldSurface.reset();
                    renderControl.reset();
                    makeWindow();
                    bindWindow();
                    QQmlComponent component(&engine,
                                            QUrl("qrc:/qt/qml/Yalahoo/Music/RulerReview.qml"));
                    object.reset(component.createWithInitialProperties(
                            { { "controller", QVariant::fromValue(controller.get()) },
                              { "clock", QVariant::fromValue(&clock) },
                              { "night", night },
                              { "physicalDpr", window->devicePixelRatio() },
                              { "offscreenRendering", OffscreenSurface::Enabled } }));
                    root = qobject_cast<QQuickItem *>(object.get());
                    if (!root) {
                        qWarning() << component.errors();
                        app.exit(4);
                        return;
                    }
                    root->setParentItem(window->contentItem());
                    root->setProperty("overflow", true);
                    QTimer::singleShot(60, &reviewLifetime, [&] {
                        const auto hidden = controller->state();
                        controller->advance(.05);
                        const bool unchanged = hidden == controller->state();
                        invariants.append(QJsonObject{ { "name", "hidden-state" },
                                                       { "cycle", lifecycle },
                                                       { "passed", unchanged } });
                        passed &= unchanged;
                        window->resize(lifecycle % 2 ? QSize(140, 104) : QSize(136, 100));
                        controller->setActive(true);
                        surface->show();
                        requestLifecycle();
                    });
                };
                if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic") {
                    QObject::connect(
                            window.get(), &QQuickWindow::frameSwapped, &reviewLifetime,
                            restoreWindow,
                            Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
                    surface->requestFrame();
                } else {
                    restoreWindow();
                }
            } else {
                root->setParentItem(nullptr);
                object.reset();
                root = nullptr;
                const auto releaseWindow = [&] {
                    surface->hide();
                    window->releaseResources();
                    finalPolicy = yalahoo::verification::windowPolicy(*window);
                    retainSurfacePlan();
                    finalDpr = window->devicePixelRatio();
                    auto oldSurface = std::move(surface);
                    window.reset();
                    oldSurface.reset();
                    renderControl.reset();
                    QTimer::singleShot(200, &reviewLifetime, [&] {
                        const int remaining = yalahoo::rulerInkStats().renderers;
                        invariants.append(QJsonObject{ { "name", "renderer-release" },
                                                       { "remaining", remaining },
                                                       { "passed", remaining == 0 } });
                        passed &= remaining == 0;
                        finish();
                    });
                };
                if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic") {
                    QObject::connect(
                            window.get(), &QQuickWindow::frameSwapped, &reviewLifetime,
                            releaseWindow,
                            Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
                    surface->requestFrame();
                } else {
                    releaseWindow();
                }
            }
            return;
        }
        const auto test = cases[caseIndex].toObject();
        const auto state = test["states"].toArray()[stateIndex - 1].toObject();
        const QString name =
                QString("native-%1-%2-%3.png").arg(caseIndex).arg(stateIndex - 1).arg(mode);
        const double dpr = window->devicePixelRatio();
        captured = captured.copy(0, 0, qRound(134 * dpr), qRound(34 * dpr));
        int visibleInk = 0;
        if (!captured.isNull()) {
            const auto background =
                    captured.pixelColor(captured.width() - 1, captured.height() - 1);
            for (int y = 0; y < captured.height(); ++y)
                for (int x = 0; x < captured.width(); ++x)
                    visibleInk += captured.pixelColor(x, y) != background;
        }
        bool expectsInk = false;
        const auto &drawing = controller->drawing();
        for (const auto &mark : drawing.marks)
            expectsInk |= mark.alpha > .1 && mark.height > 2 && mark.x >= drawing.gap
                    && mark.x + drawing.tick <= drawing.width - drawing.gap;
        const bool valid = (!expectsInk || visibleInk > 0) && !captured.isNull()
                && captured.size() == QSize(qRound(134 * dpr), qRound(34 * dpr))
                && captured.save(output + '/' + name);
        captures.append(QJsonObject{
                { "file", name },
                { "visibleInkPixels", visibleInk },
                { "mode", mode },
                { "passed", valid },
                { "jsPaints", root->property("jsPaints").toInt() },
                { "jsScriptMs", root->property("jsMilliseconds").toDouble() },
                { "quickSyncCpuNs", qint64(syncNs) - modeSyncStart },
                { "quickRenderCpuNs", qint64(renderNs) - modeRenderStart },
                { "colorDepthEstimate",
                  qint64(std::ceil(controller->width() * dpr)
                         * std::ceil((test["narrow"].toBool() ? 21 : 30) * dpr) * 12) } });
        passed &= valid;
        if (!valid)
            qWarning() << "Ruler image" << caseIndex << stateIndex - 1 << mode;
        if (++mode < 3) {
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            root->setProperty("mode", mode);
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            QTimer::singleShot(60, &reviewLifetime, [&] {
                requested = true;
                root->setProperty("marker", root->property("marker").toInt() + 1);
                surface->requestFrame();
            });
        } else {
            mode = 0;
            root->setProperty("mode", 0);
            next();
        }
    };
    next = [&] {
        while (caseIndex < cases.size()) {
            const auto test = cases[caseIndex].toObject();
            if (std::abs(test["dpr"].toDouble() - window->devicePixelRatio()) > .001) {
                ++caseIndex;
                continue;
            }
            if (stateIndex == 0) {
                object.reset();
                controller = std::make_unique<yalahoo::RulerController>(nullptr, nullptr);
                controller->setNarrow(test["narrow"].toBool());
                QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/RulerReview.qml"));
                object.reset(component.createWithInitialProperties(
                        { { "controller", QVariant::fromValue(controller.get()) },
                          { "clock", QVariant::fromValue(&clock) },
                          { "night", test["night"].toBool() },
                          { "physicalDpr", window->devicePixelRatio() },
                          { "offscreenRendering", OffscreenSurface::Enabled } }));
                root = qobject_cast<QQuickItem *>(object.get());
                if (!root) {
                    qWarning() << component.errors();
                    app.exit(4);
                    return;
                }
                root->setParentItem(window->contentItem());
            }
            const auto states = test["states"].toArray();
            if (stateIndex == states.size()) {
                stateIndex = 0;
                ++caseIndex;
                continue;
            }
            const auto state = states[stateIndex++].toObject();
            const auto op = state["op"].toObject();
            if (op.contains("frames")) {
                const auto trace = state["state"].toObject()["trace"].toArray();
                for (int f = 0; f < op["frames"].toInt(); ++f) {
                    controller->advance(std::min(.05, 1.0 / test["fps"].toInt()));
                    if (!trace.isEmpty()) {
                        const double error = stateError(controller->state(), trace[f].toObject());
                        maximumStateError = std::max(maximumStateError, error);
                        passed &= error <= 1e-6;
                    }
                }
            } else {
                controller->configure(op["count"].toInt(), op["row"].toInt(),
                                      op["physical"].toDouble(), op["loop"].toBool(),
                                      op["group"].toBool());
                root->setProperty("overflow", op["count"].toInt() > 12);
            }
            const double error = stateError(controller->state(), state["state"].toObject());
            maximumStateError = std::max(maximumStateError, error);
            passed &= error <= 1e-6;
            if (error > 1e-6)
                qWarning() << "Ruler state mismatch" << test["name"] << test["fps"]
                           << stateIndex - 1 << error;
            if (!state["capture"].toBool())
                continue;
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            QTimer::singleShot(60, &reviewLifetime, [&] {
                requested = true;
                root->setProperty("marker", root->property("marker").toInt() + 1);
                surface->requestFrame();
            });
            return;
        }
        // Measure a visible, settled ruler after mode-switch/resource initialization submissions.
        controller->configure(37, 5, 5, true, true);
        for (int frame = 0; frame < 240; ++frame)
            controller->advance(1.0 / 120);
        root->setProperty("overflow", true);
        stationaryPending = true;
        QTimer::singleShot(60, &reviewLifetime, [&] {
            requested = true;
            root->setProperty("marker", root->property("marker").toInt() + 1);
            surface->requestFrame();
        });
    };
    requestLifecycle = [&] {
        root->setProperty("mode", lifecycleMode);
        root->setProperty("ordinal", lifecycleMode == 0 ? 123 : lifecycleMode == 1 ? 907 : 42);
        QTimer::singleShot(60, &reviewLifetime, [&] {
            requested = true;
            root->setProperty("marker", root->property("marker").toInt() + 1);
            surface->requestFrame();
        });
    };
    QTimer::singleShot(60000, &reviewLifetime, [&] {
        app.exit(10);
    });
    surface->show();
    QTimer::singleShot(0, &reviewLifetime, [&] {
        next();
    });
    const int result = app.exec();
    if (window) {
        ++windowEpoch;
        requested = false;
        if (root)
            root->setParentItem(nullptr);
        object.reset();
        root = nullptr;
        if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic") {
            QEventLoop cleanup;
            QObject::connect(window.get(), &QQuickWindow::frameSwapped, &cleanup, &QEventLoop::quit,
                             Qt::QueuedConnection);
            QTimer::singleShot(1000, &cleanup, &QEventLoop::quit);
            surface->requestFrame();
            cleanup.exec();
        }
        surface->hide();
        auto oldSurface = std::move(surface);
        window.reset();
        oldSurface.reset();
    }
    app.setQuitOnLastWindowClosed(quitOnClose);
    return result;
}
