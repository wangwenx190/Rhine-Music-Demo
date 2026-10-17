#include "NavigationReview.h"
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
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <functional>
#include <chrono>

using namespace Qt::StringLiterals;

namespace {
QQuickItem *find(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems())
        if (auto *result = find(child, name))
            return result;
    return nullptr;
}
}
int yalahoo::reviewNavigation(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                              LibrarySession &library, InputRouter &input, RulerController &ruler,
                              const QString &output)
{
    QDir().mkpath(output);
    auto *selection = scene.selection();
    auto *native = qobject_cast<rhi::SceneItem *>(find(window.contentItem(), u"nativeScene"_s));
    if (!selection || !native || !find(window.contentItem(), u"productionRulerInk"_s))
        return 4;
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
    window.resize(1280, 720);
    native->setTelemetryEnabled(true);
    const auto playback = library.playbackTrackId();
    bool passed = true;
    QJsonArray samples, checks, inputs;
    int step = 0;
    QObject scope;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool waiting = false, queued = false;
    qint64 restoreStartedNs = 0;
    std::function<void()> next;
    std::function<void()> advanceWhenReady;
    const auto now = [] {
        return qint64(std::chrono::duration_cast<std::chrono::nanoseconds>(
                              std::chrono::steady_clock::now().time_since_epoch())
                              .count());
    };
    QObject::connect(&input, &InputRouter::navigationCommitted, &scope, [&](quint64 revision) {
        inputs.append(QJsonObject{ { u"navigationRevision"_s, qint64(revision) },
                                   { u"inputRevision"_s, qint64(scene.frame().inputRevision) },
                                   { u"inputReceivedNs"_s, scene.frame().inputReceivedNs },
                                   { u"navigationCommittedNs"_s, now() } });
    });
    QObject::connect(native, &rhi::SceneItem::frameSubmitted, &scope, [&](QJsonObject sample) {
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
        const auto frame = scene.frame();
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
        QSaveFile file(output + "/report.json"_L1);
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(7);
            return;
        }
        file.write(
                QJsonDocument(
                        QJsonObject{
                                { u"passed"_s, passed },
                                { u"checks"_s, checks },
                                { u"inputs"_s, inputs },
                                { u"frames"_s, samples },
                                { u"windowPolicy"_s, verification::windowPolicy(window) },
                                { u"coverStatistics"_s, native->coverStatistics() },
                                { u"scope"_s,
                                  u"Actual Release App path. Event-driven input receipt, navigation commit and corresponding Qt frame submission are distinct; screen display time is unavailable."_s } })
                        .toJson());
        app.exit(file.commit() && passed ? 0 : 8);
    };
    advanceWhenReady = [&] {
        if (!waiting || queued)
            return;
        const bool ready = step == 0 ? window.isExposed() && !samples.isEmpty()
                : step == 7          ? !window.isVisible() && !ruler.active()
                : step == 8
                ? window.isExposed() && ruler.active() && !samples.isEmpty()
                        && samples.last().toObject()[u"submittedNs"_s].toInteger()
                                >= restoreStartedNs
                : hasSubmission() && (step != 4 || !ruler.state()[u"moving"_s].toBool());
        if (!ready)
            return;
        waiting = false;
        queued = true;
        deadline.stop();
        QMetaObject::invokeMethod(
                &scope,
                [&] {
                    queued = false;
                    next();
                },
                Qt::QueuedConnection);
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
        switch (step++) {
        case 0:
            for (int i = 0; i < 4; ++i)
                wheel(-12, -120);
            break;
        case 1:
            check(u"pixelDelta-priority-48px"_s, selection->albumId() == "a0-1"_L1);
            checkSubmission(u"pixelDelta-corresponding-submission"_s);
            for (int i = 0; i < 8; ++i)
                key(Qt::Key_Down);
            for (int i = 0; i < 5; ++i)
                key(Qt::Key_Up);
            break;
        case 2:
            check(u"rapid-reversal-latest-target"_s, selection->albumId() == "a0-4"_L1);
            checkSubmission(u"reversal-corresponding-submission"_s);
            key(Qt::Key_Right);
            break;
        case 3: {
            check(u"keyboard-column"_s, selection->cell().lane == 1);
            checkSubmission(u"column-corresponding-submission"_s);
            const auto before = selection->albumId();
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData("import QtQuick\nTextInput {width: 100; height: 40; text: 'x'}"_ba,
                              QUrl());
            std::unique_ptr<QObject> object(component.create());
            auto *text = qobject_cast<QQuickItem *>(object.get());
            if (!text) {
                app.exit(5);
                return;
            }
            text->setParentItem(window.contentItem());
            text->forceActiveFocus();
            key(Qt::Key_Down);
            check(u"input-focus-owned"_s, selection->albumId() == before);
            object.reset();
            selection->setRealistic(true);
            selection->navigateRow(12);
            wheel(0, -1200);
            break;
        }
        case 4: {
            check(u"wheel-event-cap-and-finite-end"_s, selection->cell().row == 15);
            checkSubmission(u"finite-end-corresponding-submission"_s);
            int first = -1;
            const auto states = ruler.slotStates();
            for (int i = 0; i < 16; ++i)
                if (states[i].toMap()[u"enabled"_s].toBool()
                    && states[i].toMap()[u"ordinal"_s].toInt() == 0)
                    first = i;
            auto *button = find(window.contentItem(), u"rulerSlot%1"_s.arg(first));
            if (!button) {
                app.exit(6);
                return;
            }
            const auto point = button->mapToScene({ button->width() / 2, button->height() / 2 });
            QMouseEvent press(QEvent::MouseButtonPress, point, window.mapToGlobal(point),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, point, window.mapToGlobal(point),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &press);
            QCoreApplication::sendEvent(&window, &release);
            break;
        }
        case 5:
            check(u"production-ruler-click"_s, selection->cell().row == 12);
            checkSubmission(u"ruler-click-corresponding-submission"_s);
            input.swipe({ 0, -36 }, 1400);
            window.resize(390, 844);
            scene.lighting()->setNight(true);
            break;
        case 6:
            check(u"touch-common-boundary"_s, selection->cell().row == 13);
            checkSubmission(u"touch-corresponding-submission"_s);
            check(u"portrait-narrow"_s, ruler.narrow() && scene.layout()->portrait());
            window.grabWindow().save(output + "/portrait-night.png"_L1);
            window.hide();
            break;
        case 7:
            check(u"hidden-ruler-no-input"_s, !ruler.active());
            window.releaseResources();
            restoreStartedNs = now();
            window.show();
            break;
        case 8: {
            check(u"restore-production-ruler"_s, ruler.active() && ruler.narrow());
            check(u"navigation-playback-independent"_s, library.playbackTrackId() == playback);
            check(u"window-policy"_s, verification::windowPolicySatisfied(window));
            check(u"real-input-frame-submissions"_s, !samples.isEmpty() && !inputs.isEmpty());
            const auto image = window.grabWindow();
            check(u"own-render-capture"_s,
                  !image.isNull() && image.save(output + "/restored.png"_L1));
            finish();
            return;
        }
        }
        waiting = true;
        deadline.start(10000);
        window.update();
        advanceWhenReady();
    };
    waiting = true;
    deadline.start(10000);
    window.show();
    window.update();
    return app.exec();
}
