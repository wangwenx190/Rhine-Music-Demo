#include "RulerAudit.h"
#include "RulerController.h"
#include "RulerInk.h"
#include "OrdinalMotion.h"
#include "ImageComparison.h"
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
QJsonObject inkCoverage(const QImage &actual, const QImage &expected, const QJsonObject &state,
                        double dpr, bool narrow, bool night, bool overflow)
{
    if (actual.size() != expected.size() || actual.isNull())
        return { { "passed", false } };
    const QColor background = expected.pixelColor(expected.width() - 1, expected.height() - 1);
    double maximumMeanRgb = 0, maximumOutsideEnvelope = 0;
    int checked = 0;
    for (const auto &value : state["slots"].toArray()) {
        const auto mark = value.toObject();
        if (mark["alpha"].toDouble() <= 0)
            continue;
        const double x = mark["x"].toDouble(), height = mark["height"].toDouble();
        const double width = narrow ? 2 : 3;
        const double full = narrow ? 21 : 30;
        const QRect roi = QRectF((2 + x) * dpr - 1, (2 + (full - height) / 2) * dpr - 1,
                                 width * dpr + 2, height * dpr + 2)
                                  .toAlignedRect()
                                  .intersected(expected.rect());
        if (roi.isEmpty())
            continue;
        double sourceInk = 0, nativeInk = 0;
        const auto weight = [&](QColor color) {
            return (std::abs(color.red() - background.red())
                    + std::abs(color.green() - background.green())
                    + std::abs(color.blue() - background.blue()))
                    / 3.0;
        };
        for (int y = roi.top(); y <= roi.bottom(); ++y)
            for (int pixelX = roi.left(); pixelX <= roi.right(); ++pixelX) {
                sourceInk += weight(expected.pixelColor(pixelX, y));
                nativeInk += weight(actual.pixelColor(pixelX, y));
            }
        const double rulerWidth = state["width"].toDouble();
        const double left = std::clamp(x, 0.0, rulerWidth);
        const double right = std::clamp(x + width, 0.0, rulerWidth);
        double horizontalCoverage = right - left;
        if (overflow) {
            const double gap = narrow ? 6 : 8;
            const auto primitive = [&](double point) {
                if (point < gap)
                    return point * point / (2 * gap);
                if (point <= rulerWidth - gap)
                    return point - gap / 2;
                const double tail = rulerWidth - point;
                return rulerWidth - gap - tail * tail / (2 * gap);
            };
            horizontalCoverage = primitive(right) - primitive(left);
        }
        // The source's continuous rectangle/fade integral bounds ink before rasterization.
        // Fractional-DPR browser layers can lose edge coverage; that loss is not a design target.
        const QColor ink = night ? QColor("#e9eef2") : QColor("#232722");
        const double geometricInk = horizontalCoverage * std::min(height, full) * dpr * dpr
                * mark["alpha"].toDouble() * weight(ink);
        const double lower = std::min(sourceInk, geometricInk);
        const double upper = std::max(sourceInk, geometricInk);
        const double outside = std::max({ lower - nativeInk, nativeInk - upper, 0.0 })
                / (roi.width() * roi.height());
        maximumOutsideEnvelope = std::max(maximumOutsideEnvelope, outside);
        ++checked;
        maximumMeanRgb = std::max(maximumMeanRgb,
                                  std::abs(nativeInk - sourceInk) / (roi.width() * roi.height()));
    }
    // Keep RGB<=6 and the raw error; missing/excess ink must satisfy the source-derived envelope.
    return { { "passed", maximumOutsideEnvelope <= 6 },
             { "checked", checked },
             { "maximumMeanRgb", maximumMeanRgb },
             { "maximumOutsideEnvelopeRgb", maximumOutsideEnvelope } };
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
    QQmlEngine engine;
    QQuickWindow window;
    yalahoo::verification::prepareWindow(window, true);
    window.setPersistentSceneGraph(false);
    window.setPersistentGraphics(false);
    window.resize(136, 36);
    std::unique_ptr<yalahoo::RulerController> controller;
    std::unique_ptr<QObject> object;
    QQuickItem *root = nullptr;
    QJsonArray captures;
    QJsonArray lifecycleChecks;
    QJsonArray invariants;
    std::array<QImage, 3> restoredReferences;
    int lifecycle = -1, lifecycleMode = 0;
    double maximumStateError = 0;
    bool passed = true;
    bool stationaryPending = false;
    bool coverageGuardsChecked = false;
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
    QImage cppImage;
    QElapsedTimer elapsed;
    elapsed.start();
    std::atomic<qint64> syncStart{ 0 }, renderStart{ 0 }, syncNs{ 0 }, renderNs{ 0 };
    std::atomic<bool> requested{ false }, inFlight{ false };
    QRhiReadbackResult readback;
    QImage image;
    QMutex imageMutex;
    std::function<void()> next, consumeImage, requestLifecycle;
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
                if (!requested || inFlight)
                    return;
                auto *ri = window.rendererInterface();
                auto *rhi = static_cast<QRhi *>(
                        ri->getResource(&window, QSGRendererInterface::RhiResource));
                auto *swap = static_cast<QRhiSwapChain *>(
                        ri->getResource(&window, QSGRendererInterface::RhiSwapchainResource));
                if (!rhi || !swap)
                    return;
                requested = false;
                inFlight = true;
                const bool flip = rhi->isYUpInFramebuffer();
                QMetaObject::invokeMethod(
                        &app, [&, frames = rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames) + 1] {
                            drain = frames;
                        });
                readback.completed = [&, flip] {
                    const auto format = readback.format == QRhiTexture::BGRA8
                            ? QImage::Format_ARGB32
                            : QImage::Format_RGBA8888;
                    QImage pixels(reinterpret_cast<const uchar *>(readback.data.constData()),
                                  readback.pixelSize.width(), readback.pixelSize.height(), format);
                    {
                        const QMutexLocker lock(&imageMutex);
                        image = flip ? pixels.flipped(Qt::Vertical) : pixels.copy();
                    }
                    inFlight = false;
                    QMetaObject::invokeMethod(&app, [&] {
                        consumeImage();
                    });
                };
                auto *batch = rhi->nextResourceUpdateBatch();
                batch->readBackTexture(QRhiReadbackDescription(), &readback);
                swap->currentFrameCommandBuffer()->resourceUpdate(batch);
            },
            Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::frameSwapped, &app, [&] {
        if (inFlight && drain-- > 0 && root) {
            root->setProperty("marker", root->property("marker").toInt() + 1);
            window.update();
        }
    });
    const auto finish = [&] {
        passed &= yalahoo::verification::windowPolicySatisfied(window);
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
                                { "maximumStateError", maximumStateError },
                                { "ordinalError", ordinalError },
                                { "captures", captures },
                                { "lifecycle", lifecycleChecks },
                                { "invariants", invariants },
                                { "dpr", window.devicePixelRatio() },
                                { "qtVersion", qVersion() },
                                { "windowPolicy", yalahoo::verification::windowPolicy(window) },
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
                QTimer::singleShot(60, &app, [&] {
                    requested = true;
                    root->setProperty("marker", root->property("marker").toInt() + 1);
                    window.update();
                });
                return;
            }
            stationaryPending = false;
            stationaryPaints = yalahoo::rulerInkStats().paints;
            controller->advance(.05);
            QTimer::singleShot(120, &app, [&] {
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
            const double dpr = window.devicePixelRatio();
            captured = captured.copy(0, 0, qRound(134 * dpr), qRound(34 * dpr));
            QJsonObject comparison;
            if (lifecycle == 0)
                restoredReferences[lifecycleMode] = captured;
            else
                comparison = yalahoo::compareImages(
                        captured, restoredReferences[lifecycleMode],
                        QPolygonF(QRectF(0, 0, captured.width(), captured.height())));
            const bool valid = !captured.isNull()
                    && (lifecycle == 0
                        || (comparison["ssim"].toDouble() >= .98
                            && comparison["meanAbsoluteRgb"].toDouble() <= 6))
                    && yalahoo::verification::windowPolicySatisfied(window)
                    && yalahoo::rulerInkStats().renderers <= 1;
            passed &= valid;
            lifecycleChecks.append(
                    QJsonObject{ { "cycle", lifecycle },
                                 { "mode", lifecycleMode },
                                 { "passed", valid },
                                 { "comparison", comparison },
                                 { "renderers", int(yalahoo::rulerInkStats().renderers) } });
            if (++lifecycleMode < 3) {
                requestLifecycle();
                return;
            }
            lifecycleMode = 0;
            if (++lifecycle <= 10) {
                window.hide();
                controller->setActive(false);
                window.releaseResources();
                QTimer::singleShot(60, &app, [&] {
                    const auto hidden = controller->state();
                    controller->advance(.05);
                    const bool unchanged = hidden == controller->state();
                    invariants.append(QJsonObject{ { "name", "hidden-state" },
                                                   { "cycle", lifecycle },
                                                   { "passed", unchanged } });
                    passed &= unchanged;
                    window.resize(lifecycle % 2 ? QSize(140, 40) : QSize(136, 36));
                    controller->setActive(true);
                    window.show();
                    requestLifecycle();
                });
            } else {
                object.reset();
                root = nullptr;
                window.hide();
                window.releaseResources();
                QTimer::singleShot(60, &app, [&] {
                    const int remaining = yalahoo::rulerInkStats().renderers;
                    invariants.append(QJsonObject{ { "name", "renderer-release" },
                                                   { "remaining", remaining },
                                                   { "passed", remaining == 0 } });
                    passed &= remaining == 0;
                    finish();
                });
            }
            return;
        }
        const auto test = cases[caseIndex].toObject();
        const auto state = test["states"].toArray()[stateIndex - 1].toObject();
        const QString name =
                QString("native-%1-%2-%3.png").arg(caseIndex).arg(stateIndex - 1).arg(mode);
        const double dpr = window.devicePixelRatio();
        captured = captured.copy(0, 0, qRound(134 * dpr), qRound(34 * dpr));
        const auto expected = QImage(web + '/' + state["file"].toString());
        const auto region = QPolygonF(QRectF(0, 0, captured.width(), captured.height()));
        const auto comparison = yalahoo::compareImages(captured, expected, region);
        const QImage alpha(web + "/alpha-" + state["file"].toString());
        const auto nonEdge = yalahoo::compareAntialiasBand(captured, expected, alpha, region);
        const auto coverage = inkCoverage(captured, expected, state["state"].toObject(), dpr,
                                          test["narrow"].toBool(), test["night"].toBool(),
                                          root->property("overflow").toBool());
        if (!coverageGuardsChecked && state["state"].toObject()["width"].toDouble() > 20) {
            QImage missing = expected;
            missing.fill(expected.pixelColor(expected.width() - 1, expected.height() - 1));
            QImage excessive = expected;
            excessive.fill(test["night"].toBool() ? QColor("#e9eef2") : QColor("#232722"));
            for (const auto &guard : { std::pair{ QString("missing-ink-rejected"), missing },
                                       std::pair{ QString("excess-ink-rejected"), excessive } }) {
                const bool rejected =
                        !inkCoverage(guard.second, expected, state["state"].toObject(), dpr,
                                     test["narrow"].toBool(), test["night"].toBool(),
                                     root->property("overflow").toBool())["passed"]
                                 .toBool();
                invariants.append(QJsonObject{ { "name", guard.first }, { "passed", rejected } });
                passed &= rejected;
            }
            coverageGuardsChecked = true;
        }
        const auto edge = yalahoo::edgeDistance(captured, expected, region);
        bool valid = !captured.isNull() && captured.save(output + '/' + name)
                && comparison["valid"].toBool() && nonEdge["valid"].toBool()
                && nonEdge["ssim"].toDouble() >= .98 && nonEdge["meanAbsoluteRgb"].toDouble() <= 6
                && coverage["passed"].toBool() && (!edge.isDouble() || edge.toDouble() <= 1);
        QJsonObject pair;
        if (mode == 0)
            cppImage = captured;
        else {
            pair = yalahoo::compareImages(captured, cppImage, region);
            const auto normalizedPair =
                    yalahoo::compareAntialiasBand(captured, cppImage, alpha, region);
            pair.insert("nonEdge", normalizedPair);
            valid &= normalizedPair["valid"].toBool() && normalizedPair["ssim"].toDouble() >= .98
                    && normalizedPair["meanAbsoluteRgb"].toDouble() <= 6;
        }
        captures.append(QJsonObject{
                { "file", name },
                { "mode", mode },
                { "passed", valid },
                { "web", comparison },
                { "nonEdge", nonEdge },
                { "inkCoverage", coverage },
                { "edgeDistancePixels", edge },
                { "cpp", pair },
                { "jsPaints", root->property("jsPaints").toInt() },
                { "jsScriptMs", root->property("jsMilliseconds").toDouble() },
                { "quickSyncCpuNs", qint64(syncNs) - modeSyncStart },
                { "quickRenderCpuNs", qint64(renderNs) - modeRenderStart },
                { "colorDepthEstimate",
                  qint64(std::ceil(controller->width() * dpr)
                         * std::ceil((test["narrow"].toBool() ? 21 : 30) * dpr) * 12) } });
        passed &= valid;
        if (!valid)
            qWarning() << "Ruler image" << caseIndex << stateIndex - 1 << mode << comparison;
        if (++mode < 3) {
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            root->setProperty("mode", mode);
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            QTimer::singleShot(60, &app, [&] {
                requested = true;
                root->setProperty("marker", root->property("marker").toInt() + 1);
                window.update();
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
            if (std::abs(test["dpr"].toDouble() - window.devicePixelRatio()) > .001) {
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
                          { "night", test["night"].toBool() },
                          { "physicalDpr", window.devicePixelRatio() } }));
                root = qobject_cast<QQuickItem *>(object.get());
                if (!root) {
                    qWarning() << component.errors();
                    app.exit(4);
                    return;
                }
                root->setParentItem(window.contentItem());
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
            if (!state["file"].isString())
                continue;
            modeSyncStart = syncNs;
            modeRenderStart = renderNs;
            QTimer::singleShot(60, &app, [&] {
                requested = true;
                root->setProperty("marker", root->property("marker").toInt() + 1);
                window.update();
            });
            return;
        }
        // Measure a visible, settled ruler after mode-switch/resource initialization submissions.
        controller->configure(37, 5, 5, true, true);
        for (int frame = 0; frame < 240; ++frame)
            controller->advance(1.0 / 120);
        root->setProperty("overflow", true);
        stationaryPending = true;
        QTimer::singleShot(60, &app, [&] {
            requested = true;
            root->setProperty("marker", root->property("marker").toInt() + 1);
            window.update();
        });
    };
    requestLifecycle = [&] {
        root->setProperty("mode", lifecycleMode);
        QTimer::singleShot(60, &app, [&] {
            requested = true;
            root->setProperty("marker", root->property("marker").toInt() + 1);
            window.update();
        });
    };
    QTimer::singleShot(60000, &app, [&] {
        app.exit(10);
    });
    window.show();
    QTimer::singleShot(0, &app, [&] {
        next();
    });
    return app.exec();
}
