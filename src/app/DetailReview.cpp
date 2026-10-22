#include "InputRouter.h"
#include "DetailReview.h"
#include "SelectionController.h"
#include "SelectionLighting.h"
#include <algorithm>
#include "ReviewSupport.h"
#include "SceneGeometry.h"
#include "ProductDefaults.h"
#include "Units.h"
#include "MotionController.h"
#include "BackendCapabilities.h"
#include "SceneAdapter.h"
#include "SceneItem.h"
#include "LibrarySession.h"
#include "PresentationController.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDir>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include "JsonFile.h"
#include <QSet>
#include <QQmlEngine>
#include <QQmlError>
#include <cmath>
#include <atomic>
#include <QThread>
#include <memory>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Anonymous review geometry, input bounds and timeouts are independent of product policy.
constexpr auto ExpectedDetailedCoverPixels = 1024;
constexpr auto MaximumOwnedCovers = 4;
constexpr auto NormalSwitchCount = 4;
constexpr auto ReverseSwitchOrdinal = 3;
constexpr auto MaximumReturningCases = 3;
constexpr auto ClearLidThreshold = .999;
constexpr auto DragDeltaX = 180;
constexpr auto DragDeltaY = 70;
constexpr auto MinimumDragRotation = .5;
constexpr auto ReturnHeightTolerance = 1e-5;
constexpr auto SwitchLiftThreshold = .1;
constexpr auto SlowMotionHoldNanoseconds = 350000000;
constexpr auto HiddenSettleMilliseconds = 250;
constexpr auto HiddenObservationMilliseconds = 150;
constexpr auto ReviewDeadlineMilliseconds = 120000;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 4,
    ReportFailed = 6
};

// Ordered review states describe each awaited fact.
enum ReviewStage {
    AwaitCover = 0,
    AwaitOpen = 1,
    AwaitDrag = 2,
    AwaitRotatedSwitch = 3,
    AwaitSwitchSequence = 4,
    AwaitBurstSelection = 5,
    InterruptSwitch = 6,
    AwaitSearchReturn = 7,
    AwaitSlowMotion = 8,
    AwaitReducedSwitch = 9,
    AwaitResourceReturn = 10,
    AwaitArchive = 11,
    CheckWindow = 12,
    AwaitFinalInput = 13,
    DetailComplete = 14,
    AwaitHiddenWindow = 15,
    AwaitRestoredWindow = 16
};

// Shared references remain valid until this review event loop has stopped.
struct DetailReviewState final
{
    bool &finished;
    int &stage;
    int &contexts;
    int &windowTransition;
    const bool &windowLifecycle;
    yalahoo::rhi::SceneItem *&native;
    QStringList &warnings;
    LibrarySession &library;
    const QString &playback;
    QQuickWindow *&window;
    qint64 &peakOwned;
    qint64 &peakReturning;
    int &switches;
    QJsonArray &samples;
    bool &passed;
    const std::shared_ptr<std::atomic<bool>> &separateRenderThread;
    QJsonArray &checks;
    QJsonArray &states;
    QJsonArray &latencies;
    QJsonArray &windowTransitions;
    QElapsedTimer &time;
    const QString &output;
    QGuiApplication &app;
    QMetaObject::Connection &renderConnection;
    const QThread *&guiThread;
    QObject &reviewLifetime;
    SelectionController *&selection;
    PresentationController &presentation;
    SceneAdapter &scene;
    QString &initialId;
    bool &initialReadinessRecorded;
    qint64 &actionAt;
    QPointF &dragStart;
    double &heldY;
    quint64 &rotatedOwner;
    QString &expected;
    MotionController &motion;
    quint64 &submissionBeforeHide;
};
auto finishDetailReview(DetailReviewState &state, auto &check)
{
    if (state.finished)
        return;
    state.finished = true;
    check(u"stage-completed"_s, state.stage == DetailComplete);
    check(u"requested-window-transitions"_s,
          state.windowTransition == (state.windowLifecycle ? 2 : 0));
    check(u"render-error-empty"_s, state.native->errorText().isEmpty());
    check(u"qml-warnings-empty"_s, state.warnings.isEmpty());
    check(u"playback-independent"_s, state.library.playbackTrackId() == state.playback);
    check(u"window-policy"_s, verification::windowPolicySatisfied(*state.window));
    check(u"bounded-owners"_s,
          state.peakOwned <= MaximumOwnedCovers && state.peakReturning <= MaximumReturningCases);
    check(u"normal-three-switches-and-reverse"_s, state.switches == NormalSwitchCount);
    QSet<qint64> generations, postRevisions;
    qint64 peakUpload = 0, resourceBytes = 0;
    for (const auto &value : state.samples) {
        const auto sample = value.toObject();
        if (sample[u"stage"_s].toInt() < CheckWindow) {
            generations.insert(sample[u"resourceGeneration"_s].toInteger());
            postRevisions.insert(sample[u"postprocessRevision"_s].toInteger());
        }
        peakUpload = std::max(peakUpload, sample[u"atlasUploadBytes"_s].toInteger());
        resourceBytes =
                std::max(resourceBytes,
                         sample[u"resources"_s].toObject()[u"estimatedCurrentBytes"_s].toInteger());
    }
    check(u"transforms-do-not-rebuild-targets"_s,
          generations.size() == 1 && postRevisions.size() == 1);
    check(u"bounded-upload-burst"_s,
          peakUpload <= 2 * yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB);
    QJsonObject report{
        { u"passed"_s, state.passed },
        { u"configuration"_s, u"Release"_s },
        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
        { u"backend"_s,
          QString::fromUtf8(
                  graphicsBackendName(state.window->rendererInterface()->graphicsApi())) },
        { u"renderLoop"_s, qEnvironmentVariable("QSG_RENDER_LOOP", u"Qt default"_s) },
        { u"separateRenderThreadObserved"_s, state.separateRenderThread->load() },
        { u"dpr"_s, state.window->effectiveDevicePixelRatio() },
        { u"windowPolicy"_s, verification::windowPolicy(*state.window) },
        { u"checks"_s, state.checks },
        { u"qmlWarnings"_s, QJsonArray::fromStringList(state.warnings) },
        { u"states"_s, state.states },
        { u"samples"_s, state.samples },
        { u"latencies"_s, state.latencies },
        { u"contexts"_s, state.contexts },
        { u"windowLifecycleRequested"_s, state.windowLifecycle },
        { u"windowTransitions"_s, state.windowTransitions },
        { u"nativeInputSubmission"_s, qint64(state.native->submission()) },
        { u"nativeInputFrameRevision"_s, qint64(state.native->input().scene.revision) },
        { u"nativeItemVisible"_s, state.native->isVisible() },
        { u"peakOwned"_s, state.peakOwned },
        { u"peakAtlasUploadBytes"_s, peakUpload },
        { u"estimatedCurrentResourceMaxBytes"_s, resourceBytes },
        { u"peakReturning"_s, state.peakReturning },
        { u"finalCovers"_s, state.native->coverStatistics() },
        { u"durationMs"_s, state.time.elapsed() },
        { u"measurementScope"_s,
          u"Product Qt submission, readiness and resource telemetry; no display/photon timing or measured VRAM"_s }
    };
    const bool saved =
            yalahoo::writeJsonFile(state.output + "/report.json"_L1, QJsonDocument(report));
    state.app.exit(saved && state.passed ? 0 : ReportFailed);
}

template <typename RecordActionFunction, typename ReadyFunction, typename CaptureFunction,
          typename MouseFunction, typename CheckFunction, typename ButtonFunction,
          typename FinishFunction>
struct DetailStageContext final
{
    DetailReviewState &state;
    const QJsonObject &covers;
    RecordActionFunction &recordAction;
    ReadyFunction &ready;
    CaptureFunction &capture;
    MouseFunction &mouse;
    CheckFunction &check;
    ButtonFunction &button;
    const QString &phase;
    FinishFunction &finish;
};
void advanceDetailAwaitCover(auto &context)
{
    DetailReviewState &state = context.state;
    const QJsonObject &covers = context.covers;
    auto &recordAction = context.recordAction;

    if (!covers[u"selectedDetailed"_s].toBool() || state.samples.isEmpty())
        return;
    state.initialId = state.selection->albumId();
    recordAction();
    state.presentation.open();
    ++state.stage;
    return;
}
void advanceDetailAwaitOpen(auto &context)
{
    DetailReviewState &state = context.state;
    auto &recordAction = context.recordAction;
    auto &ready = context.ready;
    auto &capture = context.capture;
    auto &mouse = context.mouse;
    auto &check = context.check;

    if (!ready())
        return;
    if (!state.initialReadinessRecorded) {
        capture(u"opened"_s);
        state.latencies.append(
                QJsonObject{ { u"action"_s, u"open"_s },
                             { u"readyMs"_s,
                               (state.time.nsecsElapsed() - state.actionAt)
                                       / yalahoo::units::NanosecondsPerMillisecond } });
        state.initialReadinessRecorded = true;
    }
    // Start the rotation from a clear selected lid, so the outgoing
    // copy's independently refrosting state is exercised as well.
    if (state.selection->selectedClarity() < ClearLidThreshold)
        return;
    capture(u"opened-clear"_s);
    {
        const auto center = state.scene.projectWorldPoint(state.scene.frame().caseTransform.map(
                QVector3D(0, yalahoo::sceneGeometry::HingeHeight, 0)));
        check(u"selected-projection"_s, center.has_value());
        if (!center)
            return;
        state.dragStart = { center->x(), center->y() };
        recordAction();
        mouse(QEvent::MouseButtonPress, state.dragStart, Qt::LeftButton);
        mouse(QEvent::MouseMove, state.dragStart + QPointF(DragDeltaX, DragDeltaY), Qt::LeftButton);
        check(u"native-mouse-drag-started"_s, state.selection->dragging());
    }
    ++state.stage;
    return;
}
void advanceDetailAwaitDrag(auto &context)
{
    DetailReviewState &state = context.state;
    auto &recordAction = context.recordAction;
    auto &capture = context.capture;
    auto &check = context.check;
    auto &button = context.button;

    if (state.selection->rotation() < MinimumDragRotation)
        return;
    state.heldY = state.selection->selectedPosition().y;
    state.rotatedOwner = state.selection->instances().last().objectId;
    capture(u"dragged"_s);
    state.expected = state.selection->adjacentAlbum({ }, 1);
    recordAction();
    button(u"detailNext"_s);
    check(u"switch-cancels-drag"_s, !state.selection->dragging());
    ++state.stage;
    return;
}
void advanceDetailAwaitRotatedSwitch(auto &context)
{
    DetailReviewState &state = context.state;
    auto &recordAction = context.recordAction;
    auto &ready = context.ready;
    auto &capture = context.capture;
    auto &check = context.check;
    auto &button = context.button;
    const QString &phase = context.phase;

    if (phase == "switching"_L1) {
        check(u"fixed-detail-composition"_s, state.scene.detail() == 1 && !state.motion.moving());
        for (const auto &copy : state.selection->returningCases())
            if (copy.objectId == state.rotatedOwner && copy.returnY)
                check(u"aligned-before-descending"_s,
                      std::abs(*copy.returnY - state.heldY) < ReturnHeightTolerance
                              && copy.albumId == state.initialId);
    }
    if (!ready())
        return;
    check(u"first-switch-identity"_s, state.selection->albumId() == state.expected);
    capture(u"switched"_s);
    state.latencies.append(QJsonObject{ { u"action"_s, u"rotated-switch"_s },
                                        { u"readyMs"_s,
                                          (state.time.nsecsElapsed() - state.actionAt)
                                                  / yalahoo::units::NanosecondsPerMillisecond } });
    state.switches = 1;
    state.expected = state.selection->adjacentAlbum({ }, 1);
    recordAction();
    button(u"detailNext"_s);
    ++state.stage;
    return;
}
void advanceDetailAwaitSwitchSequence(auto &context)
{
    DetailReviewState &state = context.state;
    auto &recordAction = context.recordAction;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &button = context.button;

    if (!ready())
        return;
    check(u"switch-identity"_s, state.selection->albumId() == state.expected);
    state.latencies.append(QJsonObject{ { u"action"_s, u"switch-%1"_s.arg(state.switches) },
                                        { u"readyMs"_s,
                                          (state.time.nsecsElapsed() - state.actionAt)
                                                  / yalahoo::units::NanosecondsPerMillisecond } });
    ++state.switches;
    if (state.switches < NormalSwitchCount) {
        const int direction = state.switches == ReverseSwitchOrdinal ? -1 : 1;
        state.expected = state.selection->adjacentAlbum({ }, direction);
        recordAction();
        button(direction > 0 ? u"detailNext"_s : u"detailPrevious"_s);
    } else {
        state.presentation.stepAlbum(1);
        state.presentation.stepAlbum(1);
        state.presentation.stepAlbum(-1);
        state.expected = state.presentation.pendingSelection()->albumId;
        ++state.stage;
    }
    return;
}
void advanceDetailAwaitBurstSelection(auto &context)
{
    DetailReviewState &state = context.state;
    auto &recordAction = context.recordAction;
    auto &ready = context.ready;
    auto &check = context.check;

    if (!ready())
        return;
    check(u"burst-latest-target"_s, state.selection->albumId() == state.expected);
    recordAction();
    state.presentation.stepAlbum(1);
    ++state.stage;
    return;
}
void advanceDetailInterruptSwitch(auto &context)
{
    DetailReviewState &state = context.state;
    const QString &phase = context.phase;

    if (phase != "switching"_L1 || state.selection->liftState().value < SwitchLiftThreshold)
        return;
    state.presentation.back();
    // Search uses the existing archive-route port; the search panel belongs to R23.
    state.presentation.select(state.initialId, true, true);
    ++state.stage;
    return;
}
void advanceDetailAwaitSearchReturn(auto &context)
{
    DetailReviewState &state = context.state;
    auto &recordAction = context.recordAction;
    auto &ready = context.ready;
    auto &check = context.check;

    if (!ready())
        return;
    check(u"interrupted-return-search"_s, state.selection->albumId() == state.initialId);
    state.motion.clock()->setSpeed(yalahoo::productDefaults::MinimumMotionSpeed);
    recordAction();
    state.presentation.stepAlbum(1);
    ++state.stage;
    return;
}
void advanceDetailAwaitSlowMotion(auto &context)
{
    DetailReviewState &state = context.state;

    if (state.time.nsecsElapsed() - state.actionAt < SlowMotionHoldNanoseconds)
        return;
    state.motion.clock()->setSpeed(yalahoo::productDefaults::MaximumMotionSpeed);
    state.motion.setReducedMotion(true);
    ++state.stage;
    return;
}
void advanceDetailAwaitReducedSwitch(auto &context)
{
    DetailReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;

    if (!ready())
        return;
    check(u"reduced-complete-and-release"_s, state.selection->returningCases().size() == 0);
    ++state.stage;
    return;
}
void advanceDetailAwaitResourceReturn(auto &context)
{
    DetailReviewState &state = context.state;
    const QJsonObject &covers = context.covers;
    auto &ready = context.ready;
    auto &check = context.check;

    if (!ready())
        return;
    check(u"switch-resource-return"_s,
          state.selection->returningCases().size() == 0 && covers[u"owned"_s].toInt() == 1);
    {
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(state.window, &escape);
    }
    ++state.stage;
    return;
}
void advanceDetailAwaitArchive(auto &context)
{
    DetailReviewState &state = context.state;
    auto &capture = context.capture;

    if (!state.presentation.archiveEnabled() || !state.selection->archiveReady())
        return;
    capture(u"returned"_s);
    state.presentation.open();
    ++state.stage;
    return;
}

auto observeHiddenDetailWindow(DetailReviewState &state, auto &check)
{
    const bool hidden = !state.window->isExposed() && !state.native->renderingActive()
            && !state.motion.clock()->active();
    const auto frames = state.motion.clock()->frameCount();
    const auto submission = state.native->submission();
    check(u"native-window-stops-visuals"_s, hidden);
    if (state.windowTransition == 1) {
        check(u"native-window-minimized"_s, state.window->windowState() == Qt::WindowMinimized);
#ifdef Q_OS_MACOS
        check(u"native-window-miniaturized"_s,
              verification::nativeWindowPolicy(*state.window)[u"miniaturized"_s].toBool());
#endif
    }
    QTimer::singleShot(
            HiddenObservationMilliseconds, &state.reviewLifetime, [&, frames, submission] {
                check(u"native-hidden-no-visual-work"_s,
                      state.motion.clock()->frameCount() == frames
                              && state.native->submission() == submission);
                state.windowTransitions.append(QJsonObject{
                        { u"minimized"_s, state.windowTransition == 1 },
                        { u"hiddenWindowPolicy"_s, verification::windowPolicy(*state.window) } });
                state.stage = AwaitRestoredWindow;
                state.window->showNormal();
                state.window->update();
            });
}

void advanceDetailCheckWindow(auto &context)
{
    DetailReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &button = context.button;

    if (!ready())
        return;
    if (state.windowLifecycle && state.windowTransition < 2) {
        state.stage = AwaitHiddenWindow;
        state.submissionBeforeHide = state.native->submission();
        if (state.windowTransition == 0)
            state.window->hide();
        else
            state.window->showMinimized();
        QTimer::singleShot(HiddenSettleMilliseconds, &state.reviewLifetime, [&] {
            return observeHiddenDetailWindow(state, check);
        });
        return;
    }
    state.expected = state.selection->adjacentAlbum({ }, 1);
    button(u"detailNext"_s);
    state.stage = AwaitFinalInput;
}
void advanceDetailAwaitRestoredWindow(auto &context)
{
    DetailReviewState &state = context.state;
    auto &ready = context.ready;
    auto &capture = context.capture;
    auto &check = context.check;

    if (!ready() || !state.window->isExposed()
        || state.native->submission() <= state.submissionBeforeHide)
        return;
    check(u"native-restored-original-scene"_s,
          yalahoo::review::findVisual(state.window->contentItem(), u"nativeScene"_s)
                  == state.native);
    check(u"native-restored-window-policy"_s, verification::windowPolicySatisfied(*state.window));
    capture(u"window-restored-%1"_s.arg(state.windowTransition));
    state.windowTransitions.append(
            QJsonObject{ { u"restoredWindowPolicy"_s, verification::windowPolicy(*state.window) },
                         { u"submission"_s, qint64(state.native->submission()) } });
    ++state.windowTransition;
    state.stage = CheckWindow;
    return;
}
void advanceDetailAwaitFinalInput(auto &context)
{
    DetailReviewState &state = context.state;
    auto &ready = context.ready;
    auto &capture = context.capture;
    auto &check = context.check;
    auto &finish = context.finish;

    if (!ready())
        return;
    check(u"detail-input-after-window-check"_s, state.selection->albumId() == state.expected);
    capture(u"final-detail-input"_s);
    ++state.stage;
    finish();
    return;
}
void advanceDetailDefault(auto &)
{

    return;
}

auto advanceDetailFrame(DetailReviewState &state, auto &check, auto &finish, auto &recordAction,
                        auto &ready, auto &capture, auto &mouse, auto &button)
{
    if (state.finished)
        return;
    const auto covers = state.native->coverStatistics();
    state.peakOwned = std::max(state.peakOwned, covers[u"owned"_s].toInteger());
    state.peakReturning =
            std::max(state.peakReturning, qint64(state.selection->returningCases().size()));
    const QString phase = state.presentation.phase();
    QJsonArray owners;
    QSet<quint64> ids;
    bool unique = true;
    for (const auto &instance : state.selection->instances()) {
        unique &= !ids.contains(instance.objectId);
        ids.insert(instance.objectId);
        if (instance.owned)
            owners.append(QJsonObject{ { u"objectId"_s, qint64(instance.objectId) },
                                       { u"albumId"_s, instance.albumId },
                                       { u"clarity"_s, instance.clarity } });
    }
    if (!unique || !state.native->errorText().isEmpty()) {
        check(u"unique-identity-and-render"_s, false);
        finish();
        return;
    }
    state.states.append(
            QJsonObject{ { u"timeNs"_s, state.time.nsecsElapsed() },
                         { u"stage"_s, state.stage },
                         { u"phase"_s, phase },
                         { u"detail"_s, state.scene.detail() },
                         { u"selectedClarity"_s, state.selection->selectedClarity() },
                         { u"individualMaterials"_s, state.scene.frame().individualMaterials },
                         { u"lift"_s, state.selection->liftState().value },
                         { u"rotation"_s, state.selection->rotation() },
                         { u"albumId"_s, state.selection->albumId() },
                         { u"owners"_s, owners },
                         { u"covers"_s, covers },
                         { u"frameRevision"_s, qint64(state.scene.frameRevision()) } });
    {
        DetailStageContext context{ state, covers, recordAction, ready, capture,
                                    mouse, check,  button,       phase, finish };
        static constexpr review::StageRoute<decltype(context)> routes[] = {
            { AwaitCover, &advanceDetailAwaitCover },
            { AwaitOpen, &advanceDetailAwaitOpen },
            { AwaitDrag, &advanceDetailAwaitDrag },
            { AwaitRotatedSwitch, &advanceDetailAwaitRotatedSwitch },
            { AwaitSwitchSequence, &advanceDetailAwaitSwitchSequence },
            { AwaitBurstSelection, &advanceDetailAwaitBurstSelection },
            { InterruptSwitch, &advanceDetailInterruptSwitch },
            { AwaitSearchReturn, &advanceDetailAwaitSearchReturn },
            { AwaitSlowMotion, &advanceDetailAwaitSlowMotion },
            { AwaitReducedSwitch, &advanceDetailAwaitReducedSwitch },
            { AwaitResourceReturn, &advanceDetailAwaitResourceReturn },
            { AwaitArchive, &advanceDetailAwaitArchive },
            { CheckWindow, &advanceDetailCheckWindow },
            { AwaitRestoredWindow, &advanceDetailAwaitRestoredWindow },
            { AwaitFinalInput, &advanceDetailAwaitFinalInput }
        };
        const int stageKey = state.stage;
        if (!review::dispatchStage(routes, stageKey, context))
            advanceDetailDefault(context);
    }
}

auto bindDetailWindow(DetailReviewState &state, auto &check, auto &finish, auto &recordAction,
                      auto &ready, auto &capture, auto &mouse, auto &button)
{
    state.native->setTelemetryEnabled(true);
    if (auto *router = state.native->inputRouter())
        router->attach(state.window);
    state.renderConnection = QObject::connect(
            state.window, &QQuickWindow::beforeRendering, state.window,
            [separateRenderThread = state.separateRenderThread, guiThread = state.guiThread] {
                separateRenderThread->store(QThread::currentThread() != guiThread);
            },
            Qt::DirectConnection);
    QObject::connect(
            state.window, &QQuickWindow::sceneGraphInitialized, &state.reviewLifetime,
            [&] {
                ++state.contexts;
            },
            Qt::QueuedConnection);
    QObject::connect(state.native, &rhi::SceneItem::frameSubmitted, &state.reviewLifetime,
                     [&](QJsonObject sample) {
                         sample[u"stage"_s] = state.stage;
                         sample[u"elapsedNs"_s] = state.time.nsecsElapsed();
                         state.samples.append(sample);
                     });
    QObject::connect(
            state.window, &QQuickWindow::frameSwapped, &state.reviewLifetime,
            [&] {
                return advanceDetailFrame(state, check, finish, recordAction, ready, capture, mouse,
                                          button);
            },
            Qt::QueuedConnection);
}
}

int reviewDetail(QGuiApplication &app, QQuickWindow &initialWindow, SceneAdapter &scene,
                 LibrarySession &library, PresentationController &presentation,
                 MotionController &motion, const QString &output)
{
    auto *window = &initialWindow;
    auto *native = qobject_cast<rhi::SceneItem *>(
            yalahoo::review::findVisual(window->contentItem(), u"nativeScene"_s));
    auto *selection = scene.selection();
    if (!native || !selection)
        return InvalidReviewInput;
    QDir().mkpath(output);
    window->setPersistentGraphics(false);
    window->setPersistentSceneGraph(false);
    window->resize(yalahoo::review::WidthPixels, yalahoo::review::HeightPixels);
    scene.lighting()->setNight(qEnvironmentVariableIntValue("YALAHOO_VERIFY_NIGHT") == 1);
    native->setTelemetryEnabled(true);
    const auto playback = library.playbackTrackId();
    QJsonArray checks, samples, states, latencies;
    QElapsedTimer time;
    time.start();
    int stage = AwaitCover, switches = 0, contexts = 0;
    const bool windowLifecycle =
            qEnvironmentVariableIntValue("YALAHOO_VERIFY_WINDOW_LIFECYCLE") == 1;
    int windowTransition = 0;
    quint64 submissionBeforeHide = 0;
    QJsonArray windowTransitions;
    const bool quitOnClose = app.quitOnLastWindowClosed();
    app.setQuitOnLastWindowClosed(false);
    qint64 actionAt = 0, peakOwned = 0, peakReturning = 0;
    QString initialId, expected;
    QStringList warnings;
    const auto separateRenderThread = std::make_shared<std::atomic<bool>>(false);
    const auto *guiThread = app.thread();
    QMetaObject::Connection renderConnection;
    quint64 rotatedOwner = 0;
    double heldY = 0;
    QPointF dragStart;
    bool passed = true, finished = false, initialReadinessRecorded = false;
    QObject reviewLifetime;
    const auto check = [&](const QString &name, bool value) {
        passed &= value;
        checks.append(
                QJsonObject{ { u"name"_s, name }, { u"passed"_s, value }, { u"stage"_s, stage } });
    };
    const auto capture = [&](const QString &name) {
        const auto image = window->grabWindow();
        check("capture-"_L1 + name,
              !image.isNull() && image.save(output + '/'_L1 + name + ".png"_L1));
    };
    const auto mouse = [&](QEvent::Type type, QPointF point, Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, window->mapToGlobal(point),
                          type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(window, &event);
    };
    const auto button = [&](const QString &name) {
        auto *item = yalahoo::review::findVisual(window->contentItem(), name);
        if (!item || !item->isVisible() || !item->isEnabled()) {
            check("button-"_L1 + name, false);
            return;
        }
        const auto point = item->mapToScene({ item->width() / 2, item->height() / 2 });
        mouse(QEvent::MouseButtonPress, point, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, point, Qt::NoButton);
    };
    const auto recordAction = [&] {
        actionAt = time.nsecsElapsed();
        scene.recordInput();
    };
    const auto ready = [&] {
        const auto cover = native->coverStatistics();
        bool selectedOwner = false;
        QHash<quint64, QString> expectedOwners;
        for (const auto &instance : selection->instances())
            if (instance.owned)
                expectedOwners.insert(instance.objectId, instance.albumId);
        const auto owners = cover[u"owners"_s].toArray();
        if (owners.size() != expectedOwners.size())
            return false;
        for (const auto &value : owners) {
            const auto owner = value.toObject();
            const auto objectId = quint64(owner[u"objectId"_s].toInteger());
            if (expectedOwners.value(objectId) != owner[u"albumId"_s].toString())
                return false;
            selectedOwner |= objectId == selection->instances().last().objectId
                    && owner[u"resolution"_s].toInt() == ExpectedDetailedCoverPixels;
        }
        return selectedOwner && presentation.detailEnabled() && selection->canInspect()
                && cover[u"selectedDetailed"_s].toBool();
    };
    DetailReviewState state{ finished,
                             stage,
                             contexts,
                             windowTransition,
                             windowLifecycle,
                             native,
                             warnings,
                             library,
                             playback,
                             window,
                             peakOwned,
                             peakReturning,
                             switches,
                             samples,
                             passed,
                             separateRenderThread,
                             checks,
                             states,
                             latencies,
                             windowTransitions,
                             time,
                             output,
                             app,
                             renderConnection,
                             guiThread,
                             reviewLifetime,
                             selection,
                             presentation,
                             scene,
                             initialId,
                             initialReadinessRecorded,
                             actionAt,
                             dragStart,
                             heldY,
                             rotatedOwner,
                             expected,
                             motion,
                             submissionBeforeHide };
    const auto finish = [&] {
        return finishDetailReview(state, check);
    };
    if (auto *engine = qmlEngine(window))
        QObject::connect(engine, &QQmlEngine::warnings, &reviewLifetime,
                         [&](const QList<QQmlError> &errors) {
                             for (const auto &error : errors)
                                 warnings.append(error.toString());
                         });
    bindDetailWindow(state, check, finish, recordAction, ready, capture, mouse, button);
    QTimer::singleShot(ReviewDeadlineMilliseconds, &reviewLifetime, finish);
    window->show();
    const int result = app.exec();
    QObject::disconnect(renderConnection);
    if (native)
        native->setTelemetryEnabled(false);
    app.setQuitOnLastWindowClosed(quitOnClose);
    return result;
}
}
