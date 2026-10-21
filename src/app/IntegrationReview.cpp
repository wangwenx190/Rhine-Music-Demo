#include "ReviewSupport.h"
#include "Units.h"
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
#include <memory>
#include <mutex>
#include <algorithm>
#include <limits>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Bounded diagnostics preserve observed facts; elapsed time and native refresh cadence use explicit units.
constexpr auto MinimumSampleSeconds = 12;
constexpr auto MaximumSampleSeconds = 120;
constexpr auto MaximumDiagnosticSamples = 40000;
constexpr auto FirstInputDelayMilliseconds = 350;
constexpr auto FallbackRefreshHertz = 60;
constexpr auto FinalizeDelayMilliseconds = 200;
constexpr auto MinimumObservedSamples = 100;
constexpr auto FinalizeDeadlineMilliseconds = 60000;
constexpr auto QuitObservationMilliseconds = 3000;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 20,
    IntegrationReviewFailed = 21
};

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
        return int(values.cend() - std::upper_bound(values.cbegin(), values.cend(), p));
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

struct Events final
{
    std::mutex mutex;
    QVector<qint64> swapped;
    bool overflow = false;
};
// Shared references remain valid until this review event loop has stopped.
struct IntegrationReviewState final
{
    quint64 &windowEpoch;
    yalahoo::rhi::SceneItem *&item;
    QPointer<QQuickWindow> &window;
    QJsonArray &samples;
    bool &passed;
    QElapsedTimer &elapsed;
    double &seconds;
    QMap<quint64, QString> &inputStages;
    QString &stage;
    QSet<QString> &observedSizes;
    qint64 &peak;
    QJsonObject &lastResources;
    QJsonArray &resources;
    qint64 &baseBytes;
    qint64 &finalBytes;
    std::function<bool(const QJsonObject &)> &awaiting;
    int &coalesced;
    QSet<quint64> &receivedInputs;
    QMap<quint64, qint64> &firstSubmittedNs;
    QMap<QString, QVector<double>> &inputLatency;
    QMap<QString, QVector<double>> &cpu;
    QMap<QString, QVector<double>> &sync;
    std::function<void()> &nextAction;
    QJsonArray &actions;
    int &replayIndex;
    SceneAdapter &scene;
    QSet<int> &observedEffects;
    int &rebuilt;
    QMetaObject::Connection &swapped;
    std::unique_ptr<QObject> &rebuiltRoot;
    QQmlComponent &component;
    QVariantMap &initialProperties;
    QGuiApplication &app;
    std::function<void()> &finish;
    std::function<void()> &bindWindow;
    const QString &output;
    std::shared_ptr<Events> &events;
};

auto observeIntegrationFrame(const quint64 &epoch, IntegrationReviewState &state,
                             QJsonObject sample)
{
    if (epoch != state.windowEpoch)
        return;
    if (state.samples.size() >= MaximumDiagnosticSamples) {
        state.passed = false;
        return;
    }
    const auto revision = quint64(sample[u"inputRevision"_s].toInteger());
    const auto sampleStage =
            state.elapsed.elapsed() < (state.seconds + 1) * yalahoo::units::MillisecondsPerSecond
            ? state.inputStages.value(revision, u"startup"_s)
            : state.stage;
    sample[u"stage"_s] = sampleStage;
    sample[u"elapsedMs"_s] = state.elapsed.elapsed();
    state.observedSizes.insert(u"%1x%2"_s.arg(sample[u"logicalWidth"_s].toDouble())
                                       .arg(sample[u"logicalHeight"_s].toDouble()));
    const auto inventory = sample[u"resources"_s].toObject();
    const qint64 bytes = inventory[u"estimatedCurrentBytes"_s].toInteger();
    state.peak = qMax(state.peak, inventory[u"resizeOldPlusNewUpperBoundBytes"_s].toInteger());
    if (state.lastResources != inventory) {
        auto entry = inventory;
        entry[u"stage"_s] = state.stage;
        state.resources.append(entry);
        state.lastResources = inventory;
    }
    if (!state.baseBytes && bytes > 0 && sample[u"selectedCoverWidth"_s].toInt() == 1024)
        state.baseBytes = bytes;
    state.finalBytes = bytes;
    const bool acknowledged = state.awaiting && state.awaiting(sample);
    sample.remove(u"resources"_s);
    state.coalesced = qMax(state.coalesced, int(sample[u"coalescedSamples"_s].toInteger()));
    if (revision && !state.receivedInputs.contains(revision)) {
        state.receivedInputs.insert(revision);
        state.firstSubmittedNs.insert(revision, sample[u"submittedNs"_s].toInteger());
        const double latency =
                (sample[u"submittedNs"_s].toInteger() - sample[u"inputReceivedNs"_s].toInteger())
                / yalahoo::units::NanosecondsPerMillisecond;
        sample[u"inputToQtSubmitMs"_s] = latency;
        state.inputLatency[sampleStage].append(latency);
    }
    state.cpu[sampleStage].append(sample[u"cpuSubmitMs"_s].toDouble());
    state.sync[sampleStage].append(sample[u"synchronizationMs"_s].toDouble());
    state.samples.append(sample);
    if (acknowledged) {
        state.awaiting = { };
        QTimer::singleShot(FirstInputDelayMilliseconds, Qt::PreciseTimer, state.window, [&] {
            state.nextAction();
        });
    }
}

auto bindIntegrationWindow(IntegrationReviewState &state)
{
    const auto epoch = ++state.windowEpoch;
    state.item->setTelemetryEnabled(true);
    QObject::connect(state.item, &rhi::SceneItem::frameSubmitted, state.window,
                     [&, epoch](QJsonObject sample) {
                         return observeIntegrationFrame(epoch, state, sample);
                     });
    QObject::connect(state.item, &rhi::RhiItem::renderFailed, state.window,
                     [&](const QString &message) {
                         state.actions.append(QJsonObject{ { u"renderError"_s, message } });
                         state.passed = false;
                     });
}

auto advanceIntegrationReview(IntegrationReviewState &state, auto &sizes, auto &effectStrengths,
                              auto &bindSwaps)
{
    const int index = state.replayIndex++;
    if (index < sizes.size()) {
        state.stage = "resize"_L1;
        const QSize size = sizes[index];
        const auto after = yalahoo::review::nowNs();
        state.awaiting = [size, after](const QJsonObject &sample) {
            return sample[u"submittedNs"_s].toInteger() >= after
                    && sample[u"logicalWidth"_s].toDouble() == size.width()
                    && sample[u"logicalHeight"_s].toDouble() == size.height();
        };
        state.window->resize(size);
        state.item->update();
        state.actions.append(QJsonObject{ { u"stage"_s, state.stage },
                                          { u"atNs"_s, after },
                                          { u"width"_s, size.width() },
                                          { u"height"_s, size.height() } });
    } else if (index < sizes.size() + effectStrengths.size()) {
        state.stage = "quality"_L1;
        const int strength = effectStrengths[index - sizes.size()];
        const auto after = yalahoo::review::nowNs();
        state.awaiting = [&, strength, after](const QJsonObject &sample) {
            const auto target = state.scene.quality()->targetSize();
            const bool match = sample[u"submittedNs"_s].toInteger() >= after
                    && sample[u"logicalWidth"_s].toDouble() == 1920
                    && sample[u"logicalHeight"_s].toDouble() == 1080
                    && sample[u"targetWidth"_s].toInt() == target.width()
                    && sample[u"targetHeight"_s].toInt() == target.height()
                    && sample[u"aoSamples"_s].toInt() == 64
                    && sample[u"bokeh"_s].toBool() == (strength > 0)
                    && sample[u"smaa"_s].toBool() == true;
            if (match)
                state.observedEffects.insert(strength);
            return match;
        };
        state.window->resize(1920, 1080);
        state.scene.quality()->configure({ { u"depthOfField"_s, strength } });
        state.item->update();
        state.actions.append(QJsonObject{ { u"stage"_s, state.stage },
                                          { u"depthOfField"_s, strength },
                                          { u"atNs"_s, after } });
    } else if (index
               < sizes.size() + effectStrengths.size() + yalahoo::review::LifecycleRebuildCount) {
        state.stage = "rebuild"_L1;
        const auto after = yalahoo::review::nowNs();
        state.awaiting = [&, after](const QJsonObject &sample) {
            const bool match = sample[u"submittedNs"_s].toInteger() >= after
                    && sample[u"resourceGeneration"_s].toInteger() == 1
                    && sample[u"selectedUploadBytes"_s].toInteger() > 0
                    && sample[u"resources"_s].toObject()[u"estimatedCurrentBytes"_s].toInteger()
                            == state.baseBytes;
            if (match)
                ++state.rebuilt;
            return match;
        };
        state.window->hide();
        state.window->releaseResources();
        const auto size = state.window->size();
        QObject::disconnect(state.swapped);
        if (state.rebuiltRoot)
            state.rebuiltRoot.reset();
        else
            delete state.window.data();
        state.rebuiltRoot.reset(
                state.component.createWithInitialProperties(state.initialProperties));
        state.window = qobject_cast<QQuickWindow *>(state.rebuiltRoot.get());
        if (!state.window) {
            state.passed = false;
            qCWarning(yalahoo::logApp) << state.component.errors();
            state.app.exit(IntegrationReviewFailed);
            return;
        }
        verification::prepareWindow(*state.window, true);
        state.window->setPersistentGraphics(false);
        state.window->setPersistentSceneGraph(false);
        state.window->resize(size);
        state.item = state.window->findChild<rhi::SceneItem *>("nativeScene");
        if (!state.item) {
            state.passed = false;
            state.finish();
            return;
        }
        if (auto *router = qvariant_cast<InputRouter *>(state.window->property("inputRouter")))
            router->attach(state.window);
        state.bindWindow();
        bindSwaps();
        state.actions.append(
                QJsonObject{ { u"stage"_s, state.stage },
                             { u"iteration"_s, index - sizes.size() - effectStrengths.size() },
                             { u"policy"_s, verification::windowPolicy(*state.window) } });
        QTimer::singleShot(FinalizeDelayMilliseconds, Qt::PreciseTimer, state.window, [&] {
            state.window->show();
            state.passed = state.passed && verification::windowPolicySatisfied(*state.window);
        });
    } else {
        state.finish();
    }
}

auto finishIntegrationReview(IntegrationReviewState &state, auto &sizes, auto &period)
{
    const auto screenshot = state.window->grabWindow();
    state.passed = state.passed && screenshot.save(state.output + "/integration.png"_L1)
            && verification::windowPolicySatisfied(*state.window)
            && state.samples.size() > MinimumObservedSamples && state.receivedInputs.size() == 4
            && state.coalesced == 0 && state.baseBytes == state.finalBytes
            && state.observedEffects.size() == 4
            && state.rebuilt == yalahoo::review::LifecycleRebuildCount;
    for (const auto &size : sizes)
        state.passed = state.passed
                && state.observedSizes.contains(u"%1x%2"_s.arg(size.width()).arg(size.height()));
    state.passed = state.passed && state.window->width() == 1920 && state.window->height() == 1080;
    QJsonArray swappedEvents;
    QMap<QString, QVector<double>> cadence;
    {
        std::scoped_lock lock(state.events->mutex);
        state.passed = state.passed && !state.events->overflow;
        qint64 previous = 0;
        qint64 normalEnd = std::numeric_limits<qint64>::max();
        for (const auto &value : state.actions) {
            const auto action = value.toObject();
            if (action[u"stage"_s].toString() == "resize"_L1) {
                normalEnd = action[u"atNs"_s].toInteger();
                break;
            }
        }
        for (auto time : state.events->swapped) {
            QString name = u"startup"_s;
            for (auto it = state.firstSubmittedNs.cbegin(); it != state.firstSubmittedNs.cend();
                 ++it)
                if (time >= it.value())
                    name = state.inputStages.value(it.key());
            if (previous && time < normalEnd && name != "startup"_L1)
                cadence[name].append((time - previous) / yalahoo::units::NanosecondsPerMillisecond);
            swappedEvents.append(QJsonObject{ { u"swappedNs"_s, time } });
            previous = time;
        }
    }
    QJsonObject distributions;
    for (const auto &name : state.cpu.keys())
        distributions[name] =
                QJsonObject{ { u"cpuSubmit"_s, distribution(state.cpu[name], period) },
                             { u"synchronization"_s, distribution(state.sync[name], period) },
                             { u"inputToQtSubmit"_s,
                               distribution(state.inputLatency[name], period) },
                             { u"frameSwappedIntervals"_s, distribution(cadence[name], period) } };
    const QJsonObject report{
        { u"passed"_s, state.passed },
        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
        { u"graphicsApi"_s, int(state.window->rendererInterface()->graphicsApi()) },
        { u"windowPolicy"_s, verification::windowPolicy(*state.window) },
        { u"vsyncDisabled"_s, state.window->requestedFormat().swapInterval() == 0 },
        { u"refreshPeriodMs"_s, period },
        { u"durationSeconds"_s, state.seconds },
        { u"width"_s, state.window->width() },
        { u"height"_s, state.window->height() },
        { u"dpr"_s, state.window->effectiveDevicePixelRatio() },
        { u"inputRevisionsSubmitted"_s, state.receivedInputs.size() },
        { u"coalescedSamples"_s, state.coalesced },
        { u"observedEffectCount"_s, state.observedEffects.size() },
        { u"verifiedRebuilds"_s, state.rebuilt },
        { u"resourceInventories"_s, state.resources },
        { u"estimatedReplacementPeakBytes"_s, state.peak },
        { u"estimatedInitialBytes"_s, state.baseBytes },
        { u"estimatedFinalBytes"_s, state.finalBytes },
        { u"actions"_s, state.actions },
        { u"samplingComplete"_s,
          state.coalesced == 0 && state.samples.size() > MinimumObservedSamples
                  && state.receivedInputs.size() == 4 },
        { u"samples"_s, state.samples },
        { u"frameEvents"_s, swappedEvents },
        { u"distributions"_s, distributions },
        { u"scope"_s,
          u"R10D one-case production bootstrap, both themes, real QML input to Qt command submission. frameSwapped is not scanout; no measured input-to-photon or compositor latency. This probe does not cover the full array/search/presentation/scanning product paths; full product budgets and baseline/noise remain R28. GPU memory values are descriptor estimates, not measured VRAM."_s }
    };
    QSaveFile file(state.output + "/runtime.json"_L1);
    const bool opened = file.open(QIODevice::WriteOnly);
    if (opened)
        file.write(QJsonDocument(report).toJson());
    const bool saved = opened && file.commit();
    state.app.exit(state.passed && saved ? 0 : IntegrationReviewFailed);
}

auto bindIntegrationSwaps(QMetaObject::Connection &swapped, QPointer<QQuickWindow> &window,
                          const std::shared_ptr<Events> &events)
{
    swapped = QObject::connect(
            window, &QQuickWindow::frameSwapped, window,
            [events] {
                std::scoped_lock lock(events->mutex);
                if (events->swapped.size() < MaximumDiagnosticSamples)
                    events->swapped.append(yalahoo::review::nowNs());
                else
                    events->overflow = true;
            },
            Qt::DirectConnection);
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

    auto events = std::make_shared<Events>();
    QMetaObject::Connection swapped;
    const auto bindSwaps = [&] {
        return bindIntegrationSwaps(swapped, window, events);
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
    IntegrationReviewState state{ windowEpoch,
                                  item,
                                  window,
                                  samples,
                                  passed,
                                  elapsed,
                                  seconds,
                                  inputStages,
                                  stage,
                                  observedSizes,
                                  peak,
                                  lastResources,
                                  resources,
                                  baseBytes,
                                  finalBytes,
                                  awaiting,
                                  coalesced,
                                  receivedInputs,
                                  firstSubmittedNs,
                                  inputLatency,
                                  cpu,
                                  sync,
                                  nextAction,
                                  actions,
                                  replayIndex,
                                  scene,
                                  observedEffects,
                                  rebuilt,
                                  swapped,
                                  rebuiltRoot,
                                  component,
                                  initialProperties,
                                  app,
                                  finish,
                                  bindWindow,
                                  output,
                                  events };
    bindWindow = [&] {
        return bindIntegrationWindow(state);
    };
    bindWindow();
    const double period = yalahoo::units::MillisecondsPerSecond
            / (window->screen() ? window->screen()->refreshRate() : FallbackRefreshHertz);
    // Drive actions once via Qt timers. Production FrameAnimation remains the only frame source.
    for (int i = 0; i < 4; ++i)
        QTimer::singleShot(int((.5 + i * seconds / 4) * yalahoo::units::MillisecondsPerSecond),
                           Qt::PreciseTimer, window, [&, i] {
                               changeStage(i);
                           });
    const int baseTime = int((seconds + 1) * yalahoo::units::MillisecondsPerSecond);
    const QVector<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                { 960, 900 },   { 930, 900 },   { 390, 844 },
                                { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const QList<int> effectStrengths{ 50, 150, 0, 100 };
    // Acknowledge each actual submission before driving the next mutation. Independent coarse
    // timers can expire together and reorder resize/preset/release actions on a busy GUI thread.
    nextAction = [&] {
        return advanceIntegrationReview(state, sizes, effectStrengths, bindSwaps);
    };
    finish = [&] {
        return finishIntegrationReview(state, sizes, period);
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
