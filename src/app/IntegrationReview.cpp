#include "IntegrationReview.h"
#include "SceneItem.h"
#include "VerificationWindow.h"
#include "InputRouter.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QPointer>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMouseEvent>
#include <QSaveFile>
#include <QScreen>
#include <QTimer>
#include <chrono>
#include <memory>
#include <mutex>
#include <algorithm>
#include <limits>

namespace yalahoo {
namespace {
qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}
QJsonObject distribution(QVector<double> values, double period)
{
    if (values.isEmpty())
        return { { "count", 0 } };
    int consecutive = 0, longest = 0;
    for (double value : values) {
        consecutive = value > 2 * period ? consecutive + 1 : 0;
        longest = qMax(longest, consecutive);
    }
    std::sort(values.begin(), values.end());
    auto percentile = [&](double p) {
        return values[qBound(0, int(std::ceil(p * values.size())) - 1, int(values.size()) - 1)];
    };
    auto over = [&](double p) {
        return int(std::count_if(values.begin(), values.end(), [&](double v) {
            return v > p;
        }));
    };
    return { { "longestConsecutiveOverTwoPeriods", longest },
             { "count", values.size() },
             { "p50Ms", percentile(.5) },
             { "p95Ms", percentile(.95) },
             { "p99Ms", percentile(.99) },
             { "maxMs", values.last() },
             { "overOnePeriod", over(period) },
             { "overTwoPeriods", over(2 * period) },
             { "overThreePeriods", over(3 * period) } };
}
void click(QQuickWindow &window, const QString &name)
{
    auto *button = window.findChild<QQuickItem *>(name);
    if (!button)
        return;
    const auto point = button->mapToScene({ button->width() / 2, button->height() / 2 });
    QMouseEvent press(QEvent::MouseButtonPress, point, window.mapToGlobal(point), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, window.mapToGlobal(point),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &press);
    QCoreApplication::sendEvent(&window, &release);
}
}
int reviewIntegration(QGuiApplication &app, QQuickWindow &initialWindow, SceneAdapter &scene,
                      const QString &output, double seconds)
{
    if (!std::isfinite(seconds) || seconds < 12 || seconds > 120)
        return 20;
    QPointer<QQuickWindow> window = &initialWindow;
    QQmlComponent component(qmlEngine(&initialWindow));
    component.loadFromModule("Yalahoo.Music", "BootstrapProbe");
    QVariantMap initialProperties;
    for (const auto *name :
         { "librarySession", "motionController", "inputRouter", "rulerController", "sceneAdapter",
           "verificationRun", "verificationFlags", "geometryProbe" })
        initialProperties.insert(QString::fromLatin1(name), initialWindow.property(name));
    std::unique_ptr<QObject> rebuiltRoot;
    const bool quitOnClose = app.quitOnLastWindowClosed();
    app.setQuitOnLastWindowClosed(false);
    quint64 windowEpoch = 0;
    std::function<void()> bindWindow;
    auto *item = window->findChild<rhi::SceneItem *>("nativeScene");
    if (!item || window->isVisible() || !verification::windowPolicySatisfied(*window))
        return 20;
    QDir().mkpath(output);
    item->setTelemetryEnabled(true);
    window->setPersistentGraphics(false);
    window->setPersistentSceneGraph(false);
    window->resize(1920, 1080);
    scene.layout()->setViewport(1920, 1080);
    scene.setDetail(1);
    scene.snapToTarget();
    QObject reviewLifetime;
    QElapsedTimer elapsed;
    elapsed.start();
    struct Events
    {
        std::mutex mutex;
        QVector<qint64> swapped;
        bool overflow = false;
    };
    auto events = std::make_shared<Events>();
    QMetaObject::Connection swapped;
    const auto bindSwaps = [&] {
        swapped = QObject::connect(
                window, &QQuickWindow::frameSwapped, window,
                [events] {
                    std::scoped_lock lock(events->mutex);
                    if (events->swapped.size() < 40000)
                        events->swapped.append(nowNs());
                    else
                        events->overflow = true;
                },
                Qt::DirectConnection);
    };
    bindSwaps();
    QJsonArray samples, actions, resources;
    QSet<quint64> receivedInputs;
    QMap<quint64, QString> inputStages;
    QMap<quint64, qint64> firstSubmittedNs;
    QSet<QString> observedSizes;
    QMap<QString, QVector<double>> inputLatency, cpu, sync;
    QString stage = "day-browse";
    bool passed = true;
    int coalesced = 0;
    qint64 peak = 0;
    qint64 baseBytes = 0, finalBytes = 0;
    QJsonObject lastResources;
    int stageIndex = -1;
    std::function<void()> finish, nextAction;
    std::function<bool(const QJsonObject &)> awaiting;
    int replayIndex = 0;
    QSet<QString> observedPresets;
    int rebuilt = 0;
    auto changeStage = [&](int index) {
        stageIndex = index;
        stage = QStringList{ "day-browse", "day-detail", "night-detail", "night-browse" }[index];
        click(*window, index == 2 ? "themeProbe" : "interactionProbe");
        inputStages.insert(scene.frame().inputRevision, stage);
        actions.append(QJsonObject{ { "stage", stage },
                                    { "inputRevision", qint64(scene.frame().inputRevision) },
                                    { "inputReceivedNs", scene.frame().inputReceivedNs } });
    };
    bindWindow = [&] {
        const auto epoch = ++windowEpoch;
        item->setTelemetryEnabled(true);
        QObject::connect(
                item, &rhi::SceneItem::frameSubmitted, window, [&, epoch](QJsonObject sample) {
                    if (epoch != windowEpoch)
                        return;
                    if (samples.size() >= 40000) {
                        passed = false;
                        return;
                    }
                    const auto revision = quint64(sample["inputRevision"].toInteger());
                    const auto sampleStage = elapsed.elapsed() < (seconds + 1) * 1000
                            ? inputStages.value(revision, "startup")
                            : stage;
                    sample["stage"] = sampleStage;
                    sample["elapsedMs"] = elapsed.elapsed();
                    observedSizes.insert(QString("%1x%2")
                                                 .arg(sample["logicalWidth"].toDouble())
                                                 .arg(sample["logicalHeight"].toDouble()));
                    const auto inventory = sample["resources"].toObject();
                    const qint64 bytes = inventory["estimatedCurrentBytes"].toInteger();
                    peak = qMax(peak, inventory["resizeOldPlusNewUpperBoundBytes"].toInteger());
                    if (lastResources != inventory) {
                        auto entry = inventory;
                        entry["stage"] = stage;
                        resources.append(entry);
                        lastResources = inventory;
                    }
                    if (!baseBytes && bytes > 0 && sample["selectedCoverWidth"].toInt() == 1024)
                        baseBytes = bytes;
                    finalBytes = bytes;
                    const bool acknowledged = awaiting && awaiting(sample);
                    sample.remove("resources");
                    coalesced = qMax(coalesced, int(sample["coalescedSamples"].toInteger()));
                    if (revision && !receivedInputs.contains(revision)) {
                        receivedInputs.insert(revision);
                        firstSubmittedNs.insert(revision, sample["submittedNs"].toInteger());
                        const double latency = (sample["submittedNs"].toInteger()
                                                - sample["inputReceivedNs"].toInteger())
                                / 1e6;
                        sample["inputToQtSubmitMs"] = latency;
                        inputLatency[sampleStage].append(latency);
                    }
                    cpu[sampleStage].append(sample["cpuSubmitMs"].toDouble());
                    sync[sampleStage].append(sample["synchronizationMs"].toDouble());
                    samples.append(sample);
                    if (acknowledged) {
                        awaiting = { };
                        QTimer::singleShot(350, Qt::PreciseTimer, window, [&] {
                            nextAction();
                        });
                    }
                });
        QObject::connect(item, &rhi::RhiItem::renderFailed, window, [&](const QString &message) {
            actions.append(QJsonObject{ { "renderError", message } });
            passed = false;
        });
    };
    bindWindow();
    const double period = 1000 / (window->screen() ? window->screen()->refreshRate() : 60);
    // Drive actions once via Qt timers. Production FrameAnimation remains the only frame source.
    for (int i = 0; i < 4; ++i)
        QTimer::singleShot(int((.5 + i * seconds / 4) * 1000), Qt::PreciseTimer, window, [&, i] {
            changeStage(i);
        });
    const int baseTime = int((seconds + 1) * 1000);
    const QVector<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                { 960, 900 },   { 930, 900 },   { 390, 844 },
                                { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const QStringList presets{ "high", "ultra", "performance", "original" };
    // Acknowledge each actual submission before driving the next mutation. Independent coarse
    // timers can expire together and reorder resize/preset/release actions on a busy GUI thread.
    nextAction = [&] {
        const int index = replayIndex++;
        if (index < sizes.size()) {
            stage = "resize";
            const QSize size = sizes[index];
            const auto after = nowNs();
            awaiting = [size, after](const QJsonObject &sample) {
                return sample["submittedNs"].toInteger() >= after
                        && sample["logicalWidth"].toDouble() == size.width()
                        && sample["logicalHeight"].toDouble() == size.height();
            };
            window->resize(size);
            item->update();
            actions.append(QJsonObject{ { "stage", stage },
                                        { "atNs", after },
                                        { "width", size.width() },
                                        { "height", size.height() } });
        } else if (index < sizes.size() + presets.size()) {
            stage = "quality";
            const auto preset = presets[index - sizes.size()];
            const auto after = nowNs();
            awaiting = [&, preset, after](const QJsonObject &sample) {
                const auto target = scene.quality()->targetSize();
                const auto values = RenderQuality::authoredPreset(preset);
                const bool match = sample["submittedNs"].toInteger() >= after
                        && sample["logicalWidth"].toDouble() == 1920
                        && sample["logicalHeight"].toDouble() == 1080
                        && sample["targetWidth"].toInt() == target.width()
                        && sample["targetHeight"].toInt() == target.height()
                        && sample["aoSamples"].toInt() == values["aoSamples"].toInt()
                        && sample["bokeh"].toBool() == (values["depthOfField"].toInt() > 0)
                        && sample["smaa"].toBool() == (values["antialias"] == "smaa");
                if (match)
                    observedPresets.insert(preset);
                return match;
            };
            window->resize(1920, 1080);
            scene.quality()->setPreset(preset);
            item->update();
            actions.append(
                    QJsonObject{ { "stage", stage }, { "preset", preset }, { "atNs", after } });
        } else if (index < sizes.size() + presets.size() + 10) {
            stage = "rebuild";
            const auto after = nowNs();
            awaiting = [&, after](const QJsonObject &sample) {
                const bool match = sample["submittedNs"].toInteger() >= after
                        && sample["resourceGeneration"].toInteger() == 1
                        && sample["selectedUploadBytes"].toInteger() == 1024 * 1024 * 4
                        && sample["resources"].toObject()["estimatedCurrentBytes"].toInteger()
                                == baseBytes;
                if (match)
                    ++rebuilt;
                return match;
            };
            window->hide();
            window->releaseResources();
            const auto size = window->size();
            QObject::disconnect(swapped);
            if (rebuiltRoot)
                rebuiltRoot.reset();
            else
                delete window.data();
            rebuiltRoot.reset(component.createWithInitialProperties(initialProperties));
            window = qobject_cast<QQuickWindow *>(rebuiltRoot.get());
            if (!window) {
                passed = false;
                qWarning() << component.errors();
                app.exit(21);
                return;
            }
            verification::prepareWindow(*window, true);
            window->setPersistentGraphics(false);
            window->setPersistentSceneGraph(false);
            window->resize(size);
            item = window->findChild<rhi::SceneItem *>("nativeScene");
            if (!item) {
                passed = false;
                finish();
                return;
            }
            if (auto *router = qvariant_cast<InputRouter *>(window->property("inputRouter")))
                router->attach(window);
            bindWindow();
            bindSwaps();
            actions.append(QJsonObject{ { "stage", stage },
                                        { "iteration", index - sizes.size() - presets.size() },
                                        { "policy", verification::windowPolicy(*window) } });
            QTimer::singleShot(200, Qt::PreciseTimer, window, [&] {
                window->show();
                passed = passed && verification::windowPolicySatisfied(*window);
            });
        } else {
            finish();
        }
    };
    finish = [&] {
        const auto screenshot = window->grabWindow();
        passed = passed && screenshot.save(output + "/integration.png")
                && verification::windowPolicySatisfied(*window) && samples.size() > 100
                && receivedInputs.size() == 4 && coalesced == 0 && baseBytes == finalBytes
                && observedPresets.size() == 4 && rebuilt == 10;
        for (const auto &size : sizes)
            passed = passed
                    && observedSizes.contains(
                            QString("%1x%2").arg(size.width()).arg(size.height()));
        passed = passed && window->width() == 1920 && window->height() == 1080;
        QJsonArray swappedEvents;
        QMap<QString, QVector<double>> cadence;
        {
            std::scoped_lock lock(events->mutex);
            passed = passed && !events->overflow;
            qint64 previous = 0;
            qint64 normalEnd = std::numeric_limits<qint64>::max();
            for (const auto &value : actions) {
                const auto action = value.toObject();
                if (action["stage"].toString() == "resize") {
                    normalEnd = action["atNs"].toInteger();
                    break;
                }
            }
            for (auto time : events->swapped) {
                QString name = "startup";
                for (auto it = firstSubmittedNs.cbegin(); it != firstSubmittedNs.cend(); ++it)
                    if (time >= it.value())
                        name = inputStages.value(it.key());
                if (previous && time < normalEnd && name != "startup")
                    cadence[name].append((time - previous) / 1e6);
                swappedEvents.append(QJsonObject{ { "swappedNs", time } });
                previous = time;
            }
        }
        QJsonObject distributions;
        for (const auto &name : cpu.keys())
            distributions[name] =
                    QJsonObject{ { "cpuSubmit", distribution(cpu[name], period) },
                                 { "synchronization", distribution(sync[name], period) },
                                 { "inputToQtSubmit", distribution(inputLatency[name], period) },
                                 { "frameSwappedIntervals", distribution(cadence[name], period) } };
        const QJsonObject report{
            { "passed", passed },
            { "qtVersion", qVersion() },
            { "graphicsApi", int(window->rendererInterface()->graphicsApi()) },
            { "windowPolicy", verification::windowPolicy(*window) },
            { "vsyncDisabled", !qgetenv("QSG_NO_VSYNC").isEmpty() },
            { "refreshPeriodMs", period },
            { "durationSeconds", seconds },
            { "width", window->width() },
            { "height", window->height() },
            { "dpr", window->effectiveDevicePixelRatio() },
            { "inputRevisionsSubmitted", receivedInputs.size() },
            { "coalescedSamples", coalesced },
            { "observedPresetCount", observedPresets.size() },
            { "verifiedRebuilds", rebuilt },
            { "resourceInventories", resources },
            { "estimatedReplacementPeakBytes", peak },
            { "estimatedInitialBytes", baseBytes },
            { "estimatedFinalBytes", finalBytes },
            { "actions", actions },
            { "samplingComplete",
              coalesced == 0 && samples.size() > 100 && receivedInputs.size() == 4 },
            { "samples", samples },
            { "frameEvents", swappedEvents },
            { "distributions", distributions },
            { "scope",
              "R10D one-case production bootstrap, both themes, real QML input to Qt command submission. frameSwapped is not scanout; no measured input-to-photon or compositor latency. This probe does not cover the full array/search/presentation/scanning product paths; full product budgets and baseline/noise remain R28. GPU memory values are descriptor estimates, not measured VRAM." }
        };
        QSaveFile file(output + "/runtime.json");
        const bool opened = file.open(QIODevice::WriteOnly);
        if (opened)
            file.write(QJsonDocument(report).toJson());
        const bool saved = opened && file.commit();
        app.exit(passed && saved ? 0 : 21);
    };
    QTimer::singleShot(baseTime, Qt::PreciseTimer, window, [&] {
        nextAction();
    });
    QTimer::singleShot(baseTime + 60000, Qt::PreciseTimer, &reviewLifetime, [&] {
        passed = false;
        actions.append(QJsonObject{ { "error", "Lifecycle submission acknowledgment timed out" } });
        finish();
    });
    window->show();
    QTimer::singleShot(3000, Qt::PreciseTimer, window, [&] {
        actions.append(QJsonObject{ { "stage", "first-show" },
                                    { "windowPolicy", verification::windowPolicy(*window) } });
        if (samples.isEmpty()) {
            passed = false;
            actions.append(
                    QJsonObject{ { "error", "No actual production submission after show" } });
            finish();
        }
    });
    const int result = app.exec();
    QObject::disconnect(swapped);
    if (window) {
        if (item)
            item->setTelemetryEnabled(false);
        window->hide();
        if (rebuiltRoot)
            rebuiltRoot.reset();
        else
            delete window.data();
    }
    app.setQuitOnLastWindowClosed(quitOnClose);
    return result;
}
}
