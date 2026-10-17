#include "DetailReview.h"
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
#include <QSaveFile>
#include <QSet>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <cmath>
#include <functional>
#include <atomic>
#include <QThread>
#include <memory>

using namespace Qt::StringLiterals;

namespace {
QQuickItem *find(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems())
        if (auto *found = find(child, name))
            return found;
    return nullptr;
}
}
int yalahoo::reviewDetail(QGuiApplication &app, QQuickWindow &initialWindow, SceneAdapter &scene,
                          LibrarySession &library, PresentationController &presentation,
                          MotionController &motion, const QString &output)
{
    auto *window = &initialWindow;
    auto *engine = qobject_cast<QQmlApplicationEngine *>(qmlEngine(window));
    auto *native = qobject_cast<rhi::SceneItem *>(find(window->contentItem(), u"nativeScene"_s));
    auto *selection = scene.selection();
    if (!native || !selection || !engine)
        return 4;
    QDir().mkpath(output);
    window->setPersistentGraphics(false);
    window->setPersistentSceneGraph(false);
    window->resize(1280, 720);
    scene.lighting()->setNight(qEnvironmentVariableIntValue("YALAHOO_VERIFY_NIGHT") == 1);
    native->setTelemetryEnabled(true);
    const auto playback = library.playbackTrackId();
    QJsonArray checks, samples, states, latencies;
    QElapsedTimer time;
    time.start();
    int stage = 0, switches = 0, stress = 0, rebuilds = 0, contexts = 0, fullRebuilds = 0;
    const bool lifecycleStress = qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1;
    quint64 windowEpoch = 0;
    const bool quitOnClose = app.quitOnLastWindowClosed();
    app.setQuitOnLastWindowClosed(false);
    std::function<void()> bindWindow;
    qsizetype samplesBeforeRebuild = 0;
    QJsonArray lifecycle;
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
        auto *item = find(window->contentItem(), name);
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
                    && owner[u"resolution"_s].toInt() == 1024;
        }
        return selectedOwner && presentation.detailEnabled() && selection->canInspect()
                && cover[u"selectedDetailed"_s].toBool();
    };
    const auto finish = [&] {
        if (finished)
            return;
        finished = true;
        check(u"stage-completed"_s, stage == 17);
        if (lifecycleStress) {
            check(u"ten-full-qml-window-rebuilds"_s, fullRebuilds == 10);
            check(u"ten-product-context-restores"_s, rebuilds == 10 && contexts >= 11);
        } else {
            check(u"smoke-keeps-original-window"_s, fullRebuilds == 0 && rebuilds == 0);
        }
        check(u"render-error-empty"_s, native->errorText().isEmpty());
        check(u"qml-warnings-empty"_s, warnings.isEmpty());
        check(u"playback-independent"_s, library.playbackTrackId() == playback);
        check(u"window-policy"_s, verification::windowPolicySatisfied(*window));
        check(u"bounded-owners"_s, peakOwned <= 4 && peakReturning <= 3);
        check(u"normal-three-switches-and-reverse"_s, switches == 4);
        check(u"requested-additional-switches"_s, stress == (lifecycleStress ? 30 : 0));
        QSet<qint64> generations, postRevisions;
        qint64 peakUpload = 0, resourceBytes = 0;
        for (const auto &value : samples) {
            const auto sample = value.toObject();
            if (sample[u"stage"_s].toInt() < 12) {
                generations.insert(sample[u"resourceGeneration"_s].toInteger());
                postRevisions.insert(sample[u"postprocessRevision"_s].toInteger());
            }
            peakUpload = std::max(peakUpload, sample[u"atlasUploadBytes"_s].toInteger());
            resourceBytes = std::max(
                    resourceBytes,
                    sample[u"resources"_s].toObject()[u"estimatedCurrentBytes"_s].toInteger());
        }
        check(u"transforms-do-not-rebuild-targets"_s,
              generations.size() == 1 && postRevisions.size() == 1);
        check(u"bounded-upload-burst"_s, peakUpload <= 2 * 1024 * 1024);
        QJsonObject report{
            { u"passed"_s, passed },
            { u"configuration"_s, u"Release"_s },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"backend"_s,
              window->rendererInterface()->graphicsApi() == QSGRendererInterface::Metal ? u"metal"_s
                      : window->rendererInterface()->graphicsApi()
                              == QSGRendererInterface::Direct3D11
                      ? u"d3d11"_s
                      : u"opengl"_s },
            { u"renderLoop"_s, qEnvironmentVariable("QSG_RENDER_LOOP", u"Qt default"_s) },
            { u"separateRenderThreadObserved"_s, separateRenderThread->load() },
            { u"dpr"_s, window->effectiveDevicePixelRatio() },
            { u"windowPolicy"_s, verification::windowPolicy(*window) },
            { u"checks"_s, checks },
            { u"qmlWarnings"_s, QJsonArray::fromStringList(warnings) },
            { u"states"_s, states },
            { u"samples"_s, samples },
            { u"latencies"_s, latencies },
            { u"lifecycle"_s, lifecycle },
            { u"contexts"_s, contexts },
            { u"fullWindowRebuilds"_s, fullRebuilds },
            { u"lifecycleStressRequested"_s, lifecycleStress },
            { u"nativeInputSubmission"_s, qint64(native->submission()) },
            { u"nativeInputFrameRevision"_s, qint64(native->input().scene.revision) },
            { u"nativeItemVisible"_s, native->isVisible() },
            { u"peakOwned"_s, peakOwned },
            { u"peakAtlasUploadBytes"_s, peakUpload },
            { u"estimatedCurrentResourceMaxBytes"_s, resourceBytes },
            { u"peakReturning"_s, peakReturning },
            { u"finalCovers"_s, native->coverStatistics() },
            { u"durationMs"_s, time.elapsed() },
            { u"measurementScope"_s,
              u"Product Qt submission, readiness and resource telemetry; no display/photon timing or measured VRAM"_s }
        };
        QSaveFile file(output + "/report.json"_L1);
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(5);
            return;
        }
        file.write(QJsonDocument(report).toJson());
        app.exit(file.commit() && passed ? 0 : 6);
    };
    if (auto *engine = qmlEngine(window))
        QObject::connect(engine, &QQmlEngine::warnings, &reviewLifetime,
                         [&](const QList<QQmlError> &errors) {
                             for (const auto &error : errors)
                                 warnings.append(error.toString());
                         });
    bindWindow = [&] {
        const auto epoch = ++windowEpoch;
        native->setTelemetryEnabled(true);
        if (auto *router = native->inputRouter())
            router->attach(window);
        renderConnection = QObject::connect(
                window, &QQuickWindow::beforeRendering, window,
                [separateRenderThread, guiThread] {
                    separateRenderThread->store(QThread::currentThread() != guiThread);
                },
                Qt::DirectConnection);
        QObject::connect(
                window, &QQuickWindow::sceneGraphInitialized, &reviewLifetime,
                [&, epoch] {
                    if (epoch != windowEpoch)
                        return;
                    ++contexts;
                },
                Qt::QueuedConnection);
        QObject::connect(native, &rhi::SceneItem::frameSubmitted, &reviewLifetime,
                         [&, epoch](QJsonObject sample) {
                             if (epoch != windowEpoch)
                                 return;
                             sample[u"stage"_s] = stage;
                             sample[u"elapsedNs"_s] = time.nsecsElapsed();
                             samples.append(sample);
                         });
        QObject::connect(
                window, &QQuickWindow::frameSwapped, &reviewLifetime,
                [&, epoch] {
                    if (epoch != windowEpoch || finished)
                        return;
                    const auto covers = native->coverStatistics();
                    peakOwned = std::max(peakOwned, covers[u"owned"_s].toInteger());
                    peakReturning =
                            std::max(peakReturning, qint64(selection->returningCases().size()));
                    const QString phase = presentation.phase();
                    QJsonArray owners;
                    QSet<quint64> ids;
                    bool unique = true;
                    for (const auto &instance : selection->instances()) {
                        unique &= !ids.contains(instance.objectId);
                        ids.insert(instance.objectId);
                        if (instance.owned)
                            owners.append(QJsonObject{ { u"objectId"_s, qint64(instance.objectId) },
                                                       { u"albumId"_s, instance.albumId },
                                                       { u"clarity"_s, instance.clarity } });
                    }
                    if (!unique || !native->errorText().isEmpty()) {
                        check(u"unique-identity-and-render"_s, false);
                        finish();
                        return;
                    }
                    states.append(QJsonObject{
                            { u"timeNs"_s, time.nsecsElapsed() },
                            { u"stage"_s, stage },
                            { u"phase"_s, phase },
                            { u"detail"_s, scene.detail() },
                            { u"selectedClarity"_s, selection->selectedClarity() },
                            { u"individualMaterials"_s, scene.frame().individualMaterials },
                            { u"lift"_s, selection->liftState().value },
                            { u"rotation"_s, selection->rotation() },
                            { u"albumId"_s, selection->albumId() },
                            { u"owners"_s, owners },
                            { u"covers"_s, covers },
                            { u"frameRevision"_s, qint64(scene.frameRevision()) } });
                    switch (stage) {
                    case 0:
                        if (!covers[u"selectedDetailed"_s].toBool() || samples.isEmpty())
                            return;
                        initialId = selection->albumId();
                        recordAction();
                        presentation.open();
                        ++stage;
                        break;
                    case 1:
                        if (!ready())
                            return;
                        if (!initialReadinessRecorded) {
                            capture(u"opened"_s);
                            latencies.append(QJsonObject{
                                    { u"action"_s, u"open"_s },
                                    { u"readyMs"_s, (time.nsecsElapsed() - actionAt) / 1e6 } });
                            initialReadinessRecorded = true;
                        }
                        // Start the rotation from a clear selected lid, so the outgoing
                        // copy's independently refrosting state is exercised as well.
                        if (selection->selectedClarity() < .999)
                            return;
                        capture(u"opened-clear"_s);
                        {
                            const auto center = scene.projectWorldPoint(
                                    scene.frame().caseTransform.map(QVector3D(0, 1.85, 0)));
                            dragStart = { center.x(), center.y() };
                            recordAction();
                            mouse(QEvent::MouseButtonPress, dragStart, Qt::LeftButton);
                            mouse(QEvent::MouseMove, dragStart + QPointF(180, 70), Qt::LeftButton);
                            check(u"native-mouse-drag-started"_s, selection->dragging());
                        }
                        ++stage;
                        break;
                    case 2:
                        if (selection->rotation() < .5)
                            return;
                        heldY = selection->selectedPosition().y;
                        rotatedOwner = selection->instances().last().objectId;
                        capture(u"dragged"_s);
                        expected = selection->adjacentAlbum({ }, 1);
                        recordAction();
                        button(u"detailNext"_s);
                        check(u"switch-cancels-drag"_s, !selection->dragging());
                        ++stage;
                        break;
                    case 3:
                        if (phase == "switching"_L1) {
                            check(u"fixed-detail-composition"_s,
                                  scene.detail() == 1 && !motion.moving());
                            for (const auto &copy : selection->returningCases())
                                if (copy.objectId == rotatedOwner && copy.returnY)
                                    check(u"aligned-before-descending"_s,
                                          std::abs(*copy.returnY - heldY) < 1e-5
                                                  && copy.albumId == initialId);
                        }
                        if (!ready())
                            return;
                        check(u"first-switch-identity"_s, selection->albumId() == expected);
                        capture(u"switched"_s);
                        latencies.append(QJsonObject{
                                { u"action"_s, u"rotated-switch"_s },
                                { u"readyMs"_s, (time.nsecsElapsed() - actionAt) / 1e6 } });
                        switches = 1;
                        expected = selection->adjacentAlbum({ }, 1);
                        recordAction();
                        button(u"detailNext"_s);
                        ++stage;
                        break;
                    case 4:
                        if (!ready())
                            return;
                        check(u"switch-identity"_s, selection->albumId() == expected);
                        latencies.append(QJsonObject{
                                { u"action"_s, u"switch-%1"_s.arg(switches) },
                                { u"readyMs"_s, (time.nsecsElapsed() - actionAt) / 1e6 } });
                        ++switches;
                        if (switches < 4) {
                            const int direction = switches == 3 ? -1 : 1;
                            expected = selection->adjacentAlbum({ }, direction);
                            recordAction();
                            button(direction > 0 ? u"detailNext"_s : u"detailPrevious"_s);
                        } else {
                            presentation.stepAlbum(1);
                            presentation.stepAlbum(1);
                            presentation.stepAlbum(-1);
                            expected = presentation.pendingSelection()->albumId;
                            ++stage;
                        }
                        break;
                    case 5:
                        if (!ready())
                            return;
                        check(u"burst-latest-target"_s, selection->albumId() == expected);
                        recordAction();
                        presentation.stepAlbum(1);
                        ++stage;
                        break;
                    case 6:
                        if (phase != "switching"_L1 || selection->liftState().value < .1)
                            return;
                        presentation.back();
                        // Search uses the existing archive-route port; the search panel belongs to R23.
                        presentation.select(initialId, true, true);
                        ++stage;
                        break;
                    case 7:
                        if (!ready())
                            return;
                        check(u"interrupted-return-search"_s, selection->albumId() == initialId);
                        motion.clock()->setSpeed(.25);
                        recordAction();
                        presentation.stepAlbum(1);
                        ++stage;
                        break;
                    case 8:
                        if (time.nsecsElapsed() - actionAt < 350000000)
                            return;
                        motion.clock()->setSpeed(3);
                        motion.setReducedMotion(true);
                        ++stage;
                        break;
                    case 9:
                        if (!ready())
                            return;
                        check(u"reduced-complete-and-release"_s,
                              selection->returningCases().size() == 0);
                        if (!lifecycleStress) {
                            ++stage;
                            break;
                        }
                        if (++stress <= 30) {
                            presentation.stepAlbum(1);
                            // A reduced switch is synchronously ready on the next shared frame.
                            if (stress == 30)
                                ++stage;
                        }
                        break;
                    case 10:
                        if (!ready())
                            return;
                        check(u"stress-resource-return"_s,
                              selection->returningCases().size() == 0
                                      && covers[u"owned"_s].toInt() == 1);
                        {
                            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                            QCoreApplication::sendEvent(window, &escape);
                        }
                        ++stage;
                        break;
                    case 11:
                        if (!presentation.archiveEnabled() || !selection->archiveReady())
                            return;
                        capture(u"returned"_s);
                        presentation.open();
                        ++stage;
                        break;
                    case 12:
                        if (!ready())
                            return;
                        if (!lifecycleStress) {
                            expected = selection->adjacentAlbum({ }, 1);
                            button(u"detailNext"_s);
                            stage = 16;
                            break;
                        }
                        samplesBeforeRebuild = samples.size();
                        window->hide();
                        window->releaseResources();
                        window->destroy();
                        verification::prepareWindow(*window, true);
                        window->create();
                        check(u"recreated-hidden-policy"_s,
                              !window->isVisible() && verification::windowPolicySatisfied(*window));
                        ++stage;
                        QTimer::singleShot(0, &reviewLifetime, [&] {
                            window->show();
                            window->update();
                        });
                        break;
                    case 13:
                        if (!ready() || samples.size() <= samplesBeforeRebuild
                            || !window->isExposed())
                            return;
                        ++rebuilds;
                        lifecycle.append(QJsonObject{
                                { u"cycle"_s, rebuilds },
                                { u"covers"_s, covers },
                                { u"windowPolicy"_s, verification::windowPolicy(*window) },
                                { u"lastSubmission"_s, samples.last() } });
                        if (rebuilds < 10)
                            stage = 12;
                        else {
                            capture(u"restored"_s);
                            ++stage;
                        }
                        break;
                    case 14: {
                        if (!ready())
                            return;
                        samplesBeforeRebuild = samples.size();
                        ++windowEpoch;
                        native->setTelemetryEnabled(false);
                        window->hide();
                        // Deleting the last QQuickWindow guarantees graph/device cleanup,
                        // including a basic loop's otherwise shared context (Qt contract).
                        delete window;
                        window = nullptr;
                        native = nullptr;
                        engine->loadFromModule("Yalahoo.Music", "App");
                        window = engine->rootObjects().isEmpty()
                                ? nullptr
                                : qobject_cast<QQuickWindow *>(engine->rootObjects().last());
                        native = window ? qobject_cast<rhi::SceneItem *>(
                                                  find(window->contentItem(), u"nativeScene"_s))
                                        : nullptr;
                        if (!window || !native) {
                            app.exit(8);
                            return;
                        }
                        verification::prepareWindow(*window, true);
                        window->setPersistentGraphics(false);
                        window->setPersistentSceneGraph(false);
                        window->resize(1280, 720);
                        bindWindow();
                        ++stage;
                        window->show();
                        window->update();
                        break;
                    }
                    case 15:
                        if (!ready() || samples.size() <= samplesBeforeRebuild
                            || !window->isExposed())
                            return;
                        ++fullRebuilds;
                        if (fullRebuilds < 10)
                            stage = 14;
                        else {
                            expected = selection->adjacentAlbum({ }, 1);
                            button(u"detailNext"_s);
                            ++stage;
                        }
                        break;
                    case 16:
                        if (!ready())
                            return;
                        check(u"rebuilt-detail-input"_s, selection->albumId() == expected);
                        capture(u"full-root-restored"_s);
                        ++stage;
                        finish();
                        break;
                    default:
                        break;
                    }
                },
                Qt::QueuedConnection);
    };
    bindWindow();
    QTimer::singleShot(120000, &reviewLifetime, finish);
    window->show();
    const int result = app.exec();
    QObject::disconnect(renderConnection);
    if (native)
        native->setTelemetryEnabled(false);
    app.setQuitOnLastWindowClosed(quitOnClose);
    return result;
}
