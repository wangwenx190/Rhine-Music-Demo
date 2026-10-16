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
    auto *native = qobject_cast<rhi::SceneItem *>(find(window.contentItem(), "nativeScene"));
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
    window.setProperty("reviewPlayingTitle", QStringLiteral("正在播放 · 固定位置"));
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
        checks.append(QJsonObject{ { "name", name }, { "passed", result }, { "stage", stage } });
    };
    const auto capture = [&](const QString &name) {
        const auto image = window.grabWindow();
        check("capture-" + name, !image.isNull() && image.save(output + '/' + name + ".png"));
        QJsonArray anchors;
        for (const auto &c : overview.projectColumns())
            anchors.append(QJsonObject{ { "lane", c.lane },
                                        { "name", c.name },
                                        { "x", c.x },
                                        { "y", c.y },
                                        { "count", c.count },
                                        { "extent", c.extent },
                                        { "selected", c.selected } });
        pictures.append(QJsonObject{ { "name", name },
                                     { "width", window.width() },
                                     { "height", window.height() },
                                     { "night", scene.lighting()->night() },
                                     { "realistic", selection->realistic() },
                                     { "anchors", anchors } });
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
            check("button-" + name, false);
            return;
        }
        clickPoint(item->mapToScene({ item->width() / 2, item->height() / 2 }));
    };
    const auto ready = [&] {
        return native->errorText().isEmpty() && native->submission() > 0 && scene.settled()
                && selection->archiveReady() && native->coverStatistics()["pending"].toInt() == 0
                && native->coverStatistics()["active"].toInt() == 0;
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
        check("sequence-completed", stage == 19);
        check("requested-lifecycle-cycles", cycle == requestedCycles);
        check("playback-isolated", playback == library.playbackTrackId());
        check("qml-warnings", warnings.isEmpty());
        check("window-policy", verification::windowPolicySatisfied(window));
        check("renderer-errors", native->errorText().isEmpty());
        QSet<qint64> generations, postprocess;
        qint64 peakUpload = 0;
        for (const auto value : samples) {
            const auto sample = value.toObject();
            if (sample["stage"].toInt() >= 1 && sample["stage"].toInt() <= 7) {
                generations.insert(sample["resourceGeneration"].toInteger());
                postprocess.insert(sample["postprocessRevision"].toInteger());
            }
            peakUpload = std::max(peakUpload, sample["atlasUploadBytes"].toInteger());
        }
        check("overview-keeps-render-targets", generations.size() == 1 && postprocess.size() == 1);
        check("upload-burst", peakUpload <= 2 * 1024 * 1024);
        QJsonObject report{
            { "passed", passed },
            { "configuration", "Release" },
            { "qtVersion", qVersion() },
            { "backend", qEnvironmentVariable("QSG_RHI_BACKEND") },
            { "dpr", window.effectiveDevicePixelRatio() },
            { "windowPolicy", verification::windowPolicy(window) },
            { "checks", checks },
            { "pictures", pictures },
            { "qmlWarnings", QJsonArray::fromStringList(warnings) },
            { "states", states },
            { "samples", samples },
            { "cycles", cycle },
            { "requestedLifecycleCycles", requestedCycles },
            { "peakLabelSlots", peakLabels },
            { "overviewUiAttachments",
              QJsonObject{
                      { "scope",
                        "Separate Qt Quick crop/horizontal layers; descriptor estimate, not driver allocation" },
                      { "maximumSlots", 18 },
                      { "texturesPerSlot", 2 },
                      { "width", 308 },
                      { "height", 129 },
                      { "rgba8PayloadBytesUpperBound", 5721408 },
                      { "withDepthStencilUpperBoundBytes", 17164224 },
                      { "resizeOverlapUpperBoundBytes", 34328448 },
                      { "actualDriverAllocationBytes", QJsonValue::Null } } },
            { "peakAtlasUploadBytes", peakUpload },
            { "durationMs", time.elapsed() },
            { "clockActive", motion.clock()->active() },
            { "applicationVisible", motion.clock()->applicationVisible() },
            { "cameraSettled", scene.settled() },
            { "archiveReady", selection->archiveReady() },
            { "measurementScope",
              "Qt frame/renderer telemetry and own-window PNG; no input-to-photon or measured VRAM" }
        };
        QSaveFile file(output + "/report.json");
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
        sample["stage"] = stage;
        samples.append(sample);
    });
    // GUI is blocked during sync: measure the exact polished row consumed by this frame.
    const auto positions = QObject::connect(
            &window, &QQuickWindow::afterSynchronizing, &window,
            [&] {
                auto *play = find(window.contentItem(), "playButton");
                auto *stop = find(window.contentItem(), "stopButton");
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
                states.append(QJsonObject{ { "stage", stage },
                                           { "progress", overview.progress() },
                                           { "album", selection->albumId() },
                                           { "labels", overview.labels()->liveCount() },
                                           { "frameRevision", qint64(scene.frameRevision()) } });
                peakLabels = std::max(peakLabels, overview.labels()->liveCount());
                if (overview.labels()->liveCount() > 18)
                    check("label-capacity", false);
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
                    button("overviewToggle");
                    check("toggle-enter", overview.active());
                    ++stage;
                    break;
                case 1:
                    if (!ready() || overview.progress() < .999)
                        return;
                    {
                        QMutexLocker lock(&positionsMutex);
                        check("fixed-transport-buttons",
                              QLineF(playPosition, synchronizedPlay).length() <= .01
                                      && QLineF(stopPosition, synchronizedStop).length() <= .01);
                    }
                    visibleLane = firstLane();
                    check("visible-label", visibleLane >= 0);
                    check("no-one-step-entry", !overview.enter(visibleLane));
                    button("overviewLabel-" + QString::number(visibleLane));
                    check("first-click-only-expands",
                          overview.active() && overview.labels()->canEnter(visibleLane)
                                  && selected == selection->albumId());
                    frames = 0;
                    ++stage;
                    break;
                case 2:
                    if (frames < 35)
                        return;
                    capture("expanded");
                    button("overviewEnter-" + QString::number(visibleLane));
                    check("second-click-enters-near",
                          !overview.active() && !presentation.wantsDetail());
                    ++stage;
                    break;
                case 3:
                    if (!ready() || overview.progress() > .001
                        || !presentation.browseSurface()->shown())
                        return;
                    check("return-focus",
                          find(window.contentItem(), "overviewToggle")->hasActiveFocus());
                    key(Qt::Key_V);
                    ++stage;
                    break;
                case 4:
                    if (!ready() || overview.progress() < .999)
                        return;
                    button("overviewNext");
                    check("column-navigation-stays-overview",
                          overview.active() && !overview.labels()->canEnter(visibleLane));
                    ++stage;
                    break;
                case 5:
                    if (!ready())
                        return;
                    key(Qt::Key_Escape);
                    check("escape-return", !overview.active());
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
                    check("album-click-returns", clicked && !overview.active());
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
                    check("reduced-finishes-progress", overview.progress() == 1);
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
                                           "overviewLabel-" + QString::number(lane));
                        if (!label) {
                            check("label-object", false);
                            continue;
                        }
                        const auto rect = label->mapRectToScene(label->boundingRect());
                        check("label-inside-viewport",
                              rect.left() >= -18 && rect.right() + 76 <= window.width() - 14);
                        for (const auto &other : occupied)
                            check("labels-do-not-overlap", !rect.intersects(other));
                        occupied.append(rect);
                    }
                    capture("size-" + QString::number(sizeIndex));
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
                        check("hidden-clock-paused",
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
                        capture("restored");
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
                    check("v-return", !overview.active());
                    ++stage;
                    break;
                case 18:
                    if (!ready() || overview.labels()->liveCount() != 0)
                        return;
                    check("labels-release", overview.labels()->liveCount() == 0);
                    check("archive-no-detail",
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
