#include "FrameClock.h"
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
#include <stdexcept>

using namespace Qt::StringLiterals;

namespace {
double stateError(const QVariantMap &actual, const QJsonObject &expected)
{
    double error = 0;
    for (const auto &key : { u"width"_s, u"scroll"_s, u"scrollVelocity"_s, u"selected"_s })
        error = std::max(error, std::abs(actual.value(key).toDouble() - expected[key].toDouble()));
    const auto a = actual.value(u"slots"_s).toList();
    const auto b = expected[u"slots"_s].toArray();
    if (a.size() != 16 || b.size() != 16)
        return 1e9;
    for (int i = 0; i < 16; ++i) {
        const auto slot = a[i].toMap();
        const auto reference = b[i].toObject();
        for (const auto &key : { u"x"_s, u"ordinal"_s, u"height"_s, u"alpha"_s })
            error = std::max(error,
                             std::abs(slot.value(key).toDouble() - reference[key].toDouble()));
        for (const auto &key : { u"enabled"_s, u"active"_s })
            if (slot.value(key).toBool() != reference[key].toBool())
                return 1e9;
    }
    return error;
}

}
int reviewRuler(QGuiApplication &app, const QString &output)
{
    const QString web =
            (QString::fromUtf8(FIXTURE_ROOT) + u"/verification/windows/navigation/web"_s);
    QFile input(web + "/report.json"_L1);
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    const auto source = QJsonDocument::fromJson(input.readAll()).object();
    const auto cases = source[u"captures"_s].toArray();
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
        auto nextSurface = std::make_unique<OffscreenSurface>(*window, *renderControl);
        yalahoo::verification::prepareWindow(*window, true);
        window->setPersistentSceneGraph(false);
        window->setPersistentGraphics(false);
        window->resize(136, 100);
        return nextSurface;
    };
    surface = makeWindow();
    QJsonObject finalPolicy, finalSurface;
    qreal finalDpr = window->devicePixelRatio();
    qint64 peakSurfaceBytes = 0;
    const auto retainSurfacePlan = [&] {
        auto plan = surface->diagnostics();
        peakSurfaceBytes =
                qMax(peakSurfaceBytes, plan[u"attachmentPeakBytesUpperBound"_s].toInteger());
        plan.insert(u"attachmentPeakBytesUpperBound"_s, peakSurfaceBytes);
        finalSurface = plan;
    };
    bool hasDensity = false;
    for (const auto &value : cases)
        hasDensity |= std::abs(value.toObject()[u"dpr"_s].toDouble() - window->devicePixelRatio())
                <= .001;
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
    const auto ordinalSamples = source[u"ordinalSamples"_s].toArray();
    if (ordinalSamples.isEmpty())
        return 3;
    for (const auto &value : ordinalSamples) {
        const auto sample = value.toObject();
        const auto op = sample[u"op"_s].toObject();
        if (op.contains(u"dt"_s))
            ordinal.advance(op[u"dt"_s].toDouble());
        else
            ordinal.retarget(op[u"target"_s].toDouble(), op[u"instant"_s].toBool());
        ordinalError =
                std::max({ ordinalError, std::abs(ordinal.value - sample[u"value"_s].toDouble()),
                           std::abs(ordinal.velocity - sample[u"velocity"_s].toDouble()) });
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
                        QImage pixels;
                        try {
                            pixels = yalahoo::rhi::readbackImage(readback, flip);
                        } catch (const std::runtime_error &error) {
                            qWarning("Ruler readback failed: %s", error.what());
                        }
                        {
                            const QMutexLocker lock(&imageMutex);
                            image = std::move(pixels);
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
        passed &= finalPolicy[u"passed"_s].toBool() && initialized >= 11 && invalidated >= 10;
        QSaveFile file(output + "/report.json"_L1);
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(8);
            return;
        }
        const auto &stats = yalahoo::rulerInkStats();
        file.write(
                QJsonDocument(
                        QJsonObject{
                                { u"passed"_s, passed },
                                { u"offscreen"_s, OffscreenSurface::Enabled },
                                { u"offscreenResourcePlan"_s, finalSurface },
                                { u"maximumStateError"_s, maximumStateError },
                                { u"ordinalError"_s, ordinalError },
                                { u"initialized"_s, initialized },
                                { u"invalidated"_s, invalidated },
                                { u"captures"_s, captures },
                                { u"lifecycle"_s, lifecycleChecks },
                                { u"invariants"_s, invariants },
                                { u"dpr"_s, finalDpr },
                                { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                                { u"windowPolicy"_s, finalPolicy },
                                { u"canvasPaints"_s, qint64(stats.paints) },
                                { u"canvasSyncs"_s, qint64(stats.syncs) },
                                { u"canvasPaintCpuNs"_s, qint64(stats.paintNs) },
                                { u"canvasSyncCpuNs"_s, qint64(stats.syncNs) },
                                { u"quickSyncCpuNs"_s, qint64(syncNs) },
                                { u"quickRenderCpuNs"_s, qint64(renderNs) },
                                { u"gpuNanoseconds"_s, QJsonValue::Null },
                                { u"measurementScope"_s,
                                  u"Local CPU diagnostics include setup and discrete readback submission frames; GPU completion and display latency are unavailable."_s } })
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
                invariants.append(QJsonObject{ { u"name"_s, u"stationary-paint"_s },
                                               { u"before"_s, qint64(stationaryPaints) },
                                               { u"after"_s, qint64(paints) },
                                               { u"passed"_s, stationaryPaints == paints } });
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
                    text += QString::number(face.toMap()[u"upper"_s].toInt());
            }
            const QString expected =
                    QString::number(root->property("ordinal").toInt()).rightJustified(3, '0'_L1);
            const bool digitState = digits && digits->active() && text == expected;
            passed &= digitState;
            if (lifecycle == 0)
                passed &= captured.save(output + u"/digits-%1.png"_s.arg(lifecycleMode));
            const double dpr = window->devicePixelRatio();
            captured = captured.copy(0, 0, qRound(134 * dpr), qRound(34 * dpr));
            const bool valid = !captured.isNull()
                    && captured.size() == QSize(qRound(134 * dpr), qRound(34 * dpr))
                    && captured.save(output
                                     + u"/restored-%1-%2.png"_s.arg(lifecycle).arg(lifecycleMode))
                    && yalahoo::verification::windowPolicySatisfied(*window)
                    && yalahoo::rulerInkStats().renderers <= 1;
            passed &= valid;
            lifecycleChecks.append(
                    QJsonObject{ { u"cycle"_s, lifecycle },
                                 { u"digits"_s, text },
                                 { u"digitState"_s, digitState },
                                 { u"mode"_s, lifecycleMode },
                                 { u"passed"_s, valid },
                                 { u"renderers"_s, int(yalahoo::rulerInkStats().renderers) } });
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
                    surface = makeWindow();
                    bindWindow();
                    QQmlComponent component(&engine,
                                            QUrl(u"qrc:/qt/qml/Yalahoo/Music/RulerReview.qml"_s));
                    object.reset(component.createWithInitialProperties(
                            { { u"controller"_s, QVariant::fromValue(controller.get()) },
                              { u"clock"_s, QVariant::fromValue(&clock) },
                              { u"night"_s, night },
                              { u"physicalDpr"_s, window->devicePixelRatio() },
                              { u"offscreenRendering"_s, OffscreenSurface::Enabled } }));
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
                        invariants.append(QJsonObject{ { u"name"_s, u"hidden-state"_s },
                                                       { u"cycle"_s, lifecycle },
                                                       { u"passed"_s, unchanged } });
                        passed &= unchanged;
                        window->resize(lifecycle % 2 ? QSize(140, 104) : QSize(136, 100));
                        controller->setActive(true);
                        surface->show();
                        requestLifecycle();
                    });
                };
                if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic"_ba) {
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
                        invariants.append(QJsonObject{ { u"name"_s, u"renderer-release"_s },
                                                       { u"remaining"_s, remaining },
                                                       { u"passed"_s, remaining == 0 } });
                        passed &= remaining == 0;
                        finish();
                    });
                };
                if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic"_ba) {
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
        const auto state = test[u"states"_s].toArray()[stateIndex - 1].toObject();
        const QString name = u"native-%1-%2-%3.png"_s.arg(caseIndex).arg(stateIndex - 1).arg(mode);
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
                && captured.save(output + '/'_L1 + name);
        captures.append(QJsonObject{
                { u"file"_s, name },
                { u"visibleInkPixels"_s, visibleInk },
                { u"mode"_s, mode },
                { u"passed"_s, valid },
                { u"jsPaints"_s, root->property("jsPaints").toInt() },
                { u"jsScriptMs"_s, root->property("jsMilliseconds").toDouble() },
                { u"quickSyncCpuNs"_s, qint64(syncNs) - modeSyncStart },
                { u"quickRenderCpuNs"_s, qint64(renderNs) - modeRenderStart },
                { u"colorDepthEstimate"_s,
                  qint64(std::ceil(controller->width() * dpr)
                         * std::ceil((test[u"narrow"_s].toBool() ? 21 : 30) * dpr) * 12) } });
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
            if (std::abs(test[u"dpr"_s].toDouble() - window->devicePixelRatio()) > .001) {
                ++caseIndex;
                continue;
            }
            if (stateIndex == 0) {
                object.reset();
                controller = std::make_unique<yalahoo::RulerController>(nullptr, nullptr);
                controller->setNarrow(test[u"narrow"_s].toBool());
                QQmlComponent component(&engine,
                                        QUrl(u"qrc:/qt/qml/Yalahoo/Music/RulerReview.qml"_s));
                object.reset(component.createWithInitialProperties(
                        { { u"controller"_s, QVariant::fromValue(controller.get()) },
                          { u"clock"_s, QVariant::fromValue(&clock) },
                          { u"night"_s, test[u"night"_s].toBool() },
                          { u"physicalDpr"_s, window->devicePixelRatio() },
                          { u"offscreenRendering"_s, OffscreenSurface::Enabled } }));
                root = qobject_cast<QQuickItem *>(object.get());
                if (!root) {
                    qWarning() << component.errors();
                    app.exit(4);
                    return;
                }
                root->setParentItem(window->contentItem());
            }
            const auto states = test[u"states"_s].toArray();
            if (stateIndex == states.size()) {
                stateIndex = 0;
                ++caseIndex;
                continue;
            }
            const auto state = states[stateIndex++].toObject();
            const auto op = state[u"op"_s].toObject();
            if (op.contains(u"frames"_s)) {
                const auto trace = state[u"state"_s].toObject()[u"trace"_s].toArray();
                for (int f = 0; f < op[u"frames"_s].toInt(); ++f) {
                    controller->advance(std::min(.05, 1.0 / test[u"fps"_s].toInt()));
                    if (!trace.isEmpty()) {
                        const double error = stateError(controller->state(), trace[f].toObject());
                        maximumStateError = std::max(maximumStateError, error);
                        passed &= error <= 1e-6;
                    }
                }
            } else {
                controller->configure(op[u"count"_s].toInt(), op[u"row"_s].toInt(),
                                      op[u"physical"_s].toDouble(), op[u"loop"_s].toBool(),
                                      op[u"group"_s].toBool());
                root->setProperty("overflow", op[u"count"_s].toInt() > 12);
            }
            const double error = stateError(controller->state(), state[u"state"_s].toObject());
            maximumStateError = std::max(maximumStateError, error);
            passed &= error <= 1e-6;
            if (error > 1e-6)
                qWarning() << "Ruler state mismatch" << test[u"name"_s] << test[u"fps"_s]
                           << stateIndex - 1 << error;
            if (!state[u"capture"_s].toBool())
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
        if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic"_ba) {
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
