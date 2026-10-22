#include "OverviewReview.h"
#include "SelectionController.h"
#include "SelectionLighting.h"
#include <algorithm>
#include <iterator>
#include "ReviewSupport.h"
#include "SceneGeometry.h"
#include "ProductDefaults.h"
#include "Units.h"
#include "MotionController.h"
#include "OverviewController.h"
#include "SceneAdapter.h"
#include "SceneItem.h"
#include "PresentationController.h"
#include "LibrarySession.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDir>
#include <QTimer>
#include <QScopeGuard>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include "JsonFile.h"
#include <QQmlEngine>
#include <QQmlError>
#include <QMutex>
#include <QMutexLocker>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Explicit viewport fixture inputs remain literal data; admission and waiting tolerances are named policy.
constexpr auto ViewportSetupFrames = 10;
constexpr auto MaximumLiveLabels = 18;
constexpr auto RetainedLeftSlackPixels = 18;
constexpr auto LabelBudgetWidth = 308;
constexpr auto LabelBudgetHeight = 129;
constexpr auto LabelColorBudgetBytes = 5721408;
constexpr auto LabelAttachmentBudgetBytes = 17164224;
constexpr auto LabelResizeBudgetBytes = 34328448;
constexpr auto InitialSettleFrames = 30;
constexpr auto OpenedProgressThreshold = .999;
constexpr auto ClosedProgressThreshold = .001;
constexpr auto TransportPositionTolerance = .01;
constexpr auto ExpandedSettleFrames = 35;
constexpr auto HorizontalHitInsetPixels = 50;
constexpr auto TopHitInsetPixels = 230;
constexpr auto BottomHitInsetPixels = 140;
constexpr auto ReversalSettleFrames = 8;
constexpr auto ResizeSettleFrames = 20;
constexpr auto EntryWidthPixels = 76;
constexpr auto LabelRightInsetPixels = 14;
constexpr auto ShortcutSettleFrames = 5;
constexpr auto ReviewDeadlineMilliseconds = 120000;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 4,
    ReportFailed = 6
};

// Ordered review states retain their report ordinals; names describe each awaited fact.
enum ReviewStage {
    AwaitInitialFrame = 0,
    AwaitOverview = 1,
    AwaitExpandedEntry = 2,
    AwaitBrowseFocus = 3,
    AwaitReenteredOverview = 4,
    AwaitColumnNavigation = 5,
    AwaitEscape = 6,
    ClickArchiveCase = 7,
    AwaitCaseReturn = 8,
    ReverseOverview = 9,
    ReverseAgain = 10,
    ReduceOverviewMotion = 11,
    ResizeViewport = 12,
    CheckViewport = 13,
    ExitAfterResize = 14,
    AwaitOverviewRelease = 15,
    AwaitShortcutExit = 16,
    AwaitLabelRelease = 17,
    OverviewComplete = 18
};

// Shared references remain valid until this review event loop has stopped.
struct OverviewReviewState final
{
    bool &finished;
    int &stage;
    const QString &playback;
    LibrarySession &library;
    QStringList &warnings;
    QQuickWindow &window;
    yalahoo::rhi::SceneItem *&native;
    QJsonArray &samples;
    bool &passed;
    QJsonArray &checks;
    QJsonArray &pictures;
    QJsonArray &states;
    int &peakLabels;
    QElapsedTimer &time;
    MotionController &motion;
    SceneAdapter &scene;
    SelectionController *&selection;
    const QString &output;
    QGuiApplication &app;
    int &frames;
    OverviewController &overview;
    QMutex &positionsMutex;
    QPointF &playPosition;
    QPointF &synchronizedPlay;
    QPointF &stopPosition;
    QPointF &synchronizedStop;
    QString &selected;
    int &visibleLane;
    PresentationController &presentation;
    const QList<QSize> &sizes;
    int &sizeIndex;
    const bool &initialNight;
};
auto finishOverviewReview(OverviewReviewState &state, auto &check)
{
    if (state.finished)
        return;
    state.finished = true;
    check(u"sequence-completed"_s, state.stage == OverviewComplete);
    check(u"playback-isolated"_s, state.playback == state.library.playbackTrackId());
    check(u"qml-warnings"_s, state.warnings.isEmpty());
    check(u"window-policy"_s, verification::windowPolicySatisfied(state.window));
    check(u"renderer-errors"_s, state.native->errorText().isEmpty());
    QSet<qint64> generations, postprocess;
    qint64 peakUpload = 0;
    for (const auto value : state.samples) {
        const auto sample = value.toObject();
        if (sample[u"stage"_s].toInt() >= 1 && sample[u"stage"_s].toInt() <= 7) {
            generations.insert(sample[u"resourceGeneration"_s].toInteger());
            postprocess.insert(sample[u"postprocessRevision"_s].toInteger());
        }
        peakUpload = std::max(peakUpload, sample[u"atlasUploadBytes"_s].toInteger());
    }
    check(u"overview-keeps-render-targets"_s, generations.size() == 1 && postprocess.size() == 1);
    check(u"upload-burst"_s,
          peakUpload <= 2 * yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB);
    QJsonObject report{
        { u"passed"_s, state.passed },
        { u"configuration"_s, u"Release"_s },
        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
        { u"backend"_s, qEnvironmentVariable("QSG_RHI_BACKEND") },
        { u"dpr"_s, state.window.effectiveDevicePixelRatio() },
        { u"windowPolicy"_s, verification::windowPolicy(state.window) },
        { u"checks"_s, state.checks },
        { u"pictures"_s, state.pictures },
        { u"qmlWarnings"_s, QJsonArray::fromStringList(state.warnings) },
        { u"states"_s, state.states },
        { u"samples"_s, state.samples },
        { u"peakLabelSlots"_s, state.peakLabels },
        { u"overviewUiAttachments"_s,
          QJsonObject{
                  { u"scope"_s,
                    u"Separate Qt Quick crop/horizontal layers; descriptor estimate, not driver allocation"_s },
                  { u"maximumSlots"_s, MaximumLiveLabels },
                  { u"texturesPerSlot"_s, 2 },
                  { u"width"_s, LabelBudgetWidth },
                  { u"height"_s, LabelBudgetHeight },
                  { u"rgba8PayloadBytesUpperBound"_s, LabelColorBudgetBytes },
                  { u"withDepthStencilUpperBoundBytes"_s, LabelAttachmentBudgetBytes },
                  { u"resizeOverlapUpperBoundBytes"_s, LabelResizeBudgetBytes },
                  { u"actualDriverAllocationBytes"_s, QJsonValue::Null } } },
        { u"peakAtlasUploadBytes"_s, peakUpload },
        { u"durationMs"_s, state.time.elapsed() },
        { u"clockActive"_s, state.motion.clock()->active() },
        { u"applicationVisible"_s, state.motion.clock()->applicationVisible() },
        { u"cameraSettled"_s, state.scene.settled() },
        { u"archiveReady"_s, state.selection->archiveReady() },
        { u"measurementScope"_s,
          u"Qt frame/renderer telemetry and own-window PNG; no input-to-photon or measured VRAM"_s }
    };
    const bool saved =
            yalahoo::writeJsonFile(state.output + "/report.json"_L1, QJsonDocument(report));
    state.app.exit(saved && state.passed ? 0 : ReportFailed);
}

template <typename ReadyFunction, typename ButtonFunction, typename CheckFunction,
          typename FirstLaneFunction, typename CaptureFunction, typename KeyFunction,
          typename ClickPointFunction, typename FinishFunction>
struct OverviewStageContext final
{
    OverviewReviewState &state;
    ReadyFunction &ready;
    ButtonFunction &button;
    CheckFunction &check;
    FirstLaneFunction &firstLane;
    CaptureFunction &capture;
    KeyFunction &key;
    ClickPointFunction &clickPoint;
    FinishFunction &finish;
};
void advanceOverviewAwaitInitialFrame(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &button = context.button;
    auto &check = context.check;

    if (!ready() || state.frames < InitialSettleFrames)
        return;
    {
        QMutexLocker lock(&state.positionsMutex);
        state.playPosition = state.synchronizedPlay;
        state.stopPosition = state.synchronizedStop;
    }
    state.selected = state.selection->albumId();
    button(u"overviewToggle"_s);
    check(u"toggle-enter"_s, state.overview.active());
    ++state.stage;
    return;
}
void advanceOverviewAwaitOverview(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &button = context.button;
    auto &check = context.check;
    auto &firstLane = context.firstLane;

    if (!ready() || state.overview.progress() < OpenedProgressThreshold)
        return;
    {
        QMutexLocker lock(&state.positionsMutex);
        check(u"fixed-transport-buttons"_s,
              QLineF(state.playPosition, state.synchronizedPlay).length()
                              <= TransportPositionTolerance
                      && QLineF(state.stopPosition, state.synchronizedStop).length()
                              <= TransportPositionTolerance);
    }
    state.visibleLane = firstLane();
    check(u"visible-label"_s, state.visibleLane >= 0);
    check(u"no-one-step-entry"_s, !state.overview.enter(state.visibleLane));
    button("overviewLabel-"_L1 + QString::number(state.visibleLane));
    check(u"first-click-only-expands"_s,
          state.overview.active() && state.overview.labels()->canEnter(state.visibleLane)
                  && state.selected == state.selection->albumId());
    state.frames = 0;
    ++state.stage;
    return;
}
void advanceOverviewAwaitExpandedEntry(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &button = context.button;
    auto &check = context.check;
    auto &capture = context.capture;

    if (state.frames < ExpandedSettleFrames)
        return;
    capture(u"expanded"_s);
    button("overviewEnter-"_L1 + QString::number(state.visibleLane));
    check(u"second-click-enters-near"_s,
          !state.overview.active() && !state.presentation.wantsDetail());
    ++state.stage;
    return;
}
void advanceOverviewAwaitBrowseFocus(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &key = context.key;

    if (!ready() || state.overview.progress() > ClosedProgressThreshold
        || !state.presentation.browseSurface()->shown())
        return;
    check(u"return-focus"_s,
          yalahoo::review::findVisual(state.window.contentItem(), u"overviewToggle"_s)
                  ->hasActiveFocus());
    key(Qt::Key_V);
    ++state.stage;
    return;
}
void advanceOverviewAwaitReenteredOverview(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &button = context.button;
    auto &check = context.check;

    if (!ready() || state.overview.progress() < OpenedProgressThreshold)
        return;
    button(u"overviewNext"_s);
    check(u"column-navigation-stays-overview"_s,
          state.overview.active() && !state.overview.labels()->canEnter(state.visibleLane));
    ++state.stage;
    return;
}
void advanceOverviewAwaitColumnNavigation(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &key = context.key;

    if (!ready())
        return;
    key(Qt::Key_Escape);
    check(u"escape-return"_s, !state.overview.active());
    ++state.stage;
    return;
}
void advanceOverviewAwaitEscape(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &key = context.key;

    if (!ready() || state.overview.progress() > ClosedProgressThreshold)
        return;
    key(Qt::Key_V);
    ++state.stage;
    return;
}
void advanceOverviewClickArchiveCase(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &clickPoint = context.clickPoint;
    {
        if (!ready() || state.overview.progress() < OpenedProgressThreshold)
            return;
        bool clicked = false;
        for (const auto &instance : state.selection->instances()) {
            const auto point = state.scene.projectWorldPoint(
                    instance.transform.map({ 0, yalahoo::sceneGeometry::HingeHeight, 0 }));
            if (!point)
                continue;
            if (point->x() < HorizontalHitInsetPixels
                || point->x() > state.window.width() - HorizontalHitInsetPixels
                || point->y() < TopHitInsetPixels
                || point->y() > state.window.height() - BottomHitInsetPixels)
                continue;
            if (!state.scene.hitTest(point->x(), point->y()).isEmpty()) {
                clickPoint({ point->x(), point->y() });
                clicked = true;
                break;
            }
        }
        check(u"album-click-returns"_s, clicked && !state.overview.active());
        ++state.stage;
        return;
    }
}
void advanceOverviewAwaitCaseReturn(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;

    if (!ready() || state.overview.progress() > ClosedProgressThreshold)
        return;
    state.overview.setActive(true);
    state.frames = 0;
    ++state.stage;
    return;
}
void advanceOverviewReverseOverview(auto &context)
{
    OverviewReviewState &state = context.state;

    if (state.frames < ReversalSettleFrames)
        return;
    state.overview.setActive(false);
    state.motion.clock()->setSpeed(yalahoo::productDefaults::MinimumMotionSpeed);
    state.frames = 0;
    ++state.stage;
    return;
}
void advanceOverviewReverseAgain(auto &context)
{
    OverviewReviewState &state = context.state;

    if (state.frames < ReversalSettleFrames)
        return;
    state.overview.setActive(true);
    state.motion.clock()->setSpeed(yalahoo::productDefaults::MaximumMotionSpeed);
    ++state.stage;
    return;
}
void advanceOverviewReduceOverviewMotion(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &check = context.check;

    state.motion.setReducedMotion(true);
    check(u"reduced-finishes-progress"_s, state.overview.progress() == 1);
    state.selection->setRealistic(true);
    state.frames = 0;
    ++state.stage;
    return;
}
void advanceOverviewResizeViewport(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;

    if (!ready() || state.frames < ViewportSetupFrames)
        return;
    state.window.resize(state.sizes[state.sizeIndex]);
    state.scene.lighting()->setNight(state.sizeIndex % 2 ? !state.initialNight
                                                         : state.initialNight);
    state.frames = 0;
    ++state.stage;
    return;
}
void advanceOverviewCheckViewport(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &capture = context.capture;
    {
        if (!ready() || state.frames < ResizeSettleFrames)
            return;
        QVector<QRectF> occupied;
        for (int row = 0; row < state.overview.labels()->rowCount(); ++row) {
            const auto index = state.overview.labels()->index(row);
            if (!state.overview.labels()->data(index, OverviewLabelsModel::Interactive).toBool())
                continue;
            const int lane =
                    state.overview.labels()->data(index, OverviewLabelsModel::Lane).toInt();
            auto *label = yalahoo::review::findVisual(state.window.contentItem(),
                                                      "overviewLabel-"_L1 + QString::number(lane));
            if (!label) {
                check(u"label-object"_s, false);
                continue;
            }
            const auto rect = label->mapRectToScene(label->boundingRect());
            check(u"label-inside-viewport"_s,
                  rect.left() >= -RetainedLeftSlackPixels
                          && rect.right() + EntryWidthPixels
                                  <= state.window.width() - LabelRightInsetPixels);
            for (const auto &other : occupied)
                check(u"labels-do-not-overlap"_s, !rect.intersects(other));
            occupied.append(rect);
        }
        capture("size-"_L1 + QString::number(state.sizeIndex));
        if (++state.sizeIndex < state.sizes.size())
            state.stage = ResizeViewport;
        else {
            state.window.resize(yalahoo::review::WidthPixels, yalahoo::review::HeightPixels);
            ++state.stage;
        }
        return;
    }
}
void advanceOverviewExitAfterResize(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;

    if (!ready())
        return;
    state.overview.setActive(false);
    state.stage = AwaitOverviewRelease;
}
void advanceOverviewAwaitOverviewRelease(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;

    if (!ready() || state.overview.progress() != 0)
        return;
    state.overview.setActive(true);
    state.frames = 0;
    ++state.stage;
    return;
}
void advanceOverviewAwaitShortcutExit(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &key = context.key;

    if (!ready() || state.frames < ShortcutSettleFrames)
        return;
    key(Qt::Key_V);
    check(u"v-return"_s, !state.overview.active());
    ++state.stage;
    return;
}
void advanceOverviewAwaitLabelRelease(auto &context)
{
    OverviewReviewState &state = context.state;
    auto &ready = context.ready;
    auto &check = context.check;
    auto &finish = context.finish;

    if (!ready() || state.overview.labels()->liveCount() != 0)
        return;
    check(u"labels-release"_s, state.overview.labels()->liveCount() == 0);
    check(u"archive-no-detail"_s,
          state.presentation.archiveEnabled() && !state.presentation.wantsDetail());
    ++state.stage;
    finish();
    return;
}
void advanceOverviewDefault(auto &)
{

    return;
}

auto advanceOverviewReview(OverviewReviewState &state, auto &check, auto &ready, auto &button,
                           auto &firstLane, auto &capture, auto &key, auto &clickPoint,
                           auto &finish)
{
    if (state.finished)
        return;
    // The finite probe also advances through static reduced-motion states.
    // Request its next actual Qt frame without adding a product clock or timer.
    const auto nextFrame = qScopeGuard([&] {
        if (!state.finished && state.window.isVisible())
            state.window.update();
    });
    ++state.frames;
    state.states.append(QJsonObject{ { u"stage"_s, state.stage },
                                     { u"progress"_s, state.overview.progress() },
                                     { u"album"_s, state.selection->albumId() },
                                     { u"labels"_s, state.overview.labels()->liveCount() },
                                     { u"frameRevision"_s, qint64(state.scene.frameRevision()) } });
    state.peakLabels = std::max(state.peakLabels, state.overview.labels()->liveCount());
    if (state.overview.labels()->liveCount() > MaximumLiveLabels)
        check(u"label-capacity"_s, false);
    {
        OverviewStageContext context{ state,   ready, button,     check, firstLane,
                                      capture, key,   clickPoint, finish };
        static constexpr review::StageRoute<decltype(context)> routes[] = {
            { AwaitInitialFrame, &advanceOverviewAwaitInitialFrame },
            { AwaitOverview, &advanceOverviewAwaitOverview },
            { AwaitExpandedEntry, &advanceOverviewAwaitExpandedEntry },
            { AwaitBrowseFocus, &advanceOverviewAwaitBrowseFocus },
            { AwaitReenteredOverview, &advanceOverviewAwaitReenteredOverview },
            { AwaitColumnNavigation, &advanceOverviewAwaitColumnNavigation },
            { AwaitEscape, &advanceOverviewAwaitEscape },
            { ClickArchiveCase, &advanceOverviewClickArchiveCase },
            { AwaitCaseReturn, &advanceOverviewAwaitCaseReturn },
            { ReverseOverview, &advanceOverviewReverseOverview },
            { ReverseAgain, &advanceOverviewReverseAgain },
            { ReduceOverviewMotion, &advanceOverviewReduceOverviewMotion },
            { ResizeViewport, &advanceOverviewResizeViewport },
            { CheckViewport, &advanceOverviewCheckViewport },
            { ExitAfterResize, &advanceOverviewExitAfterResize },
            { AwaitOverviewRelease, &advanceOverviewAwaitOverviewRelease },
            { AwaitShortcutExit, &advanceOverviewAwaitShortcutExit },
            { AwaitLabelRelease, &advanceOverviewAwaitLabelRelease }
        };
        const int stageKey = state.stage;
        if (!review::dispatchStage(routes, stageKey, context))
            advanceOverviewDefault(context);
    }
}
}

int reviewOverview(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                   OverviewController &overview, PresentationController &presentation,
                   LibrarySession &library, MotionController &motion, const QString &output)
{
    auto *native = qobject_cast<rhi::SceneItem *>(
            yalahoo::review::findVisual(window.contentItem(), u"nativeScene"_s));
    auto *selection = scene.selection();
    if (!native || !selection)
        return InvalidReviewInput;
    QDir().mkpath(output);
    native->setTelemetryEnabled(true);
    window.setPersistentGraphics(false);
    window.setPersistentSceneGraph(false);
    window.resize(yalahoo::review::WidthPixels, yalahoo::review::HeightPixels);
    const bool initialNight = qEnvironmentVariableIntValue("YALAHOO_VERIFY_NIGHT") == 1;
    scene.lighting()->setNight(initialNight);
    window.setProperty("reviewPlayingTitle",
                       u"\u6b63\u5728\u64ad\u653e \u00b7 \u56fa\u5b9a\u4f4d\u7f6e"_s);
    const QString playback = library.playbackTrackId();
    QObject lifetime;
    QJsonArray checks, samples, states, pictures;
    QStringList warnings;
    QElapsedTimer time;
    time.start();
    int stage = AwaitInitialFrame, frames = 0, sizeIndex = 0, peakLabels = 0;
    int visibleLane = -1;
    QString selected;
    QPointF playPosition, stopPosition;
    QMutex positionsMutex;
    QPointF synchronizedPlay, synchronizedStop;
    bool passed = true, finished = false;
    const QList<QSize> sizes{ { 1920, 1080 },
                              { yalahoo::review::WidthPixels, yalahoo::review::HeightPixels },
                              { 995, 858 },
                              { 960, 900 },
                              { 930, 900 },
                              { 390, 844 },
                              { 320, yalahoo::review::HeightPixels },
                              { 2560, 1080 },
                              { 3440, 1440 } };
    const auto check = [&](const QString &name, bool result) {
        passed &= result;
        checks.append(
                QJsonObject{ { u"name"_s, name }, { u"passed"_s, result }, { u"stage"_s, stage } });
    };
    const auto capture = [&](const QString &name) {
        const auto image = window.grabWindow();
        check("capture-"_L1 + name,
              !image.isNull() && image.save(output + '/'_L1 + name + ".png"_L1));
        QJsonArray anchors;
        for (const auto &c : overview.projectColumns())
            anchors.append(QJsonObject{ { u"lane"_s, c.lane },
                                        { u"name"_s, c.name },
                                        { u"x"_s, c.x },
                                        { u"y"_s, c.y },
                                        { u"count"_s, c.count },
                                        { u"extent"_s, c.extent },
                                        { u"selected"_s, c.selected } });
        pictures.append(QJsonObject{ { u"name"_s, name },
                                     { u"width"_s, window.width() },
                                     { u"height"_s, window.height() },
                                     { u"night"_s, scene.lighting()->night() },
                                     { u"realistic"_s, selection->realistic() },
                                     { u"anchors"_s, anchors } });
    };
    const auto key = [&](int code) {
        QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &event);
    };
    const auto clickPoint = [&](QPointF point) {
        QMouseEvent press(QEvent::MouseButtonPress, point, window.mapToGlobal(point),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, window.mapToGlobal(point),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &press);
        QCoreApplication::sendEvent(&window, &release);
    };
    const auto button = [&](const QString &name) {
        auto *item = yalahoo::review::findVisual(window.contentItem(), name);
        if (!item || !item->isVisible() || !item->isEnabled()) {
            check("button-"_L1 + name, false);
            return;
        }
        clickPoint(item->mapToScene({ item->width() / 2, item->height() / 2 }));
    };
    const auto ready = [&] {
        const auto cache = native->coverStatistics()[u"cache"_s].toObject();
        return native->errorText().isEmpty() && native->submission() > 0 && scene.settled()
                && selection->archiveReady() && cache[u"pending"_s].toInt() == 0
                && cache[u"active"_s].toInt() == 0;
    };
    const auto firstLane = [&] {
        for (int row = 0; row < overview.labels()->rowCount(); ++row) {
            const auto index = overview.labels()->index(row);
            if (overview.labels()->data(index, OverviewLabelsModel::Interactive).toBool())
                return overview.labels()->data(index, OverviewLabelsModel::Lane).toInt();
        }
        return -1;
    };
    OverviewReviewState state{ finished,     stage,
                               playback,     library,
                               warnings,     window,
                               native,       samples,
                               passed,       checks,
                               pictures,     states,
                               peakLabels,   time,
                               motion,       scene,
                               selection,    output,
                               app,          frames,
                               overview,     positionsMutex,
                               playPosition, synchronizedPlay,
                               stopPosition, synchronizedStop,
                               selected,     visibleLane,
                               presentation, sizes,
                               sizeIndex,    initialNight };
    const auto finish = [&] {
        return finishOverviewReview(state, check);
    };
    QObject::connect(qmlEngine(&window), &QQmlEngine::warnings, &lifetime,
                     [&](const QList<QQmlError> &errors) {
                         for (const auto &error : errors)
                             warnings.append(error.toString());
                     });
    QObject::connect(native, &rhi::SceneItem::frameSubmitted, &lifetime, [&](QJsonObject sample) {
        sample[u"stage"_s] = stage;
        samples.append(sample);
    });
    // GUI is blocked during sync: measure the exact polished row consumed by this frame.
    const auto positions = QObject::connect(
            &window, &QQuickWindow::afterSynchronizing, &window,
            [&] {
                auto *play = yalahoo::review::findVisual(window.contentItem(), u"playButton"_s);
                auto *stop = yalahoo::review::findVisual(window.contentItem(), u"stopButton"_s);
                if (play && stop) {
                    QMutexLocker lock(&positionsMutex);
                    synchronizedPlay = play->mapToScene({ 0, 0 });
                    synchronizedStop = stop->mapToScene({ 0, 0 });
                }
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::frameSwapped, &lifetime,
            [&] {
                return advanceOverviewReview(state, check, ready, button, firstLane, capture, key,
                                             clickPoint, finish);
            },
            Qt::QueuedConnection);
    QTimer::singleShot(ReviewDeadlineMilliseconds, &lifetime, finish);
    window.show();
    const int result = app.exec();
    QObject::disconnect(positions);
    native->setTelemetryEnabled(false);
    return result;
}
}
