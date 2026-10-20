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
#include <QTest>
#include <QScopeGuard>
#include <algorithm>
#include <span>
#include <memory>
#include <chrono>
#include <stdexcept>

using namespace Qt::StringLiterals;

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
        return { { u"count"_s, 0 },
                 { u"p95Ms"_s, QJsonValue::Null },
                 { u"p99Ms"_s, QJsonValue::Null } };
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
    return { { u"count"_s, values.size() },
             { u"p50Ms"_s, percentile(.5) },
             { u"p95Ms"_s, percentile(.95) },
             { u"p99Ms"_s, percentile(.99) },
             { u"maxMs"_s, values.last() },
             { u"over50Ms"_s, over(50) },
             { u"overOnePeriod"_s, over(period) },
             { u"overTwoPeriods"_s, over(2 * period) },
             { u"overThreePeriods"_s, over(3 * period) },
             { u"longestOverTwoPeriods"_s, longest } };
}
QJsonObject counters()
{
    return { { u"sceneRenders"_s, qint64(rhi::sceneSubmissionStats().rendered.load()) },
             { u"sceneRenderers"_s, rhi::sceneSubmissionStats().renderers.load() },
             { u"rulerPaints"_s, qint64(rulerInkStats().paints.load()) },
             { u"rulerSyncs"_s, qint64(rulerInkStats().syncs.load()) },
             { u"rulerPaintNs"_s, qint64(rulerInkStats().paintNs.load()) },
             { u"rulerSyncNs"_s, qint64(rulerInkStats().syncNs.load()) },
             { u"rulerRenderers"_s, rulerInkStats().renderers.load() },
             { u"openingPaints"_s, qint64(openingInkStats().paints.load()) },
             { u"openingSyncs"_s, qint64(openingInkStats().syncs.load()) },
             { u"openingPaintNs"_s, qint64(openingInkStats().paintNs.load()) },
             { u"openingSyncNs"_s, qint64(openingInkStats().syncNs.load()) },
             { u"openingRenderers"_s, openingInkStats().renderers.load() },
             { u"textNodes"_s, ReelInk::liveNodes() },
             { u"textGeometryUpdates"_s, qint64(ReelInk::geometryUpdates()) },
             { u"textGeometryReuses"_s, qint64(ReelInk::geometryReuses()) } };
}
Snapshot fixture(const QString &directory, bool largeCovers = true, bool tracks = true)
{
    LibrarySnapshot library;
    QStringList paths;
    QImage image;
    if (largeCovers)
        image = QImage(2048, 2048, QImage::Format_RGB32);
    for (int i = 0; i < (largeCovers ? 96 : 0); ++i) {
        image.fill(QColor::fromHsv(i * 359 / 96, 140, 210));
        const auto path = directory + u"/anonymous-%1.jpg"_s.arg(i);
        require(image.save(path, "JPEG", 82), "Cannot create anonymous large cover");
        paths.append(path);
    }
    for (int lane = 0; lane < 9; ++lane) {
        Genre genre;
        genre.id = u"genre-%1"_s.arg(lane);
        genre.name = u"\u533f\u540d\u6536\u85cf %1"_s.arg(lane);
        library.genres.append(genre);
        for (int row = 0; row < 64; ++row) {
            Album album;
            album.id = u"album-%1-%2"_s.arg(lane).arg(row);
            album.title = u"\u533f\u540d\u4e13\u8f91 %1 / Descender (gyp) \U0001f3b5"_s.arg(row);
            album.artist = "Anonymous"_L1;
            album.genreId = genre.id;
            const int index = lane * 64 + row;
            album.coverUrl = index < paths.size() ? QUrl::fromLocalFile(paths[index])
                                                  : QUrl(u"qrc:/assets/demo-covers/square.png"_s);
            for (int trackIndex = 0; trackIndex < (tracks ? 3 : 0); ++trackIndex) {
                Track track;
                track.id = album.id + u"-track-%1"_s.arg(trackIndex);
                track.title = u"\u533f\u540d\u66f2\u76ee %1"_s.arg(trackIndex);
                track.sourceUrl = QUrl::fromLocalFile(directory + "/anonymous.wav"_L1);
                album.tracks.append(track);
            }
            library.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(library));
}
class AuditWindow final : public QQuickWindow
{
public:
    using QQuickWindow::QQuickWindow;
    QJsonArray events;
    bool eventOverflow = false;
    void record(const QString &kind)
    {
        if (!events.isEmpty() && kind != "sampling-start"_L1) {
            const auto previous = events.last().toObject();
            if (previous[u"exposed"_s].toBool() == isExposed()
                && previous[u"visible"_s].toBool() == isVisible()
                && previous[u"state"_s].toInt() == int(windowState())
                && previous[u"applicationState"_s].toInt()
                        == int(QGuiApplication::applicationState()))
                return;
        }
        if (events.size() == 512) {
            eventOverflow = true;
            return;
        }
        events.append(
                QJsonObject{ { u"kind"_s, kind },
                             { u"atNs"_s, nowNs() },
                             { u"exposed"_s, isExposed() },
                             { u"visible"_s, isVisible() },
                             { u"state"_s, int(windowState()) },
                             { u"applicationState"_s, int(QGuiApplication::applicationState()) } });
    }

protected:
    bool event(QEvent *event) override
    {
        const bool result = QQuickWindow::event(event);
        switch (event->type()) {
        case QEvent::Expose:
            record(u"expose"_s);
            break;
        case QEvent::Show:
            record(u"show"_s);
            break;
        case QEvent::Hide:
            record(u"hide"_s);
            break;
        case QEvent::WindowStateChange:
            record(u"state"_s);
            break;
        default:
            break;
        }
        return result;
    }
};
class Driver
{
public:
    Driver(Snapshot input, bool windowLifecycle = false)
        : window(OffscreenSurface::Enabled ? &control : nullptr), surface(window, control)
    {
        verification::prepareWindow(
                window,
                !windowLifecycle && qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") != 1);
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged, &engine,
                         [&](Qt::ApplicationState) {
                             window.record(u"application-state"_s);
                         });
        window.resize(1280, 720);
        surface.measurementEnabled = !windowLifecycle;
        QObject::connect(&surface, &OffscreenSurface::frameMeasured, &window,
                         [&](QJsonObject costs) {
                             if (timings.size() >= 40000)
                                 overflow = true;
                             else {
                                 costs[u"stage"_s] = stage;
                                 costs[u"atNs"_s] = nowNs();
                                 timings.append(costs);
                             }
                         });
        QObject::connect(
                &window, &QQuickWindow::frameSwapped, &window,
                [samples = presented] {
                    const QMutexLocker lock(&samples->mutex);
                    if (samples->times.size() == 40000)
                        samples->overflow = true;
                    else
                        samples->times.append(nowNs());
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
        QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/pages/MusicStage.qml"_s));
        root.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
                { { u"librarySession"_s, QVariant::fromValue(&library) },
                  { u"motionController"_s, QVariant::fromValue(&motion) },
                  { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
                  { u"startupCoordinator"_s, QVariant::fromValue(&startup) },
                  { u"openingController"_s, QVariant::fromValue(&opening) },
                  { u"rulerController"_s, QVariant::fromValue(&ruler) },
                  { u"inputRouter"_s, QVariant::fromValue(&router) },
                  { u"presentationController"_s, QVariant::fromValue(&presentation) },
                  { u"detailModel"_s, QVariant::fromValue(&detail) },
                  { u"panelController"_s, QVariant::fromValue(&panels) },
                  { u"trackFocusController"_s, QVariant::fromValue(&focus) },
                  { u"playbackController"_s, QVariant::fromValue(&playback) },
                  { u"settingsController"_s, QVariant::fromValue(&settings) },
                  { u"overviewController"_s, QVariant::fromValue(&overview) },
                  { u"offscreenRendering"_s, OffscreenSurface::Enabled } })));
        if (!root)
            qWarning() << component.errors();
        require(bool(root), "Cannot create production lifecycle stage");
        root->setParentItem(window.contentItem());
        root->setSize(window.size());
        native = qobject_cast<rhi::SceneItem *>(find(root.get(), u"nativeScene"_s));
        require(native, "Production scene missing");
        native->setTelemetryEnabled(true);
        QObject::connect(
                native, &rhi::SceneItem::frameSubmitted, root.get(), [&](QJsonObject sample) {
                    if (samples.size() >= 40000) {
                        overflow = true;
                        return;
                    }
                    if (sample[u"coalescedSamples"_s].toInt() != 0)
                        failure = "Telemetry samples lost"_L1;
                    lastResources = sample[u"resources"_s].toObject();
                    sample.remove(u"resources"_s);
                    const auto bytes =
                            lastResources[u"resizeOldPlusNewUpperBoundBytes"_s].toInteger();
                    peakBytes = qMax(peakBytes, bytes);
                    const auto plan = lastResources[u"preallocationPlan"_s].toObject();
                    if (plan != lastPlan) {
                        lastPlan = plan;
                        inventories.append(QJsonObject{ { u"stage"_s, stage },
                                                        { u"resources"_s, lastResources } });
                    }
                    sample[u"stage"_s] = stage;
                    sample[u"covers"_s] = native->coverStatistics();
                    sample[u"clockFrame"_s] = qint64(motion.clock()->frameCount());
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
        if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic"_ba
            && window.isExposed()) {
            // Basic keeps deferred QSG deletion until a frame with a current context.
            QSignalSpy cleanup(&window, &QQuickWindow::frameSwapped);
            surface.requestFrame();
            if (!QTest::qWaitFor(
                        [&] {
                            return !cleanup.isEmpty();
                        },
                        5000))
                failure = u"No native cleanup frame"_s;
        }
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
            } else if ((advance && OffscreenSurface::Enabled) || forceFrames) {
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
            QObject::connect(motion.clock(), &FrameClock::activeChanged, &loop, schedule);
            QObject::connect(motion.clock(), &FrameClock::workPendingChanged, &loop, schedule);
        }
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(timeoutMs);
        surface.requestFrame();
        loop.exec();
        if (!done)
            qWarning() << "Lifecycle wait state:" << stage << "clock-active"
                       << motion.clock()->active() << "pending" << motion.clock()->workPending()
                       << "opening" << opening.active() << "phase" << presentation.phase()
                       << "failure" << failure;
        require(done, "Product lifecycle deadline");
    }
    void frames(int count, bool advance = false)
    {
        int rendered = 0;
        wait(
                [&] {
                    return ++rendered > count;
                },
                advance, 60000, false, true);
    }
    void ready()
    {
        wait([&] {
            if (!native)
                return false;
            const auto covers = native->coverStatistics();
            return covers[u"identities"_s].toInt() > 0
                    && covers[u"identities"_s] == covers[u"ready"_s]
                    && covers[u"selectedDetailed"_s].toBool() && !samples.isEmpty()
                    && samples.last().toObject()[u"atlasPending"_s].toInt(-1) == 0;
        });
    }
    void settle()
    {
        if (OffscreenSurface::Enabled) {
            for (int i = 0; i < 180; ++i)
                motion.clock()->advance(1.0 / 60);
        }
        ready();
        // Continuous native animation can supersede preparation's exact submission token.
        // A fresh action receipt instead proves actual zero-upload submission without freezing it.
        if (!OffscreenSurface::Enabled && !motion.reducedMotion()) {
            scene.recordInput();
            const auto receipt = qint64(scene.frame().inputRevision);
            wait([&] {
                if (samples.isEmpty())
                    return false;
                const auto sample = samples.last().toObject();
                return sample[u"inputRevision"_s].toInteger() == receipt
                        && sample[u"atlasPending"_s].toInt(-1) == 0
                        && sample[u"atlasUploadBytes"_s].toInteger(-1) == 0
                        && sample[u"selectedUploadBytes"_s].toInteger(-1) == 0;
            });
            frames(3);
            return;
        }
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
    // Callback storage outlives the engine, render surface and window teardown.
    QString stage = u"cold-start"_s;
    QStringList warnings;
    QString failure;
    QJsonArray samples, timings, inventories;
    QJsonObject lastResources, lastPlan;
    qint64 peakBytes = 0;
    struct PresentationSamples
    {
        QMutex mutex;
        QVector<qint64> times;
        QVector<qint64> synchronized;
        std::atomic_bool overflow{ false };
    };
    std::shared_ptr<PresentationSamples> presented = std::make_shared<PresentationSamples>();
    std::atomic_bool overflow{ false };

    QQuickRenderControl control;
    AuditWindow window;
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
};
void verifyNativeWindowLifecycle(Driver &d, const std::function<void(const char *, bool)> &check,
                                 QJsonArray &transitions)
{
    const auto trackId = d.playback.trackId();
    const auto source = d.playback.source();
    const auto revision = d.playback.sourceRevision();
    const auto preservesMedia = [&](bool playing) {
        return d.playback.trackId() == trackId && d.playback.source() == source
                && d.playback.sourceRevision() == revision && d.playback.requested() == playing
                && (playing ? d.playback.playing() : !d.playback.playing());
    };
    require(!OffscreenSurface::Enabled, "Native lifecycle requires a visible window");
    check("first-native-show",
          d.window.isVisible() && d.window.isExposed()
                  && verification::windowPolicySatisfied(d.window));
    auto *const retainedRoot = d.root.get();
    auto *const retainedScene = d.native;
    for (const bool playing : { false, true }) {
        if (playing) {
            d.playback.toggle();
            d.wait([&] {
                return d.playback.playing();
            });
        }
        for (const bool minimize : { false, true }) {
            QSignalSpy invalidations(&d.window, &QQuickWindow::sceneGraphInvalidated);
            d.setStage(u"native-%1-%2"_s.arg(playing ? "playing"_L1 : "paused"_L1,
                                             minimize ? "minimize"_L1 : "hide"_L1));
            if (minimize)
                d.window.showMinimized();
            else
                d.surface.hide();
            const bool hidden = QTest::qWaitFor(
                    [&] {
                        bool stopped = !d.window.isExposed()
                                && !d.root->property("visualActive").toBool()
                                && !d.motion.clock()->active();
                        if (minimize) {
                            stopped &= d.window.windowState() == Qt::WindowMinimized;
#if defined(Q_OS_MACOS)
                            stopped &= verification::nativeWindowPolicy(d.window)[u"miniaturized"_s]
                                               .toBool();
#endif
                        } else {
                            stopped &= !d.window.isVisible();
                        }
                        return stopped;
                    },
                    5000);
            check("native-window-stops-visuals", hidden);
            require(hidden, "Native window did not become hidden/minimized");
            d.window.releaseResources();
            const bool basic = qgetenv("QSG_RENDER_LOOP") == "basic"_ba;
            if (basic)
                d.window.destroy();
            else
                check("same-root-sg-invalidation",
                      QTest::qWaitFor(
                              [&] {
                                  return rhi::sceneSubmissionStats().renderers.load() == 0;
                              },
                              5000));
            // Qt's persistency flags are hints; basic may retain its scene graph/context.
            // Full owner release is checked after deleting the root and its cleanup frame.
            const auto hiddenRenderers = rhi::sceneSubmissionStats().renderers.load();
            d.quiet(150);
            const auto stoppedFrames = d.motion.clock()->frameCount();
            const auto stoppedRenders = rhi::sceneSubmissionStats().rendered.load();
            const auto position = d.playback.position();
            if (playing)
                check("native-hidden-song-advances",
                      QTest::qWaitFor(
                              [&] {
                                  return d.playback.position() > position;
                              },
                              2000));
            else
                d.quiet(200);
            check("native-hidden-no-visual-work",
                  d.motion.clock()->frameCount() == stoppedFrames
                          && rhi::sceneSubmissionStats().rendered.load() == stoppedRenders);
            check("native-hidden-preserves-media", preservesMedia(playing));
            if (!playing)
                check("native-hidden-paused-position", d.playback.position() == position);
            d.window.showNormal();
            d.surface.show();
            require(QTest::qWaitFor(
                            [&] {
                                return d.window.isExposed()
                                        && d.root->property("visualActive").toBool();
                            },
                            5000),
                    "Native window did not restore");
            d.settle();
            check("native-restore-retains-root-and-scene",
                  d.root.get() == retainedRoot && d.native == retainedScene);
            const auto restoredInput = d.scene.frame().inputRevision;
            const auto submitted =
                    d.samples.isEmpty() ? QJsonObject{ } : d.samples.last().toObject();
            check("native-restore-current-render",
                  d.native->input().scene.inputRevision == restoredInput
                          && submitted[u"inputRevision"_s].toInteger() == qint64(restoredInput)
                          && !d.surface.grab().isNull());
            check("native-restore-preserves-media", preservesMedia(playing));
            check("native-restore-window-policy", verification::windowPolicySatisfied(d.window));
            transitions.append(
                    QJsonObject{ { u"playing"_s, playing },
                                 { u"minimized"_s, minimize },
                                 { u"surfaceRecreated"_s, basic },
                                 { u"sceneGraphInvalidations"_s, invalidations.size() },
                                 { u"retainedSceneRenderersWhileHidden"_s, hiddenRenderers },
                                 { u"inputRevision"_s, qint64(restoredInput) },
                                 { u"submittedInputRevision"_s, submitted[u"inputRevision"_s] },
                                 { u"submittedFrameRevision"_s, submitted[u"frameRevision"_s] },
                                 { u"windowPolicy"_s, verification::windowPolicy(d.window) } });
#if defined(Q_OS_MACOS)
            check("native-restore-onscreen",
                  verification::nativeWindowPolicy(d.window)[u"onScreen"_s].toBool());
#endif
        }
    }
    d.playback.pause();
}

}
int reviewStartup(QGuiApplication &, const QString &output, double seconds,
                  const QJsonObject &bootstrap, const QString &scenario, bool observeOnly)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 120 || (seconds > 0 && seconds < 12))
        return 2;
    const QStringList allStages{ u"day-wave"_s,     u"day-ruler"_s,      u"day-detail"_s,
                                 u"day-switch"_s,   u"day-overview"_s,   u"day-search"_s,
                                 u"night-wave"_s,   u"night-ruler"_s,    u"night-detail"_s,
                                 u"night-switch"_s, u"night-overview"_s, u"night-search"_s };
    if (scenario != "matrix"_L1 && !allStages.contains(scenario))
        return 2;
    const bool stress = qEnvironmentVariableIntValue("YALAHOO_VERIFY_STRESS") == 1;
    if ((stress && seconds != 0)
        || (!stress && qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1)
        || (observeOnly
            && (stress || OffscreenSurface::Enabled || seconds == 0
                || (scenario != "day-wave"_L1 && scenario != "night-wave"_L1))))
        return 2;
    const int switchCount = stress ? 30 : 3;
    const int rebuildCount = stress ? 10 : 2;
    QDir().mkpath(output);
    QJsonObject report{ { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                        { u"os"_s, QSysInfo::prettyProductName() },
                        { u"architecture"_s, QSysInfo::currentCpuArchitecture() },
                        { u"normalVsync"_s, QJsonValue::Null },
                        { u"inputToPhotonMs"_s, QJsonValue::Null },
                        { u"measuredVramBytes"_s, QJsonValue::Null } };
    report[u"bootstrap"_s] = bootstrap;
    report[u"scenario"_s] = scenario;
    report[u"stressRequested"_s] = stress;
    report[u"observationOnly"_s] = observeOnly;
    report[u"samplingRequestedSeconds"_s] = seconds;
    report[u"switches"_s] = 0;
    report[u"rebuilds"_s] = 0;
    QJsonArray checks, resources, actions;
    bool passed = true;
    const auto check = [&](const char *name, bool value) {
        checks.append(
                QJsonObject{ { u"name"_s, QString::fromUtf8(name) }, { u"passed"_s, value } });
        passed &= value;
        if (!value)
            qWarning("Startup check failed: %s", name);
    };
    try {
        QTemporaryDir anonymous;
        require(anonymous.isValid(), "Cannot create anonymous fixture directory");
        QByteArray bytes;
        if (!observeOnly) {
            QFile audio(anonymous.path() + "/anonymous.wav"_L1);
            require(audio.open(QIODevice::WriteOnly), "Cannot create anonymous audio");
            bytes = test::wave(600000);
            require(audio.write(bytes) == bytes.size(), "Cannot write anonymous audio");
            audio.close();
        }
        const auto input = fixture(anonymous.path(), !observeOnly, !observeOnly);
        QCryptographicHash inputHash(QCryptographicHash::Sha256);
        for (int i = 0; i < (observeOnly ? 1 : 96); ++i) {
            QFile file(observeOnly ? u":/assets/demo-covers/square.png"_s
                                   : anonymous.path() + u"/anonymous-%1.jpg"_s.arg(i));
            require(file.open(QIODevice::ReadOnly), "Cannot hash anonymous input");
            require(inputHash.addData(&file), "Cannot hash cover bytes");
        }
        inputHash.addData(bytes);
        report[u"fixtureSha256"_s] = QString::fromUtf8(inputHash.result().toHex());
        QElapsedTimer preparation;
        preparation.start();
        QJsonArray guiFrames;
        Driver d(input);
        if (observeOnly) {
            QObject::connect(d.motion.clock(), &FrameClock::advanced, &d.window, [&] {
                if (guiFrames.size() == 40000)
                    d.overflow = true;
                else
                    guiFrames.append(QJsonObject{
                            { u"atNs"_s, nowNs() },
                            { u"clockFrame"_s, qint64(d.motion.clock()->frameCount()) } });
            });
            QObject::connect(
                    &d.window, &QQuickWindow::afterSynchronizing, &d.window,
                    [samples = d.presented] {
                        const QMutexLocker lock(&samples->mutex);
                        if (samples->synchronized.size() == 40000)
                            samples->overflow = true;
                        else
                            samples->synchronized.append(nowNs());
                    },
                    Qt::DirectConnection);
        }
        struct SaveEvidence
        {
            std::function<void()> save;
            ~SaveEvidence()
            {
                save();
            }
        } evidence{ [&] {
            report[u"samples"_s] = d.samples;
            report[u"quickTimings"_s] = d.timings;
            report[u"inventories"_s] = d.inventories;
            report[u"warnings"_s] = QJsonArray::fromStringList(d.warnings);
            report[u"lastStage"_s] = d.stage;
            report[u"guiFrames"_s] = guiFrames;
            report[u"nativeWindowEvents"_s] = d.window.events;
            report[u"nativeWindowEventOverflow"_s] = d.window.eventOverflow;
            if (!report.contains(u"windowPolicy"_s))
                report[u"windowPolicy"_s] = verification::windowPolicy(d.window);
            const QMutexLocker lock(&d.presented->mutex);
            QJsonArray synchronized, presented;
            for (const auto timestamp : d.presented->synchronized)
                synchronized.append(timestamp);
            for (const auto timestamp : d.presented->times)
                presented.append(timestamp);
            report[u"synchronizationTimestampsNs"_s] = synchronized;
            if (!report.contains(u"qtPresentationTimestampsNs"_s))
                report[u"qtPresentationTimestampsNs"_s] = presented;
        } };
        d.opening.replay();
        d.wait([&] {
            return d.opening.film();
        });
        report[u"preparationMs"_s] = preparation.nsecsElapsed() / 1e6;
        check("initial-consistent-snapshot",
              d.library.snapshot()->albums.size() == 576
                      && d.startup.statistics()[u"appliedSnapshots"_s].toInt() == 1);
        check("readiness-does-not-consume-film", d.opening.time() == 0);
        d.opening.skip();
        d.motion.setReducedMotion(true);
        d.settle();
        const auto baselineResources = d.lastResources;
        report[u"baselineResources"_s] = baselineResources;
        report[u"initialCovers"_s] = d.native->coverStatistics();
        const auto releaseCheck = [&] {
            const auto value = counters();
            return d.failure.isEmpty() && value[u"sceneRenderers"_s].toInt() == 0
                    && value[u"rulerRenderers"_s].toInt() == 0
                    && value[u"openingRenderers"_s].toInt() == 0
                    && value[u"textNodes"_s].toInt() == 0;
        };
        const auto resourceSnapshot = [&] {
            return QJsonObject{
                { u"stage"_s, d.stage },
                { u"resources"_s, d.lastResources },
                { u"covers"_s, d.native->coverStatistics() },
                { u"counters"_s, counters() },
                { u"startup"_s, d.startup.statistics() },
                { u"title"_s, QJsonObject::fromVariantMap(d.native->titleReel()->diagnostics()) },
                { u"transport"_s,
                  QJsonObject::fromVariantMap(d.native->transportReel()->diagnostics()) }
            };
        };
        resources.append(resourceSnapshot());
        if (!observeOnly) {
            if (!OffscreenSurface::Enabled)
                d.wait(
                        [&] {
                            return !d.motion.clock()->active() && !d.motion.clock()->workPending();
                        },
                        false, 5000);
            const auto idle = counters();
            const auto idleSubmission = d.native->submission();
            const auto idleFrames = d.motion.clock()->frameCount();
            d.quiet(120);
            check("idle-zero-scene-submission", d.native->submission() == idleSubmission);
            check("idle-zero-visual-step", d.motion.clock()->frameCount() == idleFrames);
            check("idle-zero-canvas-repaint",
                  counters()[u"rulerPaints"_s] == idle[u"rulerPaints"_s]
                          && counters()[u"openingPaints"_s] == idle[u"openingPaints"_s]);
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
            d.setStage(u"hidden-snapshots"_s);
            d.motion.clock()->setApplicationVisible(false);
            d.motion.clock()->setActive(false);
            const auto hiddenSubmission = d.native->submission();
            const auto hiddenRenders = rhi::sceneSubmissionStats().rendered.load();
            const QJsonValue hiddenStarts =
                    d.native->coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s];
            const auto hiddenApplied = d.startup.statistics()[u"appliedSnapshots"_s].toInt();
            auto replacement = *input;
            for (int i = 0; i < 10; ++i) {
                replacement.albums[0].title = u"latest-%1"_s.arg(i);
                d.startup.receive(std::make_shared<const LibrarySnapshot>(replacement));
            }
            d.selection.moveRow(2);
            d.scene.lighting()->setNight(false);
            d.native->ensurePolished();
            d.quiet(150);
            check("hidden-no-snapshot-notifications",
                  d.startup.statistics()[u"appliedSnapshots"_s].toInt() == hiddenApplied);
            check("hidden-no-submit-or-render",
                  d.native->submission() == hiddenSubmission
                          && rhi::sceneSubmissionStats().rendered.load() == hiddenRenders);
            check("hidden-no-new-decode",
                  d.native->coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s]
                          == hiddenStarts);
            d.motion.clock()->setApplicationVisible(true);
            d.motion.clock()->setActive(true);
            QSignalSpy resumed(d.motion.clock(), &FrameClock::frame);
            d.motion.clock()->advance(100);
            check("resume-discards-first-interval",
                  resumed.size() == 1 && resumed[0][0].toDouble() == 0);
            d.frames(2);
            d.settle();
            check("resume-one-latest-snapshot",
                  d.startup.statistics()[u"appliedSnapshots"_s].toInt() == hiddenApplied + 1
                          && d.library.snapshot()->albums[0].title == "latest-9"_L1);
            check("resume-presents-latest-state",
                  d.native->input().scene.revision == d.scene.frame().revision);
            const auto preservesMedia = [&] {
                return d.playback.trackId() == trackId && d.playback.source() == source
                        && d.playback.sourceRevision() == sourceRevision
                        && d.playback.requested() == requested
                        && (requested ? d.playback.playing() : !d.playback.playing());
            };
            check("hidden-preserves-current-media-and-intent", preservesMedia());
            if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1) {
                QJsonArray transitions;
                verifyNativeWindowLifecycle(d, check, transitions);
                report[u"nativeWindowTransitions"_s] = transitions.size();
                report[u"nativeWindowTransitionStates"_s] = transitions;
                requested = false;
            }
            d.setStage(u"switch-replay"_s);
            d.presentation.open();
            d.wait(
                    [&] {
                        return d.presentation.detailEnabled();
                    },
                    true);
            for (int i = 0; i < switchCount; ++i) {
                const auto &albums = d.library.snapshot()->albums;
                d.presentation.select(albums[i % 2 ? 1 : 2].id, true);
                d.wait(
                        [&] {
                            return d.presentation.detailEnabled()
                                    && !d.presentation.pendingSelection();
                        },
                        true);
                d.settle();
                resources.append(resourceSnapshot());
                check("bounded-switch-pool",
                      d.native->coverStatistics()[u"owned"_s].toInt() <= 4
                              && d.native->input().scene.instances.size()
                                      <= ArchivePool::capacity + 4);
                report[u"switches"_s] = i + 1;
            }
            d.presentation.back();
            d.wait(
                    [&] {
                        return d.presentation.archiveEnabled();
                    },
                    true);
            d.settle();
            const QJsonValue before =
                    d.native->coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s];
            d.setStage(u"cache-hit"_s);
            for (int i = 0; i < 6; ++i) {
                d.selection.select(d.library.snapshot()->albums[i % 2 ? 1 : 2].id);
                d.settle();
            }
            check("cache-hits-no-redecode",
                  d.native->coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s] == before);
            d.frames(2);
            check("settled-zero-upload",
                  d.samples.last().toObject()[u"atlasUploadBytes"_s].toInt() == 0
                          && d.samples.last().toObject()[u"selectedUploadBytes"_s].toInt() == 0);
            const QJsonValue stableBytes = d.lastResources[u"estimatedCurrentBytes"_s];
            for (int cycle = 0; cycle < rebuildCount; ++cycle) {
                d.setStage(u"rebuild-%1"_s.arg(cycle));
                if (cycle == rebuildCount / 2) {
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
                      d.lastResources[u"estimatedCurrentBytes"_s] == stableBytes);
                check("rebuild-retained-cover-budget",
                      d.native->coverStatistics()[u"atlasRetainedBytes"_s].toInteger()
                              <= qint64(ArchivePool::capacity + 4) * 256 * 256 * 4);
                report[u"rebuilds"_s] = cycle + 1;
            }
            d.playback.stop();
            d.motion.setReducedMotion(false);
            d.motion.clock()->setActive(true);
            // Exercise real ruler/controller/Quick text, panels, source search, opening and target invalidation.
            const auto drawBefore = counters();
            d.setStage(u"ruler-reverse"_s);
            for (int i = 0; i < 24; ++i) {
                QKeyEvent key(QEvent::KeyPress, i < 12 ? Qt::Key_Down : Qt::Key_Up, Qt::NoModifier);
                QCoreApplication::sendEvent(&d.window, &key);
                d.frames(2, true);
            }
            d.setStage(u"overview"_s);
            d.overview.setActive(true);
            d.frames(45, true);
            d.overview.setActive(false);
            d.setStage(u"search-panels"_s);
            d.panels.open(u"search"_s);
            d.library.search(u"\u533f\u540d"_s);
            d.library.search(u"\u533f\u540d\u4e13\u8f91 2"_s);
            d.library.search(u""_s);
            d.frames(25, true);
            d.panels.close();
            d.frames(25, true);
            d.selection.setRealistic(true);
            d.frames(10, true);
            d.selection.setRealistic(false);
            d.setStage(u"opening-replay"_s);
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
            report[u"opaqueRootRebuilds"_s] = 1;
            d.motion.clock()->setActive(true);
            d.motion.clock()->advance(30);
            d.opening.skip();
            d.frames(50, true);
            for (double dpr : { 1.0, 1.5, 2.0 }) {
                d.setStage(u"theme-dpr-resize"_s);
                d.scene.lighting()->setNight(!d.scene.lighting()->night());
                d.resize({ 960, 900 }, dpr);
                d.frames(45, true);
            }
            d.resize({ 1280, 720 }, d.window.devicePixelRatio());
            check("actual-ruler-and-opening-drawing",
                  counters()[u"rulerPaints"_s].toInteger()
                                  > drawBefore[u"rulerPaints"_s].toInteger()
                          && counters()[u"openingPaints"_s].toInteger()
                                  > drawBefore[u"openingPaints"_s].toInteger());
            resources.append(resourceSnapshot());
        }
        if (seconds > 0) {
            if (observeOnly) {
                d.motion.setReducedMotion(false);
                d.scene.lighting()->setNight(scenario == "night-wave"_L1);
            }
            d.resize({ 1920, 1080 }, d.window.devicePixelRatio());
            d.scene.quality()->setPreset(u"original"_s);
            d.settle();
            const auto stages = scenario == "matrix"_L1 ? allStages : QStringList{ scenario };
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
                        d.panels.open(u"search"_s);
                    actions.append(QJsonObject{ { u"stage"_s, d.stage },
                                                { u"elapsedSeconds"_s, elapsed },
                                                { u"atNs"_s, nowNs() } });
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
                        d.scene.recordInput();
                        d.presentation.select(d.library.snapshot()->albums[event % 2 + 1].id, true);
                    } else if (local == 5) {
                        d.scene.recordInput();
                        d.library.search(event % 3 == 0 ? u""_s
                                                 : event % 3 == 1
                                                 ? u"\u533f\u540d"_s
                                                 : u"\u533f\u540d\u4e13\u8f91 2"_s);
                    }
                }
                return false;
            };
            if (scenario != "matrix"_L1 && !observeOnly) {
                d.wait(
                        [&] {
                            return replay(1);
                        },
                        true, 30000, true);
                eventIndex = -1;
            }
            const auto startSample = d.samples.size(), startTiming = d.timings.size();
            sampling.restart();
            report[u"samplingStartedNs"_s] = nowNs();
            d.window.record(u"sampling-start"_s);
            if (observeOnly) {
                replay(seconds);
                QEventLoop observation;
                QTimer::singleShot(int(seconds * 1000), Qt::PreciseTimer, &observation,
                                   &QEventLoop::quit);
                observation.exec();
                require(d.native->errorText().isEmpty() && d.failure.isEmpty(),
                        "Presentation observation failed");
            } else {
                d.wait(
                        [&] {
                            return replay(seconds);
                        },
                        true, int(seconds * 1000 + 30000), true);
            }
            report[u"samplingEndedNs"_s] = nowNs();
            report[u"samplingSeconds"_s] = sampling.nsecsElapsed() / 1e9;
            report[u"sampleStartIndex"_s] = startSample;
            report[u"quickTimingStartIndex"_s] = startTiming;
            report[u"fixedStages"_s] = QJsonArray::fromStringList(stages);
        }
        d.setStage(u"final-release"_s);
        if (OffscreenSurface::Enabled)
            require(d.surface.grab().save(output + "/product-restored.png"_L1),
                    "Cannot save owned product output");
        report[u"finalCovers"_s] = d.native->coverStatistics();
        report[u"peakDescriptorReplacementBytes"_s] = d.peakBytes;
        report[u"startup"_s] = d.startup.statistics();
        report[u"offscreen"_s] = d.surface.diagnostics();
        report[u"windowPolicy"_s] = verification::windowPolicy(d.window);
        report[u"dpr"_s] = d.window.effectiveDevicePixelRatio();
        report[u"backend"_s] = int(d.window.rendererInterface()->graphicsApi());
        report[u"adapters"_s] = rhi::adapterInventory();
        if (auto *api = d.surface.rhi()) {
            const auto info = api->driverInfo();
            report[u"driver"_s] =
                    QJsonObject{ { u"deviceName"_s, QString::fromUtf8(info.deviceName) },
                                 { u"deviceId"_s, qint64(info.deviceId) },
                                 { u"vendorId"_s, qint64(info.vendorId) },
                                 { u"deviceType"_s, int(info.deviceType) } };
        }
        report[u"processFixedAssets"_s] = rhi::preparedAssetStatistics();
        report[u"refreshHz"_s] = d.window.screen() ? d.window.screen()->refreshRate() : 0;
        report[u"swapIntervalRequested"_s] = d.window.requestedFormat().swapInterval();
        report[u"qsgNoVsyncRequested"_s] = qEnvironmentVariableIsSet("QSG_NO_VSYNC");
        report[u"normalPresentationMeasured"_s] =
                !OffscreenSurface::Enabled && d.window.requestedFormat().swapInterval() > 0;
        report[u"measurementScope"_s] = OffscreenSurface::Enabled
                ? u"Production MusicStage into hidden RenderControl; manual shared-clock event replay at requested 60Hz; GUI polish, sync, Quick render, endFrame wait and RHI submission are separate local costs. No native presentation or physical input measurement."_s
                : u"Production MusicStage native FrameAnimation; actual frameSwapped event timestamps; physical display/input-to-photon and compositor latency unavailable."_s;
        QMap<QString, QVector<double>> cpu, sync, gpu, inputLatency;
        QSet<quint64> inputs;
        for (qsizetype i = report[u"sampleStartIndex"_s].toInt(); i < d.samples.size(); ++i) {
            const auto sample = d.samples[i].toObject();
            const auto name = sample[u"stage"_s].toString();
            cpu[name].append(sample[u"cpuSubmitMs"_s].toDouble());
            sync[name].append(sample[u"synchronizationMs"_s].toDouble());
            if (sample[u"gpuFrameMs"_s].isDouble())
                gpu[name].append(sample[u"gpuFrameMs"_s].toDouble());
            const auto revision = quint64(sample[u"inputRevision"_s].toInteger());
            if (revision && !inputs.contains(revision)
                && sample[u"inputReceivedNs"_s].toInteger()
                        >= report[u"samplingStartedNs"_s].toInteger()
                && sample[u"submittedNs"_s].toInteger()
                        <= report[u"samplingEndedNs"_s].toInteger()) {
                inputs.insert(revision);
                inputLatency[name].append((sample[u"submittedNs"_s].toInteger()
                                           - sample[u"inputReceivedNs"_s].toInteger())
                                          / 1e6);
            }
        }
        QJsonObject stages;
        for (auto it = cpu.cbegin(); it != cpu.cend(); ++it)
            stages[it.key()] =
                    QJsonObject{ { u"cpuSubmit"_s, distribution(it.value()) },
                                 { u"sync"_s, distribution(sync[it.key()]) },
                                 { u"gpu"_s, distribution(gpu[it.key()]) },
                                 { u"inputToQtSubmit"_s, distribution(inputLatency[it.key()]) } };
        report[u"stages"_s] = stages;
        QJsonObject quickCosts;
        const QStringList costNames{ u"polishMs"_s, u"beginFrameMs"_s, u"syncMs"_s,
                                     u"quickRenderMs"_s, u"endFrameMs"_s };
        QMap<QString, QMap<QString, QVector<double>>> costs;
        for (qsizetype i = report[u"quickTimingStartIndex"_s].toInt(); i < d.timings.size(); ++i) {
            const auto timing = d.timings[i].toObject();
            for (const auto &name : costNames)
                costs[timing[u"stage"_s].toString()][name].append(timing[name].toDouble());
        }
        for (auto it = costs.cbegin(); it != costs.cend(); ++it) {
            QJsonObject entry;
            for (const auto &name : costNames)
                entry[name] = distribution(it.value().value(name));
            quickCosts[it.key()] = entry;
        }
        report[u"quickCosts"_s] = quickCosts;
        QVector<double> intervals;
        QMap<QString, QVector<double>> presentation;
        {
            const QMutexLocker lock(&d.presented->mutex);
            for (qsizetype i = 1; i < d.presented->times.size(); ++i) {
                const auto previous = d.presented->times[i - 1], current = d.presented->times[i];
                if (previous < report[u"samplingStartedNs"_s].toInteger()
                    || current > report[u"samplingEndedNs"_s].toInteger())
                    continue;
                const double dt = (current - previous) / 1e6;
                intervals.append(dt);
                for (qsizetype j = actions.size() - 1; j >= 0; --j) {
                    const auto action = actions[j].toObject();
                    // Separate each transition/warmup second from steady target gates.
                    if (previous
                        >= action[u"atNs"_s].toInteger() + (observeOnly ? 0 : 1000000000)) {
                        presentation[action[u"stage"_s].toString()].append(dt);
                        break;
                    }
                    if (previous >= action[u"atNs"_s].toInteger())
                        break;
                }
            }
        }
        report[u"qtPresentationIntervals"_s] = distribution(intervals);
        bool nativeValid = !OffscreenSurface::Enabled && seconds > 0
                && report[u"samplingSeconds"_s].toDouble() >= seconds && scenario != "matrix"_L1
                && presentation.size() == 1;
        QJsonArray presentationTimes;
        {
            const QMutexLocker lock(&d.presented->mutex);
            for (const auto timestamp : d.presented->times)
                if (timestamp >= report[u"samplingStartedNs"_s].toInteger()
                    && timestamp <= report[u"samplingEndedNs"_s].toInteger())
                    presentationTimes.append(timestamp);
        }
        report[u"qtPresentationTimestampsNs"_s] = presentationTimes;
        report[u"nativeWindowEvents"_s] = d.window.events;
        report[u"nativeWindowEventOverflow"_s] = d.window.eventOverflow;
        bool continuouslyExposed = !d.window.eventOverflow;
        for (const auto &event : d.window.events) {
            const auto value = event.toObject();
            if (value[u"atNs"_s].toInteger() >= report[u"samplingStartedNs"_s].toInteger()
                && value[u"atNs"_s].toInteger() <= report[u"samplingEndedNs"_s].toInteger())
                continuouslyExposed &= value[u"exposed"_s].toBool();
        }
        report[u"continuouslyExposedDuringSampling"_s] = OffscreenSurface::Enabled || seconds == 0
                ? QJsonValue::Null
                : QJsonValue(continuouslyExposed);
        nativeValid &= continuouslyExposed;
        bool numericPass = true;
        int stableOver50 = 0;
        QJsonObject presentationStages;
        for (auto it = presentation.cbegin(); it != presentation.cend(); ++it) {
            const auto stats = distribution(it.value());
            nativeValid &= it.value().size() >= 30;
            stableOver50 += stats[u"over50Ms"_s].toInt();
            numericPass &= stats[u"p95Ms"_s].toDouble() <= 20
                    && stats[u"p99Ms"_s].toDouble() <= 1000.0 / 30
                    && stats[u"over50Ms"_s].toInt() <= 1;
            presentationStages[it.key()] = stats;
        }
        numericPass &= stableOver50 <= 1;
        report[u"nativeSamplingValid"_s] = nativeValid;
        report[u"targetRefreshHz"_s] = 60;
        report[u"offscreenReplayPeriodMsRequested"_s] =
                OffscreenSurface::Enabled ? QJsonValue(1000.0 / 60) : QJsonValue::Null;
        report[u"steadyWarmupSecondsPerStage"_s] = observeOnly ? 0 : 1;
        report[u"qtPresentationByStage"_s] = presentationStages;
        const bool performanceGateEnforced = !observeOnly && !OffscreenSurface::Enabled
                && seconds == 120 && scenario != "matrix"_L1
                && d.window.requestedFormat().swapInterval() > 0
                && d.window.effectiveDevicePixelRatio() == 1;
        report[u"performanceGateEnforced"_s] = performanceGateEnforced;
        report[u"performanceGatePassed"_s] =
                performanceGateEnforced ? QJsonValue(nativeValid && numericPass) : QJsonValue::Null;
        report[u"samplingComplete"_s] = (!d.overflow && !d.presented->overflow)
                && (seconds == 0 || stages.size() >= (scenario == "matrix"_L1 ? 12 : 1));
        report[u"performanceAcceptance"_s] = OffscreenSurface::Enabled
                ? u"unavailable-native-presentation"_s
                : seconds == 0             ? u"functional-replay-only"_s
                : scenario == "matrix"_L1  ? u"matrix-local-diagnostic-only"_s
                : !nativeValid             ? u"invalid-native-sample"_s
                : !performanceGateEnforced ? u"native-local-diagnostic-only"_s
                : numericPass              ? u"native-qt-presentation-targets-met"_s
                                           : u"native-qt-presentation-targets-failed"_s;
        report[u"samples"_s] = d.samples;
        report[u"quickTimings"_s] = d.timings;
        report[u"inventories"_s] = d.inventories;
        report[u"actions"_s] = actions;
        check("bounded-telemetry-no-overflow", (!d.overflow && !d.presented->overflow));
        check("no-qml-warnings", d.warnings.isEmpty());
        report[u"warnings"_s] = QJsonArray::fromStringList(d.warnings);
        check("window-policy", verification::windowPolicySatisfied(d.window));
        d.destroyRoot();
        check("final-release-zero-owned-renderers", releaseCheck());
        report[u"releasedCounters"_s] = counters();
    } catch (const std::exception &error) {
        passed = false;
        report[u"error"_s] = QString::fromUtf8(error.what());
        qWarning("Startup audit failed: %s", error.what());
    }
    report[u"functionalPassed"_s] = passed;
    report[u"checks"_s] = checks;
    report[u"resourceCheckpoints"_s] = resources;
    QSaveFile file(output + "/report.json"_L1);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson()) < 0
        || !file.commit())
        return 2;
    return passed ? 0 : 1;
}

int reviewWindowLifecycle(const QString &output)
{
    if (OffscreenSurface::Enabled || QFileInfo::exists(output) || !QDir().mkpath(output))
        return 2;
    QJsonObject report{
        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
        { u"measurementScope"_s,
          u"Same native MusicStage window: paused/playing hide and minimize, scene restoration and public media continuity; no performance sampling or physical input/hearing claim."_s }
    };
    QJsonArray checks;
    bool passed = true;
    const auto check = [&](const char *name, bool value) {
        checks.append(
                QJsonObject{ { u"name"_s, QString::fromUtf8(name) }, { u"passed"_s, value } });
        passed &= value;
        if (!value)
            qWarning("Native lifecycle check failed: %s", name);
    };
    try {
        QTemporaryDir anonymous;
        require(anonymous.isValid(), "Cannot create native lifecycle fixture");
        QFile audio(anonymous.path() + "/anonymous.wav"_L1);
        require(audio.open(QIODevice::WriteOnly), "Cannot create anonymous song");
        const auto bytes = test::wave(60000);
        require(audio.write(bytes) == bytes.size(), "Cannot write anonymous song");
        audio.close();
        auto input = *fixture(anonymous.path(), false, false);
        Track track;
        track.id = u"native-lifecycle-track"_s;
        track.title = u"Anonymous lifecycle sample"_s;
        track.sourceUrl = QUrl::fromLocalFile(audio.fileName());
        input.albums[0].tracks.append(track);
        Driver d(std::make_shared<const LibrarySnapshot>(std::move(input)), true);
        const auto evidence = qScopeGuard([&] {
            report[u"windowPolicy"_s] = verification::windowPolicy(d.window);
            report[u"nativeWindowEvents"_s] = d.window.events;
            report[u"warnings"_s] = QJsonArray::fromStringList(d.warnings);
            report[u"lastStage"_s] = d.stage;
            report[u"backend"_s] = int(d.window.rendererInterface()->graphicsApi());
            report[u"dpr"_s] = d.window.effectiveDevicePixelRatio();
        });
        d.opening.replay();
        d.wait([&] {
            return d.opening.film();
        });
        d.opening.skip();
        d.motion.setReducedMotion(false);
        d.settle();
        d.playback.setVolume(0);
        check("anonymous-public-media-command",
              d.playback.playTrack(d.library.snapshot()->albums[0].id, track.id));
        d.wait([&] {
            return d.playback.playing();
        });
        d.playback.pause();
        d.wait([&] {
            return !d.playback.playing();
        });
        QJsonArray transitions;
        verifyNativeWindowLifecycle(d, check, transitions);
        report[u"nativeWindowTransitions"_s] = transitions;
        check("restored-own-output", d.surface.grab().save(output + "/restored.png"_L1));
        check("window-policy", verification::windowPolicySatisfied(d.window));
        check("no-qml-or-renderer-errors",
              d.warnings.isEmpty() && d.failure.isEmpty() && d.native->errorText().isEmpty());
        d.playback.stop();
        check("stop-releases-song", d.playback.source().isEmpty() && !d.playback.requested());
        d.destroyRoot();
        report[u"releasedCounters"_s] = counters();
        check("released-owned-renderers",
              d.failure.isEmpty() && rhi::sceneSubmissionStats().renderers.load() == 0
                      && rulerInkStats().renderers.load() == 0
                      && openingInkStats().renderers.load() == 0 && ReelInk::liveNodes() == 0);
    } catch (const std::exception &error) {
        passed = false;
        report[u"error"_s] = QString::fromUtf8(error.what());
        qWarning("Native lifecycle failed: %s", error.what());
    }
    report[u"passed"_s] = passed;
    report[u"checks"_s] = checks;
    QSaveFile file(output + "/report.json"_L1);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson()) < 0
        || !file.commit())
        return 2;
    return passed ? 0 : 1;
}

int reviewGui(const QString &output, bool narrowOnly, bool representative)
{
    // The same driver and production stage serve cold-start and integrated GUI checks.
    // Captures are explicit checkpoints; their readback cost is excluded from local timings.
    if (QFileInfo::exists(output) || !QDir().mkpath(output))
        return 2;
    QJsonObject report{
        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
        { u"os"_s, QSysInfo::prettyProductName() },
        { u"architecture"_s, QSysInfo::currentCpuArchitecture() },
        { u"measurementScope"_s,
          u"Production MusicStage checkpoint replay; component source mathematics and continuous trajectories are verified separately. No physical input, native presentation, normal VSYNC or driver residency measurement."_s },
        { u"normalVsync"_s, QJsonValue::Null },
        { u"inputToPhotonMs"_s, QJsonValue::Null },
        { u"measuredVramBytes"_s, QJsonValue::Null }
    };
    QJsonArray checks, captures;
    QString context;
    bool passed = true;
    const auto check = [&](const char *name, bool value) {
        checks.append(QJsonObject{ { u"state"_s, context },
                                   { u"name"_s, QString::fromUtf8(name) },
                                   { u"passed"_s, value } });
        passed &= value;
        if (!value)
            qWarning().noquote() << "GUI check failed:" << context << name;
    };
    try {
        QTemporaryDir anonymous;
        require(anonymous.isValid(), "Cannot create GUI fixture");
        auto input = *fixture(anonymous.path(), false);
        const QStringList covers{ u"portrait.png"_s, u"landscape.png"_s, u"square.png"_s };
        for (int i = 0; i < input.albums.size(); ++i)
            input.albums[i].coverUrl =
                    QUrl("qrc:/assets/demo-covers/"_L1 + covers[i % covers.size()]);
        Driver d(std::make_shared<const LibrarySnapshot>(std::move(input)));
        const auto saveDiagnostics = qScopeGuard([&] {
            report[u"warnings"_s] = QJsonArray::fromStringList(d.warnings);
            report[u"samples"_s] = d.samples;
            report[u"quickTimings"_s] = d.timings;
            report[u"lastStage"_s] = d.stage;
        });
        d.opening.replay();
        d.wait([&] {
            return d.opening.film();
        });
        d.opening.skip();
        d.settle();
        const auto advance = [&](double seconds) {
            for (int i = 0; i < qCeil(seconds * 60); ++i)
                d.motion.clock()->advance(1.0 / 60);
            d.frames(2);
        };
        const auto click = [&](const QString &name) {
            auto *item = find(d.root.get(), name);
            require(item && item->isVisible() && item->isEnabled(),
                    "GUI action target unavailable");
            const auto point = item->mapToScene({ item->width() / 2, item->height() / 2 });
            require(QRectF(QPointF(), d.window.size()).contains(point),
                    "GUI action outside viewport");
            QTest::mouseClick(&d.window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        };
        const auto capture = [&](const QString &phase) {
            d.setStage(context + "-"_L1 + phase);
            d.frames(2);
            d.native->ensurePolished();
            check("no-render-error", d.native->errorText().isEmpty());
            const auto &frame = d.native->input().scene;
            check("current-production-snapshot",
                  (d.opening.sceneOccluded() ? d.native->input().sceneOccluded
                                             : frame.revision == d.scene.frame().revision)
                          && frame.width == d.window.width() && frame.height == d.window.height());
            bool finite = true;
            for (float value : std::span(frame.projection.constData(), 16))
                finite &= std::isfinite(value);
            for (const auto &instance : frame.instances)
                for (float value : std::span(instance.transform.constData(), 16))
                    finite &= std::isfinite(value);
            check("finite-bounded-geometry",
                  finite && frame.instances.size() <= ArchivePool::capacity + 4);
            check("bounded-owned-covers", d.native->coverStatistics()[u"owned"_s].toInt() <= 4);
            const auto image = d.surface.grab();
            const auto file = context + "-"_L1 + phase + ".png"_L1;
            require(!image.isNull() && image.save(output + "/"_L1 + file),
                    "Cannot capture own GUI output");
            const auto dpr = d.window.effectiveDevicePixelRatio();
            check("actual-output-dimensions", image.size() == d.window.size() * dpr);
            captures.append(QJsonObject{ { u"state"_s, context },
                                         { u"phase"_s, phase },
                                         { u"image"_s, file },
                                         { u"width"_s, d.window.width() },
                                         { u"height"_s, d.window.height() },
                                         { u"dpr"_s, dpr },
                                         { u"night"_s, d.scene.lighting()->night() },
                                         { u"realistic"_s, d.selection.realistic() },
                                         { u"selection"_s, d.selection.albumId() },
                                         { u"instances"_s, frame.instances.size() },
                                         { u"covers"_s, d.native->coverStatistics() },
                                         { u"resources"_s, d.lastResources },
                                         { u"submission"_s, qint64(d.native->submission()) } });
        };
        const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                  { 960, 900 },   { 930, 900 },   { 390, 844 },
                                  { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
        for (qsizetype index = 0; index < sizes.size(); ++index) {
            const auto size = sizes[index];
            if (representative && size != QSize(1280, 720) && size != QSize(930, 900)
                && size != QSize(390, 844))
                continue;
            if (narrowOnly && size.width() > 700)
                continue;
            const double dpr = OffscreenSurface::Enabled ? (index % 3 == 0           ? 1.0
                                                                    : index % 3 == 1 ? 1.5
                                                                                     : 2.0)
                                                         : d.window.devicePixelRatio();
            d.resize(size, dpr);
            for (bool night : { false, true }) {
                for (bool realistic : { false, true }) {
                    context = u"%1x%2-%3-%4"_s.arg(size.width())
                                      .arg(size.height())
                                      .arg(night ? "night"_L1 : "day"_L1)
                                      .arg(realistic ? "real"_L1 : "fill"_L1);
                    qInfo().noquote() << "GUI state:" << context;
                    d.scene.lighting()->setNight(night);
                    d.selection.setRealistic(realistic);
                    d.selection.select(u"album-0-2"_s);
                    advance(3);
                    d.ready();
                    d.root->forceActiveFocus(Qt::OtherFocusReason);
                    const auto selected = d.selection.albumId();
                    const auto revision = d.scene.frame().inputRevision;
                    QTest::keyClick(&d.window, Qt::Key_Down);
                    advance(1);
                    check("qt-navigation-commits",
                          d.selection.albumId() != selected
                                  && d.scene.frame().inputRevision > revision);
                    QTest::keyClick(&d.window, Qt::Key_Up);
                    advance(1);
                    check("reverse-navigation", d.selection.albumId() == selected);
                    d.ready();
                    check("archive-overview-control-visible",
                          find(d.root.get(), u"overviewToggle"_s)->isVisible());
                    auto *albumStepper = find(d.root.get(), u"albumStepper"_s);
                    auto *genreStepper = find(d.root.get(), u"genreStepper"_s);
                    require(albumStepper && genreStepper, "Navigation groups missing");
                    const auto albumBounds =
                            albumStepper->mapRectToScene(albumStepper->boundingRect());
                    const auto genreBounds =
                            genreStepper->mapRectToScene(genreStepper->boundingRect());
                    const QRectF viewport(QPointF(), d.window.size());
                    check("navigation-groups-fit",
                          viewport.contains(albumBounds) && viewport.contains(genreBounds)
                                  && !albumBounds.intersects(genreBounds));
                    capture(u"browse"_s);
                    const QJsonValue targetGeneration =
                            d.samples.last().toObject()[u"resourceGeneration"_s];
                    click(u"overviewToggle"_s);
                    advance(4);
                    check("qt-overview-enters", d.overview.active() && d.overview.ready());
                    capture(u"overview"_s);
                    click(u"overviewReturn"_s);
                    advance(4);
                    check("qt-overview-returns",
                          !d.overview.active() && d.presentation.archiveEnabled());
                    click(u"openAlbumButton"_s);
                    d.wait(
                            [&] {
                                return d.presentation.detailEnabled();
                            },
                            true);
                    d.ready();
                    advance(.5);
                    check("detail-content-fully-shown",
                          d.presentation.detailSurface()->opacity() == 1);
                    check("detail-hides-archive-overview-control",
                          !find(d.root.get(), u"overviewToggle"_s)->isVisible());
                    auto *back = find(d.root.get(), u"detailBack"_s);
                    auto *navigation = find(d.root.get(), u"headerNavigation"_s);
                    require(back && navigation, "Detail/header layout missing");
                    check("portrait-back-below-header",
                          !d.scene.layout()->portrait()
                                  || back->mapRectToScene(back->boundingRect()).top()
                                          >= navigation->mapRectToScene(navigation->boundingRect())
                                                     .bottom());
                    capture(u"detail"_s);
                    // Latest intent wins while the shared state machine is retiring prior text/case.
                    for (const auto &id : { "album-0-3", "album-0-4", "album-0-2" })
                        d.presentation.select(QString::fromUtf8(id), true);
                    d.wait(
                            [&] {
                                return d.presentation.detailEnabled()
                                        && !d.presentation.pendingSelection();
                            },
                            true);
                    check("interrupted-selection-last-intent", d.selection.albumId() == selected);
                    click(u"detailBack"_s);
                    d.wait(
                            [&] {
                                return d.presentation.archiveEnabled();
                            },
                            true);
                    d.root->forceActiveFocus(Qt::OtherFocusReason);
                    d.panels.open(u"search"_s);
                    advance(.5);
                    auto *search = find(d.window.contentItem(), u"librarySearchInput"_s);
                    require(search, "Search input missing");
                    search->setProperty("text", u"\u533f\u540d\u4e13\u8f91 2"_s);
                    const auto isolated = d.selection.albumId();
                    QTest::keyClick(&d.window, Qt::Key_Down);
                    advance(.2);
                    check("modal-navigation-isolated", d.selection.albumId() == isolated);
                    capture(u"search"_s);
                    d.panels.close();
                    advance(.5);
                    check("panel-releases", !d.panels.surface()->shown());
                    check("browse-does-not-play",
                          d.playback.source().isEmpty() && !d.playback.requested());
                    // Generation comparisons use renderer telemetry, not the GUI snapshot revision.
                    check("ui-preserves-target-generation",
                          targetGeneration.isDouble()
                                  && d.samples.last().toObject()[u"resourceGeneration"_s]
                                          == targetGeneration);
                    if (!realistic) {
                        d.opening.replay();
                        d.wait([&] {
                            return d.opening.film();
                        });
                        advance(2);
                        capture(u"opening-mark"_s);
                        advance(5);
                        capture(u"opening-scene"_s);
                        advance(6);
                        check("full-opening-handoff",
                              !d.opening.active() && d.presentation.archiveEnabled());
                    }
                }
            }
        }
        if (narrowOnly) {
            context = "navigation-label-boundaries"_L1;
            d.resize({ 390, 844 }, 2);
            auto replacement = *d.library.snapshot();
            replacement.genres[0].name = QString(200, QChar(0x957F));
            d.startup.receive(std::make_shared<const LibrarySnapshot>(replacement));
            d.frames(2);
            advance(3);
            auto *navigation = find(d.root.get(), u"navigationFrame"_s);
            auto *label = find(d.root.get(), u"navigationColumnLabel"_s);
            require(navigation && label, "Navigation boundary items missing");
            check("long-label-wraps-and-bounds",
                  navigation->property("wrapped").toBool()
                          && label->property("text").toString().size() == 200
                          && label->width() <= navigation->width());
            require(d.surface.grab().save(output + "/long-column.png"_L1),
                    "Cannot save long-column output");
            replacement.genres[0].name = "A"_L1;
            replacement.albums.resize(1);
            d.startup.receive(std::make_shared<const LibrarySnapshot>(replacement));
            d.frames(2);
            advance(3);
            check("short-pool-restores-single-row", !navigation->property("wrapped").toBool());
            require(d.surface.grab().save(output + "/single-album.png"_L1),
                    "Cannot save single-album output");
        }
        report[u"backend"_s] = int(d.window.rendererInterface()->graphicsApi());
        report[u"adapters"_s] = rhi::adapterInventory();
        report[u"windowPolicy"_s] = verification::windowPolicy(d.window);
        report[u"offscreen"_s] = d.surface.diagnostics();
        if (auto *api = d.surface.rhi()) {
            const auto info = api->driverInfo();
            report[u"driver"_s] =
                    QJsonObject{ { u"deviceName"_s, QString::fromUtf8(info.deviceName) },
                                 { u"deviceId"_s, qint64(info.deviceId) },
                                 { u"vendorId"_s, qint64(info.vendorId) },
                                 { u"deviceType"_s, int(info.deviceType) } };
        }
        report[u"peakDescriptorReplacementBytes"_s] = d.peakBytes;
        report[u"warnings"_s] = QJsonArray::fromStringList(d.warnings);
        report[u"samples"_s] = d.samples;
        report[u"quickTimings"_s] = d.timings;
        check("no-qml-warnings", d.warnings.isEmpty());
        check("bounded-samples", (!d.overflow && !d.presented->overflow));
        check("window-policy", verification::windowPolicySatisfied(d.window));
        d.destroyRoot();
        report[u"releasedCounters"_s] = counters();
        check("released-owned-renderers",
              d.failure.isEmpty() && rhi::sceneSubmissionStats().renderers.load() == 0
                      && rulerInkStats().renderers.load() == 0
                      && openingInkStats().renderers.load() == 0 && ReelInk::liveNodes() == 0);
    } catch (const std::exception &error) {
        passed = false;
        report[u"error"_s] = QString::fromUtf8(error.what());
        qWarning("GUI replay failed: %s", error.what());
    }
    report[u"passed"_s] = passed;
    report[u"checks"_s] = checks;
    report[u"captures"_s] = captures;
    QSaveFile file(output + "/report.json"_L1);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson()) < 0
        || !file.commit())
        return 2;
    return passed ? 0 : 1;
}
