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
    auto *native = qobject_cast<rhi::SceneItem *>(find(window.contentItem(), "nativeScene"));
    if (!selection || !native || !find(window.contentItem(), "productionRulerInk"))
        return 4;
    LibrarySnapshot fixture;
    for (int lane = 0; lane < 2; ++lane) {
        Genre genre;
        genre.id = QString("g%1").arg(lane);
        genre.name = QString("测试列 %1").arg(lane + 1);
        fixture.genres.append(genre);
        for (int row = 0; row < (lane == 0 ? 37 : 4); ++row) {
            Album album;
            album.id = QString("a%1-%2").arg(lane).arg(row);
            album.title = QString("专辑 %1").arg(row + 1);
            album.genreId = genre.id;
            album.coverUrl = QUrl("qrc:/assets/demo-covers/square.png");
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
        inputs.append(QJsonObject{ { "navigationRevision", qint64(revision) },
                                   { "inputRevision", qint64(scene.frame().inputRevision) },
                                   { "inputReceivedNs", scene.frame().inputReceivedNs },
                                   { "navigationCommittedNs", now() } });
    });
    QObject::connect(native, &rhi::SceneItem::frameSubmitted, &scope, [&](QJsonObject sample) {
        samples.append(sample);
        advanceWhenReady();
    });
    const auto check = [&](const QString &name, bool value) {
        passed &= value;
        checks.append(QJsonObject{ { "name", name },
                                   { "passed", value },
                                   { "albumId", selection->albumId() },
                                   { "rulerSelected", ruler.state()["selected"].toDouble() } });
    };
    const auto hasSubmission = [&] {
        const auto frame = scene.frame();
        for (const auto &value : samples) {
            const auto sample = value.toObject();
            if (sample["navigationRevision"].toInteger() > 0
                && sample["inputRevision"].toInteger() == qint64(frame.inputRevision)
                && sample["navigationInputRevision"].toInteger() == qint64(frame.inputRevision)
                && sample["navigationCommittedNs"].toInteger()
                        >= sample["inputReceivedNs"].toInteger()
                && sample["submittedNs"].toInteger()
                        >= sample["navigationCommittedNs"].toInteger()) {
                return frame.inputRevision > 0;
            }
        }
        return false;
    };
    const auto checkSubmission = [&](const QString &name) {
        check(name, hasSubmission());
    };
    const auto finish = [&] {
        QSaveFile file(output + "/report.json");
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(7);
            return;
        }
        file.write(
                QJsonDocument(
                        QJsonObject{
                                { "passed", passed },
                                { "checks", checks },
                                { "inputs", inputs },
                                { "frames", samples },
                                { "windowPolicy", verification::windowPolicy(window) },
                                { "coverStatistics", native->coverStatistics() },
                                { "scope",
                                  "Actual Release App path. Event-driven input receipt, navigation commit and corresponding Qt frame submission are distinct; screen display time is unavailable." } })
                        .toJson());
        app.exit(file.commit() && passed ? 0 : 8);
    };
    advanceWhenReady = [&] {
        if (!waiting || queued)
            return;
        const bool ready = step == 0 ? window.isExposed() && !samples.isEmpty()
                : step == 7          ? !window.isVisible() && !ruler.active()
                : step == 8          ? window.isExposed() && ruler.active() && !samples.isEmpty()
                        && samples.last().toObject()["submittedNs"].toInteger() >= restoreStartedNs
                            : hasSubmission() && (step != 4 || !ruler.state()["moving"].toBool());
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
        check("native-window-or-submission-deadline", false);
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
            check("pixelDelta-priority-48px", selection->albumId() == "a0-1");
            checkSubmission("pixelDelta-corresponding-submission");
            for (int i = 0; i < 8; ++i)
                key(Qt::Key_Down);
            for (int i = 0; i < 5; ++i)
                key(Qt::Key_Up);
            break;
        case 2:
            check("rapid-reversal-latest-target", selection->albumId() == "a0-4");
            checkSubmission("reversal-corresponding-submission");
            key(Qt::Key_Right);
            break;
        case 3: {
            check("keyboard-column", selection->cell().lane == 1);
            checkSubmission("column-corresponding-submission");
            const auto before = selection->albumId();
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData("import QtQuick\nTextInput {width: 100; height: 40; text: 'x'}",
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
            check("input-focus-owned", selection->albumId() == before);
            object.reset();
            selection->setRealistic(true);
            selection->navigateRow(12);
            wheel(0, -1200);
            break;
        }
        case 4: {
            check("wheel-event-cap-and-finite-end", selection->cell().row == 15);
            checkSubmission("finite-end-corresponding-submission");
            int first = -1;
            const auto states = ruler.slotStates();
            for (int i = 0; i < 16; ++i)
                if (states[i].toMap()["enabled"].toBool()
                    && states[i].toMap()["ordinal"].toInt() == 0)
                    first = i;
            auto *button = find(window.contentItem(), QString("rulerSlot%1").arg(first));
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
            check("production-ruler-click", selection->cell().row == 12);
            checkSubmission("ruler-click-corresponding-submission");
            input.swipe({ 0, -36 }, 1400);
            window.resize(390, 844);
            scene.lighting()->setNight(true);
            break;
        case 6:
            check("touch-common-boundary", selection->cell().row == 13);
            checkSubmission("touch-corresponding-submission");
            check("portrait-narrow", ruler.narrow() && scene.layout()->portrait());
            window.grabWindow().save(output + "/portrait-night.png");
            window.hide();
            break;
        case 7:
            check("hidden-ruler-no-input", !ruler.active());
            window.releaseResources();
            restoreStartedNs = now();
            window.show();
            break;
        case 8: {
            check("restore-production-ruler", ruler.active() && ruler.narrow());
            check("navigation-playback-independent", library.playbackTrackId() == playback);
            check("window-policy", verification::windowPolicySatisfied(window));
            check("real-input-frame-submissions", !samples.isEmpty() && !inputs.isEmpty());
            const auto image = window.grabWindow();
            check("own-render-capture", !image.isNull() && image.save(output + "/restored.png"));
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
