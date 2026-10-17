#include "OverviewReview.h"
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
#include <QSaveFile>
#include <QQmlEngine>
#include <QQmlError>
#include <QMutex>
#include <QMutexLocker>

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
int yalahoo::reviewOverview(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                            OverviewController &overview, PresentationController &presentation,
                            LibrarySession &library, MotionController &motion,
                            const QString &output)
{
    auto *native = qobject_cast<rhi::SceneItem *>(find(window.contentItem(), u"nativeScene"_s));
    auto *selection = scene.selection();
    if (!native || !selection)
        return 4;
    QDir().mkpath(output);
    native->setTelemetryEnabled(true);
    window.setPersistentGraphics(false);
    window.setPersistentSceneGraph(false);
    window.resize(1280, 720);
    const bool initialNight = qEnvironmentVariableIntValue("YALAHOO_VERIFY_NIGHT") == 1;
    const int requestedCycles =
            qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1 ? 10 : 0;
    scene.lighting()->setNight(initialNight);
    window.setProperty("reviewPlayingTitle",
                       u"\u6b63\u5728\u64ad\u653e \u00b7 \u56fa\u5b9a\u4f4d\u7f6e"_s);
    const QString playback = library.playbackTrackId();
    QObject lifetime;
    QJsonArray checks, samples, states, pictures;
    QStringList warnings;
    QElapsedTimer time;
    time.start();
    int stage = 0, frames = 0, sizeIndex = 0, cycle = 0, peakLabels = 0;
    int visibleLane = -1;
    quint64 priorSubmission = 0;
    double pausedProgress = 0;
    QString selected;
    QPointF playPosition, stopPosition;
    QMutex positionsMutex;
    QPointF synchronizedPlay, synchronizedStop;
    bool passed = true, finished = false;
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
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
        auto *item = find(window.contentItem(), name);
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
    const auto finish = [&] {
        if (finished)
            return;
        finished = true;
        check(u"sequence-completed"_s, stage == 19);
        check(u"requested-lifecycle-cycles"_s, cycle == requestedCycles);
        check(u"playback-isolated"_s, playback == library.playbackTrackId());
        check(u"qml-warnings"_s, warnings.isEmpty());
        check(u"window-policy"_s, verification::windowPolicySatisfied(window));
        check(u"renderer-errors"_s, native->errorText().isEmpty());
        QSet<qint64> generations, postprocess;
        qint64 peakUpload = 0;
        for (const auto value : samples) {
            const auto sample = value.toObject();
            if (sample[u"stage"_s].toInt() >= 1 && sample[u"stage"_s].toInt() <= 7) {
                generations.insert(sample[u"resourceGeneration"_s].toInteger());
                postprocess.insert(sample[u"postprocessRevision"_s].toInteger());
            }
            peakUpload = std::max(peakUpload, sample[u"atlasUploadBytes"_s].toInteger());
        }
        check(u"overview-keeps-render-targets"_s,
              generations.size() == 1 && postprocess.size() == 1);
        check(u"upload-burst"_s, peakUpload <= 2 * 1024 * 1024);
        QJsonObject report{
            { u"passed"_s, passed },
            { u"configuration"_s, u"Release"_s },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"backend"_s, qEnvironmentVariable("QSG_RHI_BACKEND") },
            { u"dpr"_s, window.effectiveDevicePixelRatio() },
            { u"windowPolicy"_s, verification::windowPolicy(window) },
            { u"checks"_s, checks },
            { u"pictures"_s, pictures },
            { u"qmlWarnings"_s, QJsonArray::fromStringList(warnings) },
            { u"states"_s, states },
            { u"samples"_s, samples },
            { u"cycles"_s, cycle },
            { u"requestedLifecycleCycles"_s, requestedCycles },
            { u"peakLabelSlots"_s, peakLabels },
            { u"overviewUiAttachments"_s,
              QJsonObject{
                      { u"scope"_s,
                        u"Separate Qt Quick crop/horizontal layers; descriptor estimate, not driver allocation"_s },
                      { u"maximumSlots"_s, 18 },
                      { u"texturesPerSlot"_s, 2 },
                      { u"width"_s, 308 },
                      { u"height"_s, 129 },
                      { u"rgba8PayloadBytesUpperBound"_s, 5721408 },
                      { u"withDepthStencilUpperBoundBytes"_s, 17164224 },
                      { u"resizeOverlapUpperBoundBytes"_s, 34328448 },
                      { u"actualDriverAllocationBytes"_s, QJsonValue::Null } } },
            { u"peakAtlasUploadBytes"_s, peakUpload },
            { u"durationMs"_s, time.elapsed() },
            { u"clockActive"_s, motion.clock()->active() },
            { u"applicationVisible"_s, motion.clock()->applicationVisible() },
            { u"cameraSettled"_s, scene.settled() },
            { u"archiveReady"_s, selection->archiveReady() },
            { u"measurementScope"_s,
              u"Qt frame/renderer telemetry and own-window PNG; no input-to-photon or measured VRAM"_s }
        };
        QSaveFile file(output + "/report.json"_L1);
        if (!file.open(QIODevice::WriteOnly)) {
            app.exit(5);
            return;
        }
        file.write(QJsonDocument(report).toJson());
        app.exit(file.commit() && passed ? 0 : 6);
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
                auto *play = find(window.contentItem(), u"playButton"_s);
                auto *stop = find(window.contentItem(), u"stopButton"_s);
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
                if (finished)
                    return;
                // The finite probe also advances through static reduced-motion states.
                // Request its next actual Qt frame without adding a product clock or timer.
                const auto nextFrame = qScopeGuard([&] {
                    if (!finished && window.isVisible())
                        window.update();
                });
                ++frames;
                states.append(QJsonObject{ { u"stage"_s, stage },
                                           { u"progress"_s, overview.progress() },
                                           { u"album"_s, selection->albumId() },
                                           { u"labels"_s, overview.labels()->liveCount() },
                                           { u"frameRevision"_s, qint64(scene.frameRevision()) } });
                peakLabels = std::max(peakLabels, overview.labels()->liveCount());
                if (overview.labels()->liveCount() > 18)
                    check(u"label-capacity"_s, false);
                switch (stage) {
                case 0:
                    if (!ready() || frames < 30)
                        return;
                    {
                        QMutexLocker lock(&positionsMutex);
                        playPosition = synchronizedPlay;
                        stopPosition = synchronizedStop;
                    }
                    selected = selection->albumId();
                    button(u"overviewToggle"_s);
                    check(u"toggle-enter"_s, overview.active());
                    ++stage;
                    break;
                case 1:
                    if (!ready() || overview.progress() < .999)
                        return;
                    {
                        QMutexLocker lock(&positionsMutex);
                        check(u"fixed-transport-buttons"_s,
                              QLineF(playPosition, synchronizedPlay).length() <= .01
                                      && QLineF(stopPosition, synchronizedStop).length() <= .01);
                    }
                    visibleLane = firstLane();
                    check(u"visible-label"_s, visibleLane >= 0);
                    check(u"no-one-step-entry"_s, !overview.enter(visibleLane));
                    button("overviewLabel-"_L1 + QString::number(visibleLane));
                    check(u"first-click-only-expands"_s,
                          overview.active() && overview.labels()->canEnter(visibleLane)
                                  && selected == selection->albumId());
                    frames = 0;
                    ++stage;
                    break;
                case 2:
                    if (frames < 35)
                        return;
                    capture(u"expanded"_s);
                    button("overviewEnter-"_L1 + QString::number(visibleLane));
                    check(u"second-click-enters-near"_s,
                          !overview.active() && !presentation.wantsDetail());
                    ++stage;
                    break;
                case 3:
                    if (!ready() || overview.progress() > .001
                        || !presentation.browseSurface()->shown())
                        return;
                    check(u"return-focus"_s,
                          find(window.contentItem(), u"overviewToggle"_s)->hasActiveFocus());
                    key(Qt::Key_V);
                    ++stage;
                    break;
                case 4:
                    if (!ready() || overview.progress() < .999)
                        return;
                    button(u"overviewNext"_s);
                    check(u"column-navigation-stays-overview"_s,
                          overview.active() && !overview.labels()->canEnter(visibleLane));
                    ++stage;
                    break;
                case 5:
                    if (!ready())
                        return;
                    key(Qt::Key_Escape);
                    check(u"escape-return"_s, !overview.active());
                    ++stage;
                    break;
                case 6:
                    if (!ready() || overview.progress() > .001)
                        return;
                    key(Qt::Key_V);
                    ++stage;
                    break;
                case 7: {
                    if (!ready() || overview.progress() < .999)
                        return;
                    bool clicked = false;
                    for (const auto &instance : selection->instances()) {
                        const auto point =
                                scene.projectWorldPoint(instance.transform.map({ 0, 1.85f, 0 }));
                        if (point.x() < 50 || point.x() > window.width() - 50 || point.y() < 230
                            || point.y() > window.height() - 140)
                            continue;
                        if (!scene.hitTest(point.x(), point.y()).isEmpty()) {
                            clickPoint({ point.x(), point.y() });
                            clicked = true;
                            break;
                        }
                    }
                    check(u"album-click-returns"_s, clicked && !overview.active());
                    ++stage;
                    break;
                }
                case 8:
                    if (!ready() || overview.progress() > .001)
                        return;
                    overview.setActive(true);
                    frames = 0;
                    ++stage;
                    break;
                case 9:
                    if (frames < 8)
                        return;
                    overview.setActive(false);
                    motion.clock()->setSpeed(.25);
                    frames = 0;
                    ++stage;
                    break;
                case 10:
                    if (frames < 8)
                        return;
                    overview.setActive(true);
                    motion.clock()->setSpeed(3);
                    ++stage;
                    break;
                case 11:
                    motion.setReducedMotion(true);
                    check(u"reduced-finishes-progress"_s, overview.progress() == 1);
                    selection->setRealistic(true);
                    frames = 0;
                    ++stage;
                    break;
                case 12:
                    if (!ready() || frames < 10)
                        return;
                    window.resize(sizes[sizeIndex]);
                    scene.lighting()->setNight(sizeIndex % 2 ? !initialNight : initialNight);
                    frames = 0;
                    ++stage;
                    break;
                case 13: {
                    if (!ready() || frames < 20)
                        return;
                    QVector<QRectF> occupied;
                    for (int row = 0; row < overview.labels()->rowCount(); ++row) {
                        const auto index = overview.labels()->index(row);
                        if (!overview.labels()
                                     ->data(index, OverviewLabelsModel::Interactive)
                                     .toBool())
                            continue;
                        const int lane =
                                overview.labels()->data(index, OverviewLabelsModel::Lane).toInt();
                        auto *label = find(window.contentItem(),
                                           "overviewLabel-"_L1 + QString::number(lane));
                        if (!label) {
                            check(u"label-object"_s, false);
                            continue;
                        }
                        const auto rect = label->mapRectToScene(label->boundingRect());
                        check(u"label-inside-viewport"_s,
                              rect.left() >= -18 && rect.right() + 76 <= window.width() - 14);
                        for (const auto &other : occupied)
                            check(u"labels-do-not-overlap"_s, !rect.intersects(other));
                        occupied.append(rect);
                    }
                    capture("size-"_L1 + QString::number(sizeIndex));
                    if (++sizeIndex < sizes.size())
                        stage = 12;
                    else {
                        window.resize(1280, 720);
                        ++stage;
                    }
                    break;
                }
                case 14:
                    if (!ready())
                        return;
                    if (!requestedCycles) {
                        overview.setActive(false);
                        stage = 16;
                        break;
                    }
                    pausedProgress = overview.progress();
                    priorSubmission = quint64(samples.size());
                    window.hide();
                    window.releaseResources();
                    window.destroy();
                    ++stage;
                    QTimer::singleShot(40, &lifetime, [&] {
                        check(u"hidden-clock-paused"_s,
                              !motion.clock()->active() && overview.progress() == pausedProgress);
                        window.create();
                        window.show();
                        window.update();
                    });
                    break;
                case 15:
                    if (!ready() || quint64(samples.size()) <= priorSubmission
                        || !window.isExposed())
                        return;
                    if (++cycle < requestedCycles)
                        stage = 14;
                    else {
                        capture(u"restored"_s);
                        overview.setActive(false);
                        ++stage;
                    }
                    break;
                case 16:
                    if (!ready() || overview.progress() != 0)
                        return;
                    overview.setActive(true);
                    frames = 0;
                    ++stage;
                    break;
                case 17:
                    if (!ready() || frames < 5)
                        return;
                    key(Qt::Key_V);
                    check(u"v-return"_s, !overview.active());
                    ++stage;
                    break;
                case 18:
                    if (!ready() || overview.labels()->liveCount() != 0)
                        return;
                    check(u"labels-release"_s, overview.labels()->liveCount() == 0);
                    check(u"archive-no-detail"_s,
                          presentation.archiveEnabled() && !presentation.wantsDetail());
                    ++stage;
                    finish();
                    break;
                default:
                    break;
                }
            },
            Qt::QueuedConnection);
    QTimer::singleShot(120000, &lifetime, finish);
    window.show();
    const int result = app.exec();
    QObject::disconnect(positions);
    native->setTelemetryEnabled(false);
    return result;
}
