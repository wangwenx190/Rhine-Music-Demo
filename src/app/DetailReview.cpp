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
    auto *native = qobject_cast<rhi::SceneItem *>(find(window->contentItem(), "nativeScene"));
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
        checks.append(QJsonObject{ { "name", name }, { "passed", value }, { "stage", stage } });
    };
    const auto capture = [&](const QString &name) {
        const auto image = window->grabWindow();
        check("capture-" + name, !image.isNull() && image.save(output + '/' + name + ".png"));
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
            check("button-" + name, false);
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
        const auto owners = cover["owners"].toArray();
        if (owners.size() != expectedOwners.size())
            return false;
        for (const auto &value : owners) {
            const auto owner = value.toObject();
            const auto objectId = quint64(owner["objectId"].toInteger());
            if (expectedOwners.value(objectId) != owner["albumId"].toString())
                return false;
            selectedOwner |= objectId == selection->instances().last().objectId
                    && owner["resolution"].toInt() == 1024;
        }
        return selectedOwner && presentation.detailEnabled() && selection->canInspect()
                && cover["selectedDetailed"].toBool();
    };
    const auto finish = [&] {
        if (finished)
            return;
        finished = true;
        check("stage-completed", stage == 17);
        if (lifecycleStress) {
            check("ten-full-qml-window-rebuilds", fullRebuilds == 10);
            check("ten-product-context-restores", rebuilds == 10 && contexts >= 11);
        } else {
            check("smoke-keeps-original-window", fullRebuilds == 0 && rebuilds == 0);
        }
        check("render-error-empty", native->errorText().isEmpty());
        check("qml-warnings-empty", warnings.isEmpty());
        check("playback-independent", library.playbackTrackId() == playback);
        check("window-policy", verification::windowPolicySatisfied(*window));
        check("bounded-owners", peakOwned <= 4 && peakReturning <= 3);
        check("normal-three-switches-and-reverse", switches == 4);
        check("requested-additional-switches", stress == (lifecycleStress ? 30 : 0));
        QSet<qint64> generations, postRevisions;
        qint64 peakUpload = 0, resourceBytes = 0;
        for (const auto &value : samples) {
            const auto sample = value.toObject();
            if (sample["stage"].toInt() < 12) {
                generations.insert(sample["resourceGeneration"].toInteger());
                postRevisions.insert(sample["postprocessRevision"].toInteger());
            }
            peakUpload = std::max(peakUpload, sample["atlasUploadBytes"].toInteger());
            resourceBytes =
                    std::max(resourceBytes,
                             sample["resources"].toObject()["estimatedCurrentBytes"].toInteger());
        }
        check("transforms-do-not-rebuild-targets",
              generations.size() == 1 && postRevisions.size() == 1);
        check("bounded-upload-burst", peakUpload <= 2 * 1024 * 1024);
        QJsonObject report{
            { "passed", passed },
            { "configuration", "Release" },
            { "qtVersion", qVersion() },
            { "backend",
              window->rendererInterface()->graphicsApi() == QSGRendererInterface::Metal ? "metal"
                      : window->rendererInterface()->graphicsApi()
                              == QSGRendererInterface::Direct3D11
                      ? "d3d11"
                      : "opengl" },
            { "renderLoop", qEnvironmentVariable("QSG_RENDER_LOOP", "Qt default") },
            { "separateRenderThreadObserved", separateRenderThread->load() },
            { "dpr", window->effectiveDevicePixelRatio() },
            { "windowPolicy", verification::windowPolicy(*window) },
            { "checks", checks },
            { "qmlWarnings", QJsonArray::fromStringList(warnings) },
            { "states", states },
            { "samples", samples },
            { "latencies", latencies },
            { "lifecycle", lifecycle },
            { "contexts", contexts },
            { "fullWindowRebuilds", fullRebuilds },
            { "lifecycleStressRequested", lifecycleStress },
            { "nativeInputSubmission", qint64(native->submission()) },
            { "nativeInputFrameRevision", qint64(native->input().scene.revision) },
            { "nativeItemVisible", native->isVisible() },
            { "peakOwned", peakOwned },
            { "peakAtlasUploadBytes", peakUpload },
            { "estimatedCurrentResourceMaxBytes", resourceBytes },
            { "peakReturning", peakReturning },
            { "finalCovers", native->coverStatistics() },
            { "durationMs", time.elapsed() },
            { "measurementScope",
              "Product Qt submission, readiness and resource telemetry; no display/photon timing or measured VRAM" }
        };
        QSaveFile file(output + "/report.json");
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
                             sample["stage"] = stage;
                             sample["elapsedNs"] = time.nsecsElapsed();
                             samples.append(sample);
                         });
        QObject::connect(
                window, &QQuickWindow::frameSwapped, &reviewLifetime,
                [&, epoch] {
                    if (epoch != windowEpoch || finished)
                        return;
                    const auto covers = native->coverStatistics();
                    peakOwned = std::max(peakOwned, covers["owned"].toInteger());
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
                            owners.append(QJsonObject{ { "objectId", qint64(instance.objectId) },
                                                       { "albumId", instance.albumId },
                                                       { "clarity", instance.clarity } });
                    }
                    if (!unique || !native->errorText().isEmpty()) {
                        check("unique-identity-and-render", false);
                        finish();
                        return;
                    }
                    states.append(QJsonObject{
                            { "timeNs", time.nsecsElapsed() },
                            { "stage", stage },
                            { "phase", phase },
                            { "detail", scene.detail() },
                            { "selectedClarity", selection->selectedClarity() },
                            { "individualMaterials", scene.frame().individualMaterials },
                            { "lift", selection->liftState().value },
                            { "rotation", selection->rotation() },
                            { "albumId", selection->albumId() },
                            { "owners", owners },
                            { "covers", covers },
                            { "frameRevision", qint64(scene.frameRevision()) } });
                    switch (stage) {
                    case 0:
                        if (!covers["selectedDetailed"].toBool() || samples.isEmpty())
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
                            capture("opened");
                            latencies.append(QJsonObject{
                                    { "action", "open" },
                                    { "readyMs", (time.nsecsElapsed() - actionAt) / 1e6 } });
                            initialReadinessRecorded = true;
                        }
                        // Start the rotation from a clear selected lid, so the outgoing
                        // copy's independently refrosting state is exercised as well.
                        if (selection->selectedClarity() < .999)
                            return;
                        capture("opened-clear");
                        {
                            const auto center = scene.projectWorldPoint(
                                    scene.frame().caseTransform.map(QVector3D(0, 1.85, 0)));
                            dragStart = { center.x(), center.y() };
                            recordAction();
                            mouse(QEvent::MouseButtonPress, dragStart, Qt::LeftButton);
                            mouse(QEvent::MouseMove, dragStart + QPointF(180, 70), Qt::LeftButton);
                            check("native-mouse-drag-started", selection->dragging());
                        }
                        ++stage;
                        break;
                    case 2:
                        if (selection->rotation() < .5)
                            return;
                        heldY = selection->selectedPosition().y;
                        rotatedOwner = selection->instances().last().objectId;
                        capture("dragged");
                        expected = selection->adjacentAlbum({ }, 1);
                        recordAction();
                        button("detailNext");
                        check("switch-cancels-drag", !selection->dragging());
                        ++stage;
                        break;
                    case 3:
                        if (phase == "switching") {
                            check("fixed-detail-composition",
                                  scene.detail() == 1 && !motion.moving());
                            for (const auto &copy : selection->returningCases())
                                if (copy.objectId == rotatedOwner && copy.returnY)
                                    check("aligned-before-descending",
                                          std::abs(*copy.returnY - heldY) < 1e-5
                                                  && copy.albumId == initialId);
                        }
                        if (!ready())
                            return;
                        check("first-switch-identity", selection->albumId() == expected);
                        capture("switched");
                        latencies.append(QJsonObject{
                                { "action", "rotated-switch" },
                                { "readyMs", (time.nsecsElapsed() - actionAt) / 1e6 } });
                        switches = 1;
                        expected = selection->adjacentAlbum({ }, 1);
                        recordAction();
                        button("detailNext");
                        ++stage;
                        break;
                    case 4:
                        if (!ready())
                            return;
                        check("switch-identity", selection->albumId() == expected);
                        latencies.append(QJsonObject{
                                { "action", QString("switch-%1").arg(switches) },
                                { "readyMs", (time.nsecsElapsed() - actionAt) / 1e6 } });
                        ++switches;
                        if (switches < 4) {
                            const int direction = switches == 3 ? -1 : 1;
                            expected = selection->adjacentAlbum({ }, direction);
                            recordAction();
                            button(direction > 0 ? "detailNext" : "detailPrevious");
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
                        check("burst-latest-target", selection->albumId() == expected);
                        recordAction();
                        presentation.stepAlbum(1);
                        ++stage;
                        break;
                    case 6:
                        if (phase != "switching" || selection->liftState().value < .1)
                            return;
                        presentation.back();
                        // Search uses the existing archive-route port; the search panel belongs to R23.
                        presentation.select(initialId, true, true);
                        ++stage;
                        break;
                    case 7:
                        if (!ready())
                            return;
                        check("interrupted-return-search", selection->albumId() == initialId);
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
                        check("reduced-complete-and-release",
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
                        check("stress-resource-return",
                              selection->returningCases().size() == 0
                                      && covers["owned"].toInt() == 1);
                        {
                            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                            QCoreApplication::sendEvent(window, &escape);
                        }
                        ++stage;
                        break;
                    case 11:
                        if (!presentation.archiveEnabled() || !selection->archiveReady())
                            return;
                        capture("returned");
                        presentation.open();
                        ++stage;
                        break;
                    case 12:
                        if (!ready())
                            return;
                        if (!lifecycleStress) {
                            expected = selection->adjacentAlbum({ }, 1);
                            button("detailNext");
                            stage = 16;
                            break;
                        }
                        samplesBeforeRebuild = samples.size();
                        window->hide();
                        window->releaseResources();
                        window->destroy();
                        verification::prepareWindow(*window, true);
                        window->create();
                        check("recreated-hidden-policy",
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
                                { "cycle", rebuilds },
                                { "covers", covers },
                                { "windowPolicy", verification::windowPolicy(*window) },
                                { "lastSubmission", samples.last() } });
                        if (rebuilds < 10)
                            stage = 12;
                        else {
                            capture("restored");
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
                                                  find(window->contentItem(), "nativeScene"))
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
                            button("detailNext");
                            ++stage;
                        }
                        break;
                    case 16:
                        if (!ready())
                            return;
                        check("rebuilt-detail-input", selection->albumId() == expected);
                        capture("full-root-restored");
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
