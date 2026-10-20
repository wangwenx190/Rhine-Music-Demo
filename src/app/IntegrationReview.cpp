#include "SceneAdapter.h"
#include "Logging.h"
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

namespace {
// Bounded diagnostics preserve observed facts; elapsed time and native refresh cadence use explicit units.
constexpr auto MinimumSampleSeconds = 12;
constexpr auto MaximumSampleSeconds = 120;
constexpr auto MaximumDiagnosticSamples = 40000;
constexpr auto MillisecondsPerSecond = 1000;
constexpr auto NanosecondsPerMillisecond = 1e6;
constexpr auto FirstInputDelayMilliseconds = 350;
constexpr auto FallbackRefreshHertz = 60;
constexpr auto LifecycleRebuildCount = 10;
constexpr auto FinalizeDelayMilliseconds = 200;
constexpr auto MinimumObservedSamples = 100;
constexpr auto FinalizeDeadlineMilliseconds = 60000;
constexpr auto QuitObservationMilliseconds = 3000;
}

namespace {
// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 20,
    IntegrationReviewFailed = 21
};
}

using namespace Qt::StringLiterals;

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
        return { { u"count"_s, 0 } };
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
    return { { u"longestConsecutiveOverTwoPeriods"_s, longest },
             { u"count"_s, values.size() },
             { u"p50Ms"_s, percentile(.5) },
             { u"p95Ms"_s, percentile(.95) },
             { u"p99Ms"_s, percentile(.99) },
             { u"maxMs"_s, values.last() },
             { u"overOnePeriod"_s, over(period) },
             { u"overTwoPeriods"_s, over(2 * period) },
             { u"overThreePeriods"_s, over(3 * period) } };
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
    if (!std::isfinite(seconds) || seconds < MinimumSampleSeconds || seconds > MaximumSampleSeconds)
        return InvalidReviewInput;
    QPointer<QQuickWindow> window = &initialWindow;
    QQmlComponent component(qmlEngine(&initialWindow));
    component.loadFromModule("Yalahoo.Music", "BootstrapProbe");
    QVariantMap initialProperties;
    for (const auto *name :
         { "librarySession", "motionController", "inputRouter", "rulerController", "sceneAdapter",
           "verificationRun", "verificationFlags", "geometryProbe" })
        initialProperties.insert(QString::fromUtf8(name), initialWindow.property(name));
    std::unique_ptr<QObject> rebuiltRoot;
    const bool quitOnClose = app.quitOnLastWindowClosed();
    app.setQuitOnLastWindowClosed(false);
    quint64 windowEpoch = 0;
    std::function<void()> bindWindow;
    auto *item = window->findChild<rhi::SceneItem *>("nativeScene");
    if (!item || window->isVisible() || !verification::windowPolicySatisfied(*window))
        return InvalidReviewInput;
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
    struct Events final
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
                    if (events->swapped.size() < MaximumDiagnosticSamples)
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
    QString stage = u"day-browse"_s;
    bool passed = true;
    int coalesced = 0;
    qint64 peak = 0;
    qint64 baseBytes = 0, finalBytes = 0;
    QJsonObject lastResources;
    int stageIndex = -1;
    std::function<void()> finish, nextAction;
    std::function<bool(const QJsonObject &)> awaiting;
    int replayIndex = 0;
    QSet<int> observedEffects;
    int rebuilt = 0;
    auto changeStage = [&](int index) {
        stageIndex = index;
        stage = QStringList{ u"day-browse"_s, u"day-detail"_s, u"night-detail"_s,
                             u"night-browse"_s }[index];
        click(*window, index == 2 ? u"themeProbe"_s : u"interactionProbe"_s);
        inputStages.insert(scene.frame().inputRevision, stage);
        actions.append(QJsonObject{ { u"stage"_s, stage },
                                    { u"inputRevision"_s, qint64(scene.frame().inputRevision) },
                                    { u"inputReceivedNs"_s, scene.frame().inputReceivedNs } });
    };
    bindWindow = [&] {
        const auto epoch = ++windowEpoch;
        item->setTelemetryEnabled(true);
        QObject::connect(
                item, &rhi::SceneItem::frameSubmitted, window, [&, epoch](QJsonObject sample) {
                    if (epoch != windowEpoch)
                        return;
                    if (samples.size() >= MaximumDiagnosticSamples) {
                        passed = false;
                        return;
                    }
                    const auto revision = quint64(sample[u"inputRevision"_s].toInteger());
                    const auto sampleStage =
                            elapsed.elapsed() < (seconds + 1) * MillisecondsPerSecond
                            ? inputStages.value(revision, u"startup"_s)
                            : stage;
                    sample[u"stage"_s] = sampleStage;
                    sample[u"elapsedMs"_s] = elapsed.elapsed();
                    observedSizes.insert(u"%1x%2"_s.arg(sample[u"logicalWidth"_s].toDouble())
                                                 .arg(sample[u"logicalHeight"_s].toDouble()));
                    const auto inventory = sample[u"resources"_s].toObject();
                    const qint64 bytes = inventory[u"estimatedCurrentBytes"_s].toInteger();
                    peak = qMax(peak, inventory[u"resizeOldPlusNewUpperBoundBytes"_s].toInteger());
                    if (lastResources != inventory) {
                        auto entry = inventory;
                        entry[u"stage"_s] = stage;
                        resources.append(entry);
                        lastResources = inventory;
                    }
                    if (!baseBytes && bytes > 0 && sample[u"selectedCoverWidth"_s].toInt() == 1024)
                        baseBytes = bytes;
                    finalBytes = bytes;
                    const bool acknowledged = awaiting && awaiting(sample);
                    sample.remove(u"resources"_s);
                    coalesced = qMax(coalesced, int(sample[u"coalescedSamples"_s].toInteger()));
                    if (revision && !receivedInputs.contains(revision)) {
                        receivedInputs.insert(revision);
                        firstSubmittedNs.insert(revision, sample[u"submittedNs"_s].toInteger());
                        const double latency = (sample[u"submittedNs"_s].toInteger()
                                                - sample[u"inputReceivedNs"_s].toInteger())
                                / NanosecondsPerMillisecond;
                        sample[u"inputToQtSubmitMs"_s] = latency;
                        inputLatency[sampleStage].append(latency);
                    }
                    cpu[sampleStage].append(sample[u"cpuSubmitMs"_s].toDouble());
                    sync[sampleStage].append(sample[u"synchronizationMs"_s].toDouble());
                    samples.append(sample);
                    if (acknowledged) {
                        awaiting = { };
                        QTimer::singleShot(FirstInputDelayMilliseconds, Qt::PreciseTimer, window,
                                           [&] {
                                               nextAction();
                                           });
                    }
                });
        QObject::connect(item, &rhi::RhiItem::renderFailed, window, [&](const QString &message) {
            actions.append(QJsonObject{ { u"renderError"_s, message } });
            passed = false;
        });
    };
    bindWindow();
    const double period = MillisecondsPerSecond
            / (window->screen() ? window->screen()->refreshRate() : FallbackRefreshHertz);
    // Drive actions once via Qt timers. Production FrameAnimation remains the only frame source.
    for (int i = 0; i < 4; ++i)
        QTimer::singleShot(int((.5 + i * seconds / 4) * MillisecondsPerSecond), Qt::PreciseTimer,
                           window, [&, i] {
                               changeStage(i);
                           });
    const int baseTime = int((seconds + 1) * MillisecondsPerSecond);
    const QVector<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                { 960, 900 },   { 930, 900 },   { 390, 844 },
                                { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const QList<int> effectStrengths{ 50, 150, 0, 100 };
    // Acknowledge each actual submission before driving the next mutation. Independent coarse
    // timers can expire together and reorder resize/preset/release actions on a busy GUI thread.
    nextAction = [&] {
        const int index = replayIndex++;
        if (index < sizes.size()) {
            stage = "resize"_L1;
            const QSize size = sizes[index];
            const auto after = nowNs();
            awaiting = [size, after](const QJsonObject &sample) {
                return sample[u"submittedNs"_s].toInteger() >= after
                        && sample[u"logicalWidth"_s].toDouble() == size.width()
                        && sample[u"logicalHeight"_s].toDouble() == size.height();
            };
            window->resize(size);
            item->update();
            actions.append(QJsonObject{ { u"stage"_s, stage },
                                        { u"atNs"_s, after },
                                        { u"width"_s, size.width() },
                                        { u"height"_s, size.height() } });
        } else if (index < sizes.size() + effectStrengths.size()) {
            stage = "quality"_L1;
            const int strength = effectStrengths[index - sizes.size()];
            const auto after = nowNs();
            awaiting = [&, strength, after](const QJsonObject &sample) {
                const auto target = scene.quality()->targetSize();
                const bool match = sample[u"submittedNs"_s].toInteger() >= after
                        && sample[u"logicalWidth"_s].toDouble() == 1920
                        && sample[u"logicalHeight"_s].toDouble() == 1080
                        && sample[u"targetWidth"_s].toInt() == target.width()
                        && sample[u"targetHeight"_s].toInt() == target.height()
                        && sample[u"aoSamples"_s].toInt() == 64
                        && sample[u"bokeh"_s].toBool() == (strength > 0)
                        && sample[u"smaa"_s].toBool() == true;
                if (match)
                    observedEffects.insert(strength);
                return match;
            };
            window->resize(1920, 1080);
            scene.quality()->configure({ { u"depthOfField"_s, strength } });
            item->update();
            actions.append(QJsonObject{
                    { u"stage"_s, stage }, { u"depthOfField"_s, strength }, { u"atNs"_s, after } });
        } else if (index < sizes.size() + effectStrengths.size() + LifecycleRebuildCount) {
            stage = "rebuild"_L1;
            const auto after = nowNs();
            awaiting = [&, after](const QJsonObject &sample) {
                const bool match = sample[u"submittedNs"_s].toInteger() >= after
                        && sample[u"resourceGeneration"_s].toInteger() == 1
                        && sample[u"selectedUploadBytes"_s].toInteger() > 0
                        && sample[u"resources"_s].toObject()[u"estimatedCurrentBytes"_s].toInteger()
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
                qCWarning(yalahoo::logApp) << component.errors();
                app.exit(IntegrationReviewFailed);
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
            actions.append(
                    QJsonObject{ { u"stage"_s, stage },
                                 { u"iteration"_s, index - sizes.size() - effectStrengths.size() },
                                 { u"policy"_s, verification::windowPolicy(*window) } });
            QTimer::singleShot(FinalizeDelayMilliseconds, Qt::PreciseTimer, window, [&] {
                window->show();
                passed = passed && verification::windowPolicySatisfied(*window);
            });
        } else {
            finish();
        }
    };
    finish = [&] {
        const auto screenshot = window->grabWindow();
        passed = passed && screenshot.save(output + "/integration.png"_L1)
                && verification::windowPolicySatisfied(*window)
                && samples.size() > MinimumObservedSamples && receivedInputs.size() == 4
                && coalesced == 0 && baseBytes == finalBytes && observedEffects.size() == 4
                && rebuilt == LifecycleRebuildCount;
        for (const auto &size : sizes)
            passed = passed
                    && observedSizes.contains(u"%1x%2"_s.arg(size.width()).arg(size.height()));
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
                if (action[u"stage"_s].toString() == "resize"_L1) {
                    normalEnd = action[u"atNs"_s].toInteger();
                    break;
                }
            }
            for (auto time : events->swapped) {
                QString name = u"startup"_s;
                for (auto it = firstSubmittedNs.cbegin(); it != firstSubmittedNs.cend(); ++it)
                    if (time >= it.value())
                        name = inputStages.value(it.key());
                if (previous && time < normalEnd && name != "startup"_L1)
                    cadence[name].append((time - previous) / NanosecondsPerMillisecond);
                swappedEvents.append(QJsonObject{ { u"swappedNs"_s, time } });
                previous = time;
            }
        }
        QJsonObject distributions;
        for (const auto &name : cpu.keys())
            distributions[name] =
                    QJsonObject{ { u"cpuSubmit"_s, distribution(cpu[name], period) },
                                 { u"synchronization"_s, distribution(sync[name], period) },
                                 { u"inputToQtSubmit"_s, distribution(inputLatency[name], period) },
                                 { u"frameSwappedIntervals"_s,
                                   distribution(cadence[name], period) } };
        const QJsonObject report{
            { u"passed"_s, passed },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"graphicsApi"_s, int(window->rendererInterface()->graphicsApi()) },
            { u"windowPolicy"_s, verification::windowPolicy(*window) },
            { u"vsyncDisabled"_s, window->requestedFormat().swapInterval() == 0 },
            { u"refreshPeriodMs"_s, period },
            { u"durationSeconds"_s, seconds },
            { u"width"_s, window->width() },
            { u"height"_s, window->height() },
            { u"dpr"_s, window->effectiveDevicePixelRatio() },
            { u"inputRevisionsSubmitted"_s, receivedInputs.size() },
            { u"coalescedSamples"_s, coalesced },
            { u"observedEffectCount"_s, observedEffects.size() },
            { u"verifiedRebuilds"_s, rebuilt },
            { u"resourceInventories"_s, resources },
            { u"estimatedReplacementPeakBytes"_s, peak },
            { u"estimatedInitialBytes"_s, baseBytes },
            { u"estimatedFinalBytes"_s, finalBytes },
            { u"actions"_s, actions },
            { u"samplingComplete"_s,
              coalesced == 0 && samples.size() > MinimumObservedSamples
                      && receivedInputs.size() == 4 },
            { u"samples"_s, samples },
            { u"frameEvents"_s, swappedEvents },
            { u"distributions"_s, distributions },
            { u"scope"_s,
              u"R10D one-case production bootstrap, both themes, real QML input to Qt command submission. frameSwapped is not scanout; no measured input-to-photon or compositor latency. This probe does not cover the full array/search/presentation/scanning product paths; full product budgets and baseline/noise remain R28. GPU memory values are descriptor estimates, not measured VRAM."_s }
        };
        QSaveFile file(output + "/runtime.json"_L1);
        const bool opened = file.open(QIODevice::WriteOnly);
        if (opened)
            file.write(QJsonDocument(report).toJson());
        const bool saved = opened && file.commit();
        app.exit(passed && saved ? 0 : IntegrationReviewFailed);
    };
    QTimer::singleShot(baseTime, Qt::PreciseTimer, window, [&] {
        nextAction();
    });
    QTimer::singleShot(
            baseTime + FinalizeDeadlineMilliseconds, Qt::PreciseTimer, &reviewLifetime, [&] {
                passed = false;
                actions.append(QJsonObject{
                        { u"error"_s, u"Lifecycle submission acknowledgment timed out"_s } });
                finish();
            });
    window->show();
    QTimer::singleShot(QuitObservationMilliseconds, Qt::PreciseTimer, window, [&] {
        actions.append(QJsonObject{ { u"stage"_s, u"first-show"_s },
                                    { u"windowPolicy"_s, verification::windowPolicy(*window) } });
        if (samples.isEmpty()) {
            passed = false;
            actions.append(
                    QJsonObject{ { u"error"_s, u"No actual production submission after show"_s } });
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
