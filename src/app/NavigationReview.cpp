#include "NavigationReview.h"
#include "SelectionController.h"
#include "SelectionLighting.h"
#include "LayoutMetrics.h"
#include "MonotonicTime.h"
#include <algorithm>
#include <iterator>
#include "ReviewSupport.h"
#include <QWheelEvent>
#include "InputRouter.h"
#include "RulerController.h"
#include "SceneAdapter.h"
#include "SceneItem.h"
#include "LibrarySession.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDir>
#include "JsonFile.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <functional>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Stage deadlines bound event-driven review completion; injected inputs target only this window.
constexpr auto StageDeadlineMilliseconds = 10000;
constexpr auto SwipeDeadlineMilliseconds = 1400;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 4,
    RulerObjectMissing = 5,
    RulerSlotsMissing = 6,
    ReportFailed = 8
};

// Ordered review states retain their report ordinals; names describe each awaited fact.
enum ReviewStage {
    AwaitInitialFrame = 0,
    CheckWheelBurst = 1,
    CheckOpposedWheel = 2,
    CheckFiniteColumn = 3,
    CheckWheelCap = 4,
    CheckRulerClick = 5,
    CheckSwipe = 6,
    CheckHidden = 7,
    CheckRestored = 8
};

// Shared references remain valid until this review event loop has stopped.
struct NavigationReviewState final
{
    bool &waiting;
    bool &queued;
    int &step;
    QQuickWindow &window;
    QJsonArray &samples;
    RulerController &ruler;
    qint64 &restoreStartedNs;
    QTimer &deadline;
    QObject &scope;
    std::function<void()> &next;
    SelectionController *&selection;
    QGuiApplication &app;
    InputRouter &input;
    SceneAdapter &scene;
    const QString &output;
    qint64 (*const &now)();
    LibrarySession &library;
    const QString &playback;
    QJsonArray &inputs;
    std::function<void()> &advanceWhenReady;
};
auto finishNavigationReview(NavigationReviewState &state, auto &hasSubmission)
{
    if (!state.waiting || state.queued)
        return;
    const bool ready = state.step == AwaitInitialFrame
            ? state.window.isExposed() && !state.samples.isEmpty()
            : state.step == CheckHidden ? !state.window.isVisible() && !state.ruler.active()
            : state.step == CheckRestored
            ? state.window.isExposed() && state.ruler.active() && !state.samples.isEmpty()
                    && state.samples.last().toObject()[u"submittedNs"_s].toInteger()
                            >= state.restoreStartedNs
            : hasSubmission()
                    && (state.step != CheckWheelCap || !state.ruler.state()[u"moving"_s].toBool());
    if (!ready)
        return;
    state.waiting = false;
    state.queued = true;
    state.deadline.stop();
    QMetaObject::invokeMethod(
            &state.scope,
            [&] {
                state.queued = false;
                state.next();
            },
            Qt::QueuedConnection);
}

template <typename WheelFunction, typename CheckFunction, typename CheckSubmissionFunction,
          typename KeyFunction, typename FinishFunction>
struct NavigationStageContext final
{
    NavigationReviewState &state;
    WheelFunction &wheel;
    CheckFunction &check;
    CheckSubmissionFunction &checkSubmission;
    KeyFunction &key;
    FinishFunction &finish;
};
void advanceNavigationAwaitInitialFrame(auto &context)
{
    auto &wheel = context.wheel;

    for (int i = 0; i < 4; ++i)
        wheel(-12, -120);
    return;
}
void advanceNavigationCheckWheelBurst(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;
    auto &checkSubmission = context.checkSubmission;
    auto &key = context.key;

    check(u"pixelDelta-priority-48px"_s, state.selection->albumId() == "a0-1"_L1);
    checkSubmission(u"pixelDelta-corresponding-submission"_s);
    for (int i = 0; i < 8; ++i)
        key(Qt::Key_Down);
    for (int i = 0; i < 5; ++i)
        key(Qt::Key_Up);
    return;
}
void advanceNavigationCheckOpposedWheel(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;
    auto &checkSubmission = context.checkSubmission;
    auto &key = context.key;

    check(u"rapid-reversal-latest-target"_s, state.selection->albumId() == "a0-4"_L1);
    checkSubmission(u"reversal-corresponding-submission"_s);
    key(Qt::Key_Right);
    return;
}
void advanceNavigationCheckFiniteColumn(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &wheel = context.wheel;
    auto &check = context.check;
    auto &checkSubmission = context.checkSubmission;
    auto &key = context.key;
    {
        check(u"keyboard-column"_s, state.selection->cell().lane == 1);
        checkSubmission(u"column-corresponding-submission"_s);
        const auto before = state.selection->albumId();
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nTextInput {width: 100; height: 40; text: 'x'}"_ba,
                          QUrl());
        std::unique_ptr<QObject> object(component.create());
        auto *text = qobject_cast<QQuickItem *>(object.get());
        if (!text) {
            state.app.exit(RulerObjectMissing);
            return;
        }
        text->setParentItem(state.window.contentItem());
        text->forceActiveFocus();
        key(Qt::Key_Down);
        check(u"input-focus-owned"_s, state.selection->albumId() == before);
        object.reset();
        state.selection->setRealistic(true);
        state.selection->navigateRow(12);
        wheel(0, -1200);
        return;
    }
}
void advanceNavigationCheckWheelCap(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;
    auto &checkSubmission = context.checkSubmission;
    {
        check(u"wheel-event-cap-and-finite-end"_s, state.selection->cell().row == 15);
        checkSubmission(u"finite-end-corresponding-submission"_s);
        int first = -1;
        const auto states = state.ruler.slotStates();
        for (int i = 0; i < 16; ++i)
            if (states[i].toMap()[u"enabled"_s].toBool()
                && states[i].toMap()[u"ordinal"_s].toInt() == 0)
                first = i;
        auto *button = yalahoo::review::findVisual(state.window.contentItem(),
                                                   u"rulerSlot%1"_s.arg(first));
        if (!button) {
            state.app.exit(RulerSlotsMissing);
            return;
        }
        const auto point = button->mapToScene({ button->width() / 2, button->height() / 2 });
        QMouseEvent press(QEvent::MouseButtonPress, point, state.window.mapToGlobal(point),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, state.window.mapToGlobal(point),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&state.window, &press);
        QCoreApplication::sendEvent(&state.window, &release);
        return;
    }
}
void advanceNavigationCheckRulerClick(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;
    auto &checkSubmission = context.checkSubmission;

    check(u"production-ruler-click"_s, state.selection->cell().row == 12);
    checkSubmission(u"ruler-click-corresponding-submission"_s);
    state.input.swipe({ 0, -36 }, SwipeDeadlineMilliseconds);
    state.window.resize(390, 844);
    state.scene.lighting()->setNight(true);
    return;
}
void advanceNavigationCheckSwipe(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;
    auto &checkSubmission = context.checkSubmission;

    check(u"touch-common-boundary"_s, state.selection->cell().row == 13);
    checkSubmission(u"touch-corresponding-submission"_s);
    check(u"portrait-narrow"_s, state.ruler.narrow() && state.scene.layout()->portrait());
    const auto image = state.window.grabWindow();
    check(u"portrait-night-output"_s,
          !image.isNull() && image.save(state.output + "/portrait-night.png"_L1));
    state.window.hide();
    return;
}
void advanceNavigationCheckHidden(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;

    check(u"hidden-ruler-no-input"_s, !state.ruler.active());
    state.window.releaseResources();
    state.restoreStartedNs = state.now();
    state.window.show();
    return;
}
void advanceNavigationCheckRestored(auto &context)
{
    NavigationReviewState &state = context.state;
    auto &check = context.check;
    auto &finish = context.finish;
    {
        check(u"restore-production-ruler"_s, state.ruler.active() && state.ruler.narrow());
        check(u"navigation-playback-independent"_s,
              state.library.playbackTrackId() == state.playback);
        check(u"window-policy"_s, verification::windowPolicySatisfied(state.window));
        check(u"real-input-frame-submissions"_s,
              !state.samples.isEmpty() && !state.inputs.isEmpty());
        const auto image = state.window.grabWindow();
        check(u"own-render-capture"_s,
              !image.isNull() && image.save(state.output + "/restored.png"_L1));
        finish();
        return;
    }
}

auto advanceNavigationReview(NavigationReviewState &state, auto &wheel, auto &check,
                             auto &checkSubmission, auto &key, auto &finish)
{
    {
        NavigationStageContext context{ state, wheel, check, checkSubmission, key, finish };
        static constexpr review::StageRoute<decltype(context)> routes[] = {
            { AwaitInitialFrame, &advanceNavigationAwaitInitialFrame },
            { CheckWheelBurst, &advanceNavigationCheckWheelBurst },
            { CheckOpposedWheel, &advanceNavigationCheckOpposedWheel },
            { CheckFiniteColumn, &advanceNavigationCheckFiniteColumn },
            { CheckWheelCap, &advanceNavigationCheckWheelCap },
            { CheckRulerClick, &advanceNavigationCheckRulerClick },
            { CheckSwipe, &advanceNavigationCheckSwipe },
            { CheckHidden, &advanceNavigationCheckHidden },
            { CheckRestored, &advanceNavigationCheckRestored }
        };
        const int stageKey = state.step++;
        review::dispatchStage(routes, stageKey, context);
    }
    state.waiting = true;
    state.deadline.start(StageDeadlineMilliseconds);
    state.window.update();
    state.advanceWhenReady();
}
}

int reviewNavigation(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                     LibrarySession &library, InputRouter &input, RulerController &ruler,
                     const QString &output)
{
    QDir().mkpath(output);
    auto *selection = scene.selection();
    auto *native = qobject_cast<rhi::SceneItem *>(
            yalahoo::review::findVisual(window.contentItem(), u"nativeScene"_s));
    if (!selection || !native
        || !yalahoo::review::findVisual(window.contentItem(), u"productionRulerInk"_s))
        return InvalidReviewInput;
    LibrarySnapshot fixture;
    for (int lane = 0; lane < 2; ++lane) {
        Genre genre;
        genre.id = u"g%1"_s.arg(lane);
        genre.name = u"\u6d4b\u8bd5\u5217 %1"_s.arg(lane + 1);
        fixture.genres.append(genre);
        for (int row = 0; row < (lane == 0 ? 37 : 4); ++row) {
            Album album;
            album.id = u"a%1-%2"_s.arg(lane).arg(row);
            album.title = u"\u4e13\u8f91 %1"_s.arg(row + 1);
            album.genreId = genre.id;
            album.coverUrl = QUrl(u"qrc:/assets/demo-covers/square.png"_s);
            fixture.albums.append(album);
        }
    }
    library.setSnapshot(std::move(fixture));
    selection->setRealistic(false);
    window.resize(yalahoo::review::WidthPixels, yalahoo::review::HeightPixels);
    native->setTelemetryEnabled(true);
    const auto playback = library.playbackTrackId();
    bool passed = true;
    QJsonArray samples, checks, inputs;
    int step = AwaitInitialFrame;
    QObject scope;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool waiting = true, queued = false;
    qint64 restoreStartedNs = 0;
    std::function<void()> next;
    std::function<void()> advanceWhenReady;
    const auto now = yalahoo::monotonicNanoseconds;
    QObject::connect(&input, &InputRouter::navigationCommitted, &scope, [&](quint64 revision) {
        inputs.append(QJsonObject{ { u"navigationRevision"_s, qint64(revision) },
                                   { u"inputRevision"_s, qint64(scene.frame().inputRevision) },
                                   { u"inputReceivedNs"_s, scene.frame().inputReceivedNs },
                                   { u"navigationCommittedNs"_s, now() } });
    });
    QObject::connect(native, &rhi::SceneItem::frameSubmitted, &scope,
                     [&](const QJsonObject &sample) {
                         samples.append(sample);
                         advanceWhenReady();
                     });
    const auto check = [&](const QString &name, bool value) {
        passed &= value;
        checks.append(
                QJsonObject{ { u"name"_s, name },
                             { u"passed"_s, value },
                             { u"albumId"_s, selection->albumId() },
                             { u"rulerSelected"_s, ruler.state()[u"selected"_s].toDouble() } });
    };
    const auto hasSubmission = [&] {
        const auto &frame = scene.frame();
        for (const auto &value : samples) {
            const auto sample = value.toObject();
            if (sample[u"navigationRevision"_s].toInteger() > 0
                && sample[u"inputRevision"_s].toInteger() == qint64(frame.inputRevision)
                && sample[u"navigationInputRevision"_s].toInteger() == qint64(frame.inputRevision)
                && sample[u"navigationCommittedNs"_s].toInteger()
                        >= sample[u"inputReceivedNs"_s].toInteger()
                && sample[u"submittedNs"_s].toInteger()
                        >= sample[u"navigationCommittedNs"_s].toInteger()) {
                return frame.inputRevision > 0;
            }
        }
        return false;
    };
    const auto checkSubmission = [&](const QString &name) {
        check(name, hasSubmission());
    };
    const auto finish = [&] {
        const bool saved = yalahoo::writeJsonFile(
                output + "/report.json"_L1,
                QJsonDocument(QJsonObject{
                        { u"passed"_s, passed },
                        { u"checks"_s, checks },
                        { u"inputs"_s, inputs },
                        { u"frames"_s, samples },
                        { u"windowPolicy"_s, verification::windowPolicy(window) },
                        { u"coverStatistics"_s, native->coverStatistics() },
                        { u"scope"_s,
                          u"Actual Release App path. Event-driven input receipt, navigation commit and corresponding Qt frame submission are distinct; screen display time is unavailable."_s } }));
        app.exit(saved && passed ? 0 : ReportFailed);
    };
    NavigationReviewState state{
        waiting,  queued, step,    window,    samples, ruler,           restoreStartedNs,
        deadline, scope,  next,    selection, app,     input,           scene,
        output,   now,    library, playback,  inputs,  advanceWhenReady
    };
    advanceWhenReady = [&] {
        return finishNavigationReview(state, hasSubmission);
    };
    QObject::connect(&window, &QQuickWindow::frameSwapped, &scope, advanceWhenReady);
    QObject::connect(&window, &QWindow::visibleChanged, &scope, advanceWhenReady);
    QObject::connect(&deadline, &QTimer::timeout, &scope, [&] {
        check(u"native-window-or-submission-deadline"_s, false);
        finish();
    });
    const auto key = [&](int code) {
        QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &event);
    };
    const auto wheel = [&](int pixels, int angle = 0) {
        QWheelEvent event({ 300, 180 }, window.mapToGlobal(QPointF(300, 180)), { 0, pixels },
                          { 0, angle }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &event);
    };
    next = [&] {
        return advanceNavigationReview(state, wheel, check, checkSubmission, key, finish);
    };
    deadline.start(StageDeadlineMilliseconds);
    window.show();
    window.update();
    return app.exec();
}
}
