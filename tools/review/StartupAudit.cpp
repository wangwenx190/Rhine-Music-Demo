#include "StartupAudit.h"
#include "OffscreenSurface.h"
#include "StartupCoordinator.h"
#include "OverviewController.h"
#include "SceneItem.h"
#include "OpeningInk.h"
#include "RulerInk.h"
#include "ReelInk.h"
#include "RulerController.h"
#include "InputRouter.h"
#include "DetailPorts.h"
#include "DetailModel.h"
#include "PanelController.h"
#include "TrackFocusController.h"
#include "SettingsController.h"
#include "PlaybackController.h"
#include "VerificationWindow.h"
#include "AudioFixture.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QScreen>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QKeyEvent>
#include <algorithm>
#include <chrono>
#include <stdexcept>
using namespace yalahoo;
namespace {
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}
QQuickItem *find(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *item = find(child, name))
            return item;
    return nullptr;
}
QJsonObject distribution(QVector<double> values, double period = 1000.0 / 60)
{
    if (values.isEmpty())
        return { { "count", 0 }, { "p95Ms", QJsonValue::Null }, { "p99Ms", QJsonValue::Null } };
    int consecutive = 0, longest = 0;
    for (double value : values) {
        consecutive = value > 2 * period ? consecutive + 1 : 0;
        longest = qMax(longest, consecutive);
    }
    std::sort(values.begin(), values.end());
    const auto percentile = [&](double p) {
        return values[qMax(0, int(std::ceil(p * values.size())) - 1)];
    };
    const auto over = [&](double limit) {
        return int(std::count_if(values.begin(), values.end(), [=](double v) {
            return v > limit;
        }));
    };
    return { { "count", values.size() },
             { "p50Ms", percentile(.5) },
             { "p95Ms", percentile(.95) },
             { "p99Ms", percentile(.99) },
             { "maxMs", values.last() },
             { "over50Ms", over(50) },
             { "overOnePeriod", over(period) },
             { "overTwoPeriods", over(2 * period) },
             { "overThreePeriods", over(3 * period) },
             { "longestOverTwoPeriods", longest } };
}
QJsonObject counters()
{
    return { { "sceneRenders", qint64(rhi::sceneSubmissionStats().rendered.load()) },
             { "sceneRenderers", rhi::sceneSubmissionStats().renderers.load() },
             { "rulerPaints", qint64(rulerInkStats().paints.load()) },
             { "rulerSyncs", qint64(rulerInkStats().syncs.load()) },
             { "rulerPaintNs", qint64(rulerInkStats().paintNs.load()) },
             { "rulerSyncNs", qint64(rulerInkStats().syncNs.load()) },
             { "rulerRenderers", rulerInkStats().renderers.load() },
             { "openingPaints", qint64(openingInkStats().paints.load()) },
             { "openingSyncs", qint64(openingInkStats().syncs.load()) },
             { "openingPaintNs", qint64(openingInkStats().paintNs.load()) },
             { "openingSyncNs", qint64(openingInkStats().syncNs.load()) },
             { "openingRenderers", openingInkStats().renderers.load() },
             { "textNodes", ReelInk::liveNodes() },
             { "textGeometryUpdates", qint64(ReelInk::geometryUpdates()) },
             { "textGeometryReuses", qint64(ReelInk::geometryReuses()) } };
}
Snapshot fixture(const QString &directory)
{
    LibrarySnapshot library;
    QStringList paths;
    QImage image(2048, 2048, QImage::Format_RGB32);
    for (int i = 0; i < 96; ++i) {
        image.fill(QColor::fromHsv(i * 359 / 96, 140, 210));
        const auto path = directory + QString("/anonymous-%1.jpg").arg(i);
        require(image.save(path, "JPEG", 82), "Cannot create anonymous large cover");
        paths.append(path);
    }
    for (int lane = 0; lane < 9; ++lane) {
        Genre genre;
        genre.id = QString("genre-%1").arg(lane);
        genre.name = QString("匿名收藏 %1").arg(lane);
        library.genres.append(genre);
        for (int row = 0; row < 64; ++row) {
            Album album;
            album.id = QString("album-%1-%2").arg(lane).arg(row);
            album.title = QString("匿名专辑 %1 / Descender (gyp) 🎵").arg(row);
            album.artist = "Anonymous";
            album.genreId = genre.id;
            const int index = lane * 64 + row;
            album.coverUrl = index < 96 ? QUrl::fromLocalFile(paths[index])
                                        : QUrl("qrc:/assets/demo-covers/square.png");
            for (int trackIndex = 0; trackIndex < 3; ++trackIndex) {
                Track track;
                track.id = album.id + QString("-track-%1").arg(trackIndex);
                track.title = QString("匿名曲目 %1").arg(trackIndex);
                track.sourceUrl = QUrl::fromLocalFile(directory + "/anonymous.wav");
                album.tracks.append(track);
            }
            library.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(library));
}
class Driver
{
public:
    Driver(Snapshot input)
        : window(OffscreenSurface::Enabled ? &control : nullptr), surface(window, control)
    {
        verification::prepareWindow(window, true);
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        window.resize(1280, 720);
        surface.measurementEnabled = true;
        QObject::connect(&surface, &OffscreenSurface::frameMeasured, &window,
                         [&](QJsonObject costs) {
                             if (timings.size() >= 40000)
                                 overflow = true;
                             else {
                                 costs["stage"] = stage;
                                 costs["atNs"] = nowNs();
                                 timings.append(costs);
                             }
                         });
        QObject::connect(
                &window, &QQuickWindow::frameSwapped, &window,
                [&] {
                    const QMutexLocker lock(&swapMutex);
                    if (swapped.size() == 40000)
                        overflow = true;
                    else
                        swapped.append(nowNs());
                },
                Qt::DirectConnection);
        QObject::connect(&library, &LibrarySession::snapshotChanged, &selection, [&] {
            selection.setColumns(library.albums()->order().columns, library.snapshot());
        });
        QObject::connect(&selection, &SelectionController::selectionChanged, &library, [&] {
            library.navigate(selection.albumId());
        });
        QObject::connect(&library, &LibrarySession::navigationChanged, &selection, [&] {
            if (library.navigationAlbumId() != selection.albumId())
                selection.select(library.navigationAlbumId());
        });
        QObject::connect(&motion, &MotionController::reducedMotionChanged, &scene, [&] {
            scene.setReducedMotion(motion.reducedMotion());
            presentation.setReducedMotion(motion.reducedMotion());
            focus.setReducedMotion(motion.reducedMotion());
            panels.setReducedMotion(motion.reducedMotion());
        });
        QObject::connect(&presentation, &PresentationController::prepared, &detail, [&] {
            detail.prepare(library);
        });
        QObject::connect(&selection, &SelectionController::libraryChanged, &presentation,
                         &PresentationController::reset);
        QObject::connect(&opening, &OpeningController::started, &presentation, [&] {
            panels.close();
            focus.cancel();
            overview.setActive(false);
            presentation.reset();
        });
        overview.bind(&scene, &selection, &presentation);
        scene.setSelection(&selection);
        ruler.setInputRouter(&router);
        QObject::connect(&router, &InputRouter::inputReceived, &scene, &SceneAdapter::recordInput);
        QObject::connect(&ruler, &RulerController::inputReceived, &scene,
                         &SceneAdapter::recordInput);
        QObject::connect(motion.clock(), &FrameClock::frame, &router, [&](double, double) {
            router.flush();
        });
        QObject::connect(&router, &InputRouter::navigationCommitted, &scene,
                         &SceneAdapter::recordNavigationCommit);
        QObject::connect(&engine, &QQmlEngine::warnings, &window,
                         [&](const QList<QQmlError> &errors) {
                             for (const auto &error : errors)
                                 warnings.append(error.toString());
                         });
        startup.receive(std::move(input));
        createRoot();
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        surface.show();
    }
    ~Driver()
    {
        destroyRoot();
    }
    void setStage(QString value)
    {
        stage = std::move(value);
        qInfo().noquote() << "Startup stage:" << stage;
    }
    void createRoot()
    {
        QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/pages/MusicStage.qml"));
        root.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
                { { "librarySession", QVariant::fromValue(&library) },
                  { "motionController", QVariant::fromValue(&motion) },
                  { "sceneAdapter", QVariant::fromValue(&scene) },
                  { "startupCoordinator", QVariant::fromValue(&startup) },
                  { "openingController", QVariant::fromValue(&opening) },
                  { "rulerController", QVariant::fromValue(&ruler) },
                  { "inputRouter", QVariant::fromValue(&router) },
                  { "presentationController", QVariant::fromValue(&presentation) },
                  { "detailModel", QVariant::fromValue(&detail) },
                  { "panelController", QVariant::fromValue(&panels) },
                  { "trackFocusController", QVariant::fromValue(&focus) },
                  { "playbackController", QVariant::fromValue(&playback) },
                  { "settingsController", QVariant::fromValue(&settings) },
                  { "overviewController", QVariant::fromValue(&overview) },
                  { "offscreenRendering", OffscreenSurface::Enabled } })));
        if (!root)
            qWarning() << component.errors();
        require(bool(root), "Cannot create production lifecycle stage");
        root->setParentItem(window.contentItem());
        root->setSize(window.size());
        native = qobject_cast<rhi::SceneItem *>(find(root.get(), "nativeScene"));
        require(native, "Production scene missing");
        native->setTelemetryEnabled(true);
        QObject::connect(
                native, &rhi::SceneItem::frameSubmitted, root.get(), [&](QJsonObject sample) {
                    if (samples.size() >= 40000) {
                        overflow = true;
                        return;
                    }
                    if (sample["coalescedSamples"].toInt() != 0)
                        failure = "Telemetry samples lost";
                    lastResources = sample["resources"].toObject();
                    sample.remove("resources");
                    const auto bytes = lastResources["resizeOldPlusNewUpperBoundBytes"].toInteger();
                    peakBytes = qMax(peakBytes, bytes);
                    const auto plan = lastResources["preallocationPlan"].toObject();
                    if (plan != lastPlan) {
                        lastPlan = plan;
                        inventories.append(
                                QJsonObject{ { "stage", stage }, { "resources", lastResources } });
                    }
                    sample["stage"] = stage;
                    sample["covers"] = native->coverStatistics();
                    sample["clockFrame"] = qint64(motion.clock()->frameCount());
                    samples.append(sample);
                });
        router.attach(&window);
    }
    void destroyRoot()
    {
        router.attach(nullptr);
        QEventLoop invalidation;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&window, &QQuickWindow::sceneGraphInvalidated, &invalidation,
                         &QEventLoop::quit, Qt::QueuedConnection);
        QObject::connect(&deadline, &QTimer::timeout, &invalidation, &QEventLoop::quit);
        root.reset();
        native = nullptr;
        surface.hide();
        window.releaseResources();
        if (!OffscreenSurface::Enabled && rhi::sceneSubmissionStats().renderers.load() > 0) {
            deadline.start(10000);
            invalidation.exec();
        }
    }
    void wait(const std::function<bool()> &predicate, bool advance = false, int timeoutMs = 60000,
              bool paced = false, bool forceFrames = false)
    {
        if (predicate())
            return;
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        bool done = false, pending = false;
        qint64 nextTickNs = nowNs();
        const auto evaluate = [&] {
            pending = false;
            if (done)
                return;
            if (!native->errorText().isEmpty() || !failure.isEmpty()) {
                loop.quit();
                return;
            }
            if (predicate()) {
                done = true;
                loop.quit();
            } else if (advance || forceFrames) {
                if (advance && OffscreenSurface::Enabled)
                    motion.clock()->advance(1.0 / 60);
                surface.requestFrame();
            }
        };
        const auto schedule = [&] {
            if (pending || done)
                return;
            pending = true;
            // Local RenderControl replay has an explicit 60Hz cadence; it is not native VSYNC.
            if (paced && OffscreenSurface::Enabled) {
                const auto now = nowNs();
                nextTickNs = qMax(nextTickNs + 16666667, now);
                const int delay = int((nextTickNs - now + 999999) / 1000000);
                QTimer::singleShot(delay, Qt::PreciseTimer, &loop, evaluate);
            } else
                QMetaObject::invokeMethod(&loop, evaluate, Qt::QueuedConnection);
        };
        surface.connectFrames(&loop, schedule);
        if (!advance && !forceFrames) {
            QObject::connect(native, &rhi::SceneItem::scenePrepared, &loop, schedule);
            QObject::connect(&opening, &OpeningController::changed, &loop, schedule);
            QObject::connect(&playback, &PlaybackController::stateChanged, &loop, schedule);
            QObject::connect(&library, &LibrarySession::snapshotChanged, &loop, schedule);
        }
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(timeoutMs);
        surface.requestFrame();
        loop.exec();
        require(done, "Product lifecycle deadline");
    }
    void frames(int count, bool advance = false)
    {
        int rendered = 0;
        wait(
                [&] {
                    return ++rendered > count;
                },
                advance);
    }
    void ready()
    {
        wait([&] {
            if (!native)
                return false;
            const auto covers = native->coverStatistics();
            return covers["identities"].toInt() > 0 && covers["identities"] == covers["ready"]
                    && covers["selectedDetailed"].toBool() && !samples.isEmpty()
                    && samples.last().toObject()["atlasPending"].toInt(-1) == 0;
        });
    }
    void settle()
    {
        if (OffscreenSurface::Enabled) {
            for (int i = 0; i < 180; ++i)
                motion.clock()->advance(1.0 / 60);
        }
        ready();
        // A static item's last telemetry may be the final nonzero upload. A real preparation
        // acknowledgement proves the queue is empty, then yields a fresh zero-upload sample.
        bool acknowledged = false;
        const auto connection =
                QObject::connect(native, &rhi::SceneItem::scenePrepared, &window, [&] {
                    acknowledged = true;
                });
        native->restartPreparation();
        wait([&] {
            return acknowledged;
        });
        QObject::disconnect(connection);
        native->setPreparing(false);
        frames(3);
    }
    void resize(QSize size, double dpr)
    {
        window.resize(size);
        root->setSize(size);
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        frames(2);
    }
    void quiet(int milliseconds)
    {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QQuickRenderControl control;
    QQuickWindow window;
    OffscreenSurface surface;
    MotionController motion;
    LibrarySession library{ nullptr };
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OpeningController opening{ &motion, &scene, &selection };
    StartupCoordinator startup{ &library, &opening, motion.clock(), true };
    InputRouter router{ &selection };
    RulerController ruler{ &motion, &selection };
    DetailModel detail;
    PresentationController presentation{ motion.clock(), detailPorts(scene, selection, library) };
    PanelController panels{ motion.clock() };
    TrackFocusController focus{ &library, &presentation, motion.clock() };
    PlaybackController playback{ &library };
    SettingsController settings{ nullptr, &library, &motion, &scene, &selection };
    OverviewController overview{ &motion };
    QQmlEngine engine;
    std::unique_ptr<QQuickItem> root;
    rhi::SceneItem *native = nullptr;
    QString stage = "cold-start";
    QStringList warnings;
    QString failure;
    QJsonArray samples, timings, inventories;
    QJsonObject lastResources, lastPlan;
    qint64 peakBytes = 0;
    QMutex swapMutex;
    QVector<qint64> swapped;
    std::atomic_bool overflow{ false };
};
}
int reviewStartup(QGuiApplication &, const QString &output, double seconds,
                  const QJsonObject &bootstrap, const QString &scenario)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 120 || (seconds > 0 && seconds < 12))
        return 2;
    const QStringList allStages{ "day-wave",     "day-ruler",    "day-detail",     "day-switch",
                                 "day-overview", "day-search",   "night-wave",     "night-ruler",
                                 "night-detail", "night-switch", "night-overview", "night-search" };
    if (scenario != "matrix" && !allStages.contains(scenario))
        return 2;
    QDir().mkpath(output);
    QJsonObject report{ { "qtVersion", qVersion() },
                        { "os", QSysInfo::prettyProductName() },
                        { "architecture", QSysInfo::currentCpuArchitecture() },
                        { "normalVsync", QJsonValue::Null },
                        { "inputToPhotonMs", QJsonValue::Null },
                        { "measuredVramBytes", QJsonValue::Null } };
    report["bootstrap"] = bootstrap;
    report["scenario"] = scenario;
    QJsonArray checks, resources, actions;
    bool passed = true;
    const auto check = [&](const char *name, bool value) {
        checks.append(QJsonObject{ { "name", name }, { "passed", value } });
        passed &= value;
        if (!value)
            qWarning("Startup check failed: %s", name);
    };
    try {
        QTemporaryDir anonymous;
        require(anonymous.isValid(), "Cannot create anonymous fixture directory");
        QFile audio(anonymous.path() + "/anonymous.wav");
        require(audio.open(QIODevice::WriteOnly), "Cannot create anonymous audio");
        const auto bytes = test::wave(600000);
        require(audio.write(bytes) == bytes.size(), "Cannot write anonymous audio");
        audio.close();
        const auto input = fixture(anonymous.path());
        QCryptographicHash inputHash(QCryptographicHash::Sha256);
        for (int i = 0; i < 96; ++i) {
            QFile file(anonymous.path() + QString("/anonymous-%1.jpg").arg(i));
            require(file.open(QIODevice::ReadOnly), "Cannot hash anonymous input");
            require(inputHash.addData(&file), "Cannot hash cover bytes");
        }
        inputHash.addData(bytes);
        report["fixtureSha256"] = QString::fromLatin1(inputHash.result().toHex());
        QElapsedTimer preparation;
        preparation.start();
        Driver d(input);
        struct SaveEvidence
        {
            std::function<void()> save;
            ~SaveEvidence()
            {
                save();
            }
        } evidence{ [&] {
            report["samples"] = d.samples;
            report["quickTimings"] = d.timings;
            report["inventories"] = d.inventories;
            report["warnings"] = QJsonArray::fromStringList(d.warnings);
            report["lastStage"] = d.stage;
        } };
        d.opening.replay();
        d.wait([&] {
            return d.opening.film();
        });
        report["preparationMs"] = preparation.nsecsElapsed() / 1e6;
        check("initial-consistent-snapshot",
              d.library.snapshot()->albums.size() == 576
                      && d.startup.statistics()["appliedSnapshots"].toInt() == 1);
        check("readiness-does-not-consume-film", d.opening.time() == 0);
        d.opening.skip();
        d.motion.setReducedMotion(true);
        d.settle();
        const auto baselineResources = d.lastResources;
        report["baselineResources"] = baselineResources;
        report["initialCovers"] = d.native->coverStatistics();
        const auto releaseCheck = [&] {
            const auto value = counters();
            return value["sceneRenderers"].toInt() == 0 && value["rulerRenderers"].toInt() == 0
                    && value["openingRenderers"].toInt() == 0 && value["textNodes"].toInt() == 0;
        };
        const auto resourceSnapshot = [&] {
            return QJsonObject{
                { "stage", d.stage },
                { "resources", d.lastResources },
                { "covers", d.native->coverStatistics() },
                { "counters", counters() },
                { "startup", d.startup.statistics() },
                { "title", QJsonObject::fromVariantMap(d.native->titleReel()->diagnostics()) },
                { "transport",
                  QJsonObject::fromVariantMap(d.native->transportReel()->diagnostics()) }
            };
        };
        resources.append(resourceSnapshot());
        const auto idle = counters();
        const auto idleSubmission = d.native->submission();
        const auto idleFrames = d.motion.clock()->frameCount();
        d.quiet(120);
        check("idle-zero-scene-submission", d.native->submission() == idleSubmission);
        check("idle-zero-visual-step", d.motion.clock()->frameCount() == idleFrames);
        check("idle-zero-canvas-repaint",
              counters()["rulerPaints"] == idle["rulerPaints"]
                      && counters()["openingPaints"] == idle["openingPaints"]);
        // Change state from a queued callback after a completed render. Its dirty revision must survive.
        bool late = false;
        QMetaObject::invokeMethod(
                d.native,
                [&] {
                    d.scene.lighting()->setNight(true);
                    d.selection.moveRow(1);
                    late = true;
                },
                Qt::QueuedConnection);
        d.wait([&] {
            return late && d.native->input().scene.revision == d.scene.frame().revision
                    && d.native->submission() > idleSubmission;
        });
        check("late-invalidation-submits-current-revision", late);
        d.settle();
        // Real public media path, muted anonymous WAV. Preserve source/intent across visual lifetimes.
        d.playback.setVolume(0);
        const auto albumId = d.library.snapshot()->albums[0].id;
        const auto trackId = d.library.snapshot()->albums[0].tracks[0].id;
        check("anonymous-public-media-command", d.playback.playTrack(albumId, trackId));
        d.playback.pause();
        const auto sourceRevision = d.playback.sourceRevision();
        const auto source = d.playback.source();
        bool requested = d.playback.requested();
        d.setStage("hidden-snapshots");
        d.motion.clock()->setApplicationVisible(false);
        d.motion.clock()->setActive(false);
        const auto hiddenSubmission = d.native->submission();
        const auto hiddenRenders = rhi::sceneSubmissionStats().rendered.load();
        const QJsonValue hiddenStarts =
                d.native->coverStatistics()["cache"].toObject()["decodeStarts"];
        const auto hiddenApplied = d.startup.statistics()["appliedSnapshots"].toInt();
        auto replacement = *input;
        for (int i = 0; i < 10; ++i) {
            replacement.albums[0].title = QString("latest-%1").arg(i);
            d.startup.receive(std::make_shared<const LibrarySnapshot>(replacement));
        }
        d.selection.moveRow(2);
        d.scene.lighting()->setNight(false);
        d.native->ensurePolished();
        d.quiet(150);
        check("hidden-no-snapshot-notifications",
              d.startup.statistics()["appliedSnapshots"].toInt() == hiddenApplied);
        check("hidden-no-submit-or-render",
              d.native->submission() == hiddenSubmission
                      && rhi::sceneSubmissionStats().rendered.load() == hiddenRenders);
        check("hidden-no-new-decode",
              d.native->coverStatistics()["cache"].toObject()["decodeStarts"] == hiddenStarts);
        d.motion.clock()->setApplicationVisible(true);
        d.motion.clock()->setActive(true);
        QSignalSpy resumed(d.motion.clock(), &FrameClock::frame);
        d.motion.clock()->advance(100);
        check("resume-discards-first-interval",
              resumed.size() == 1 && resumed[0][0].toDouble() == 0);
        d.frames(2);
        d.settle();
        check("resume-one-latest-snapshot",
              d.startup.statistics()["appliedSnapshots"].toInt() == hiddenApplied + 1
                      && d.library.snapshot()->albums[0].title == "latest-9");
        check("resume-presents-latest-state",
              d.native->input().scene.revision == d.scene.frame().revision);
        const auto preservesMedia = [&] {
            return d.playback.trackId() == trackId && d.playback.source() == source
                    && d.playback.sourceRevision() == sourceRevision
                    && d.playback.requested() == requested
                    && (requested ? d.playback.playing() : !d.playback.playing());
        };
        check("hidden-preserves-current-media-and-intent", preservesMedia());
        d.setStage("switch-stress");
        d.presentation.open();
        d.wait(
                [&] {
                    return d.presentation.detailEnabled();
                },
                true);
        for (int i = 0; i < 30; ++i) {
            const auto &albums = d.library.snapshot()->albums;
            d.presentation.select(albums[i % 2 ? 1 : 2].id, true);
            d.wait(
                    [&] {
                        return d.presentation.detailEnabled() && !d.presentation.pendingSelection();
                    },
                    true);
            d.settle();
            resources.append(resourceSnapshot());
            check("bounded-switch-pool",
                  d.native->coverStatistics()["owned"].toInt() <= 4
                          && d.native->input().scene.instances.size() <= ArchivePool::capacity + 4);
        }
        report["switches"] = 30;
        d.presentation.back();
        d.wait(
                [&] {
                    return d.presentation.archiveEnabled();
                },
                true);
        d.settle();
        const QJsonValue before = d.native->coverStatistics()["cache"].toObject()["decodeStarts"];
        d.setStage("cache-hit");
        for (int i = 0; i < 6; ++i) {
            d.selection.select(d.library.snapshot()->albums[i % 2 ? 1 : 2].id);
            d.settle();
        }
        check("cache-hits-no-redecode",
              d.native->coverStatistics()["cache"].toObject()["decodeStarts"] == before);
        d.frames(2);
        check("settled-zero-upload",
              d.samples.last().toObject()["atlasUploadBytes"].toInt() == 0
                      && d.samples.last().toObject()["selectedUploadBytes"].toInt() == 0);
        const QJsonValue stableBytes = d.lastResources["estimatedCurrentBytes"];
        for (int cycle = 0; cycle < 10; ++cycle) {
            d.setStage(QString("rebuild-%1").arg(cycle));
            if (cycle == 5) {
                d.playback.toggle();
                d.wait([&] {
                    return d.playback.playing();
                });
                requested = true;
            }
            d.destroyRoot();
            check("rebuild-releases-renderers", releaseCheck());
            d.createRoot();
            d.motion.clock()->setActive(true);
            d.surface.show();
            d.settle();
            resources.append(resourceSnapshot());
            check("rebuild-preserves-media-and-intent", preservesMedia());
            check("rebuild-resources-return-to-baseline",
                  d.lastResources["estimatedCurrentBytes"] == stableBytes);
            check("rebuild-retained-cover-budget",
                  d.native->coverStatistics()["atlasRetainedBytes"].toInteger()
                          <= qint64(ArchivePool::capacity + 4) * 256 * 256 * 4);
        }
        report["rebuilds"] = 10;
        d.playback.stop();
        d.motion.setReducedMotion(false);
        d.motion.clock()->setActive(true);
        // Exercise real ruler/controller/Quick text, panels, source search, opening and target invalidation.
        const auto drawBefore = counters();
        d.setStage("ruler-reverse");
        for (int i = 0; i < 24; ++i) {
            QKeyEvent key(QEvent::KeyPress, i < 12 ? Qt::Key_Down : Qt::Key_Up, Qt::NoModifier);
            QCoreApplication::sendEvent(&d.window, &key);
            d.frames(2, true);
        }
        d.setStage("overview");
        d.overview.setActive(true);
        d.frames(45, true);
        d.overview.setActive(false);
        d.setStage("search-panels");
        d.panels.open("search");
        d.library.search("匿名");
        d.library.search("匿名专辑 2");
        d.library.search("");
        d.frames(25, true);
        d.panels.close();
        d.frames(25, true);
        d.selection.setRealistic(true);
        d.frames(10, true);
        d.selection.setRealistic(false);
        d.setStage("opening-replay");
        d.opening.replay();
        d.wait([&] {
            return d.opening.film();
        });
        d.frames(40, true);
        d.motion.clock()->setApplicationVisible(false);
        d.motion.clock()->setActive(false);
        d.destroyRoot();
        check("opaque-rebuild-releases-renderers", releaseCheck());
        d.createRoot();
        d.surface.show();
        const auto opaqueBefore = rhi::sceneSubmissionStats().rendered.load();
        d.motion.clock()->setApplicationVisible(true);
        d.frames(2);
        check("opaque-rebuild-submits-only-clear",
              d.native->input().sceneOccluded
                      && rhi::sceneSubmissionStats().rendered.load() == opaqueBefore);
        report["opaqueRootRebuilds"] = 1;
        d.motion.clock()->setActive(true);
        d.motion.clock()->advance(30);
        d.opening.skip();
        d.frames(50, true);
        for (double dpr : { 1.0, 1.5, 2.0 }) {
            d.setStage("theme-dpr-resize");
            d.scene.lighting()->setNight(!d.scene.lighting()->night());
            d.resize({ 960, 900 }, dpr);
            d.frames(45, true);
        }
        d.resize({ 1280, 720 }, d.window.devicePixelRatio());
        check("actual-ruler-and-opening-drawing",
              counters()["rulerPaints"].toInteger() > drawBefore["rulerPaints"].toInteger()
                      && counters()["openingPaints"].toInteger()
                              > drawBefore["openingPaints"].toInteger());
        resources.append(resourceSnapshot());
        if (seconds > 0) {
            d.resize({ 1920, 1080 }, d.window.devicePixelRatio());
            d.scene.quality()->setPreset("original");
            d.settle();
            const auto stages = scenario == "matrix" ? allStages : QStringList{ scenario };
            QElapsedTimer sampling;
            int stageIndex = -1, eventIndex = -1;
            sampling.start();
            const auto replay = [&](double duration) {
                const auto elapsed = sampling.nsecsElapsed() / 1e9;
                if (elapsed >= duration)
                    return true;
                const int index =
                        qMin(stages.size() - 1, qsizetype(elapsed / duration * stages.size()));
                const int sourceIndex = allStages.indexOf(stages[index]);
                if (index != stageIndex) {
                    stageIndex = index;
                    d.setStage(stages[index]);
                    d.presentation.back();
                    d.panels.close();
                    d.overview.setActive(false);
                    d.scene.lighting()->setNight(sourceIndex >= 6);
                    if (sourceIndex % 6 == 2 || sourceIndex % 6 == 3)
                        d.presentation.open();
                    if (sourceIndex % 6 == 4)
                        d.overview.setActive(true);
                    if (sourceIndex % 6 == 5)
                        d.panels.open("search");
                    actions.append(QJsonObject{ { "stage", d.stage },
                                                { "elapsedSeconds", elapsed },
                                                { "atNs", nowNs() } });
                }
                const int event = int(elapsed * 3);
                if (event != eventIndex) {
                    eventIndex = event;
                    const int local = sourceIndex % 6;
                    if (local == 1) {
                        QKeyEvent key(QEvent::KeyPress, event % 8 < 4 ? Qt::Key_Down : Qt::Key_Up,
                                      Qt::NoModifier);
                        QCoreApplication::sendEvent(&d.window, &key);
                    } else if (local == 3 && d.presentation.detailEnabled()) {
                        d.presentation.select(d.library.snapshot()->albums[event % 2 + 1].id, true);
                    } else if (local == 5) {
                        d.library.search(event % 3 == 0           ? ""
                                                 : event % 3 == 1 ? "匿名"
                                                                  : "匿名专辑 2");
                    }
                }
                return false;
            };
            if (scenario != "matrix") {
                d.wait(
                        [&] {
                            return replay(1);
                        },
                        true, 30000, true);
                eventIndex = -1;
            }
            const auto startSample = d.samples.size(), startTiming = d.timings.size();
            sampling.restart();
            report["samplingStartedNs"] = nowNs();
            d.wait(
                    [&] {
                        return replay(seconds);
                    },
                    true, int(seconds * 1000 + 30000), true);
            report["samplingEndedNs"] = nowNs();
            report["samplingSeconds"] = sampling.nsecsElapsed() / 1e9;
            report["sampleStartIndex"] = startSample;
            report["quickTimingStartIndex"] = startTiming;
            report["fixedStages"] = QJsonArray::fromStringList(stages);
        }
        d.setStage("final-release");
        if (OffscreenSurface::Enabled)
            require(d.surface.grab().save(output + "/product-restored.png"),
                    "Cannot save owned product output");
        report["finalCovers"] = d.native->coverStatistics();
        report["peakDescriptorReplacementBytes"] = d.peakBytes;
        report["startup"] = d.startup.statistics();
        report["offscreen"] = d.surface.diagnostics();
        report["windowPolicy"] = verification::windowPolicy(d.window);
        report["dpr"] = d.window.effectiveDevicePixelRatio();
        report["backend"] = int(d.window.rendererInterface()->graphicsApi());
        report["adapters"] = rhi::adapterInventory();
        if (auto *api = d.surface.rhi()) {
            const auto info = api->driverInfo();
            report["driver"] = QJsonObject{ { "deviceName", QString::fromUtf8(info.deviceName) },
                                            { "deviceId", qint64(info.deviceId) },
                                            { "vendorId", qint64(info.vendorId) },
                                            { "deviceType", int(info.deviceType) } };
        }
        report["processFixedAssets"] = rhi::preparedAssetStatistics();
        report["refreshHz"] = d.window.screen() ? d.window.screen()->refreshRate() : 0;
        report["swapIntervalRequested"] = d.window.format().swapInterval();
        report["qsgNoVsyncRequested"] = qEnvironmentVariable("QSG_NO_VSYNC");
        report["normalPresentationMeasured"] = !OffscreenSurface::Enabled;
        report["measurementScope"] = OffscreenSurface::Enabled
                ? "Production MusicStage into hidden RenderControl; manual shared-clock event replay at requested 60Hz; GUI polish, sync, Quick render, endFrame wait and RHI submission are separate local costs. No native presentation or physical input measurement."
                : "Production MusicStage native FrameAnimation; actual frameSwapped event timestamps; physical display/input-to-photon and compositor latency unavailable.";
        QMap<QString, QVector<double>> cpu, sync, gpu, inputLatency;
        QSet<quint64> inputs;
        for (qsizetype i = report["sampleStartIndex"].toInt(); i < d.samples.size(); ++i) {
            const auto sample = d.samples[i].toObject();
            const auto name = sample["stage"].toString();
            cpu[name].append(sample["cpuSubmitMs"].toDouble());
            sync[name].append(sample["synchronizationMs"].toDouble());
            if (sample["gpuFrameMs"].isDouble())
                gpu[name].append(sample["gpuFrameMs"].toDouble());
            const auto revision = quint64(sample["inputRevision"].toInteger());
            if (revision && !inputs.contains(revision)) {
                inputs.insert(revision);
                inputLatency[name].append(
                        (sample["submittedNs"].toInteger() - sample["inputReceivedNs"].toInteger())
                        / 1e6);
            }
        }
        QJsonObject stages;
        for (auto it = cpu.cbegin(); it != cpu.cend(); ++it)
            stages[it.key()] =
                    QJsonObject{ { "cpuSubmit", distribution(it.value()) },
                                 { "sync", distribution(sync[it.key()]) },
                                 { "gpu", distribution(gpu[it.key()]) },
                                 { "inputToQtSubmit", distribution(inputLatency[it.key()]) } };
        report["stages"] = stages;
        QJsonObject quickCosts;
        const QStringList costNames{ "polishMs", "beginFrameMs", "syncMs", "quickRenderMs",
                                     "endFrameMs" };
        QMap<QString, QMap<QString, QVector<double>>> costs;
        for (qsizetype i = report["quickTimingStartIndex"].toInt(); i < d.timings.size(); ++i) {
            const auto timing = d.timings[i].toObject();
            for (const auto &name : costNames)
                costs[timing["stage"].toString()][name].append(timing[name].toDouble());
        }
        for (auto it = costs.cbegin(); it != costs.cend(); ++it) {
            QJsonObject entry;
            for (const auto &name : costNames)
                entry[name] = distribution(it.value().value(name));
            quickCosts[it.key()] = entry;
        }
        report["quickCosts"] = quickCosts;
        QVector<double> intervals;
        QMap<QString, QVector<double>> presentation;
        {
            const QMutexLocker lock(&d.swapMutex);
            for (qsizetype i = 1; i < d.swapped.size(); ++i) {
                const auto previous = d.swapped[i - 1], current = d.swapped[i];
                if (previous < report["samplingStartedNs"].toInteger()
                    || current > report["samplingEndedNs"].toInteger())
                    continue;
                const double dt = (current - previous) / 1e6;
                intervals.append(dt);
                for (qsizetype j = actions.size() - 1; j >= 0; --j) {
                    const auto action = actions[j].toObject();
                    // Separate each transition/warmup second from steady target gates.
                    if (previous >= action["atNs"].toInteger() + 1000000000) {
                        presentation[action["stage"].toString()].append(dt);
                        break;
                    }
                    if (previous >= action["atNs"].toInteger())
                        break;
                }
            }
        }
        report["qtPresentationIntervals"] = distribution(intervals);
        bool nativeValid = !OffscreenSurface::Enabled && seconds == 120
                && report["samplingSeconds"].toDouble() >= 120 && scenario != "matrix"
                && presentation.size() == 1;
        bool numericPass = true;
        int stableOver50 = 0;
        QJsonObject presentationStages;
        for (auto it = presentation.cbegin(); it != presentation.cend(); ++it) {
            const auto stats = distribution(it.value());
            nativeValid &= it.value().size() >= 30;
            stableOver50 += stats["over50Ms"].toInt();
            numericPass &= stats["p95Ms"].toDouble() <= 20
                    && stats["p99Ms"].toDouble() <= 1000.0 / 30 && stats["over50Ms"].toInt() <= 1;
            presentationStages[it.key()] = stats;
        }
        numericPass &= stableOver50 <= 1;
        report["nativeSamplingValid"] = nativeValid;
        report["targetRefreshHz"] = 60;
        report["offscreenReplayPeriodMsRequested"] =
                OffscreenSurface::Enabled ? QJsonValue(1000.0 / 60) : QJsonValue::Null;
        report["steadyWarmupSecondsPerStage"] = 1;
        report["qtPresentationByStage"] = presentationStages;
        report["performanceGateEnforced"] = !OffscreenSurface::Enabled
                && qEnvironmentVariableIntValue("QSG_NO_VSYNC") != 1
                && d.window.effectiveDevicePixelRatio() == 1;
        report["performanceGatePassed"] = OffscreenSurface::Enabled
                ? QJsonValue::Null
                : QJsonValue(nativeValid && numericPass);
        report["samplingComplete"] =
                !d.overflow && (seconds == 0 || stages.size() >= (scenario == "matrix" ? 12 : 1));
        report["performanceAcceptance"] = OffscreenSurface::Enabled
                ? "unavailable-native-presentation"
                : scenario == "matrix" ? "matrix-local-diagnostic-only"
                : !nativeValid         ? "invalid-native-sample"
                : numericPass          ? "native-qt-presentation-targets-met"
                                       : "native-qt-presentation-targets-failed";
        report["samples"] = d.samples;
        report["quickTimings"] = d.timings;
        report["inventories"] = d.inventories;
        report["actions"] = actions;
        check("bounded-telemetry-no-overflow", !d.overflow);
        check("no-qml-warnings", d.warnings.isEmpty());
        report["warnings"] = QJsonArray::fromStringList(d.warnings);
        check("window-policy", verification::windowPolicySatisfied(d.window));
        d.destroyRoot();
        check("final-release-zero-owned-renderers", releaseCheck());
        report["releasedCounters"] = counters();
    } catch (const std::exception &error) {
        passed = false;
        report["error"] = QString::fromUtf8(error.what());
        qWarning("Startup audit failed: %s", error.what());
    }
    report["functionalPassed"] = passed;
    report["checks"] = checks;
    report["resourceCheckpoints"] = resources;
    QSaveFile file(output + "/report.json");
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson()) < 0
        || !file.commit())
        return 2;
    return passed ? 0 : 1;
}
