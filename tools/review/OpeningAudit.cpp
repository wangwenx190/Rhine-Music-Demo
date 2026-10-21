#include "Require.h"
#include "ReviewSupport.h"
#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include <QQuickWindow>
#include "OpeningAudit.h"
#include "OffscreenSurface.h"
#include "OpeningController.h"
#include "OpeningInk.h"
#include "SceneItem.h"
#include "LibrarySession.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include "JsonFile.h"
#include <QEventLoop>
#include <QTimer>
#include <QKeyEvent>
#include <QSignalSpy>
#include <stdexcept>

using namespace Qt::StringLiterals;

using namespace yalahoo;

namespace {
// Diagnostic elapsed time converts monotonic nanoseconds to explicitly named report units.
constexpr auto NanosecondsPerMillisecond = 1e6;

class Driver final
{
    Q_DISABLE_COPY_MOVE(Driver)
public:
    Driver() : window(OffscreenSurface::Enabled ? &control : nullptr), surface(window, control)
    {
        verification::prepareWindow(window, true);
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        window.resize(1280, 720);
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        LibrarySnapshot snapshot;
        Genre genre;
        genre.id = "opening-demo"_L1;
        genre.name = u"\u533f\u540d\u5e8f\u7ae0\u6536\u85cf"_s;
        snapshot.genres.append(genre);
        for (int i = 0; i < 3; ++i) {
            Album album;
            album.id = u"opening-album-%1"_s.arg(i);
            album.title = u"\u533f\u540d\u4e13\u8f91 %1"_s.arg(i + 1);
            album.genreId = genre.id;
            album.coverUrl = QUrl(u"qrc:/assets/demo-covers/square.png"_s);
            snapshot.albums.append(album);
        }
        library.setSnapshot(std::move(snapshot));
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        scene.setSelection(&selection);
        QObject::connect(&engine, &QQmlEngine::warnings, &window,
                         [&](const QList<QQmlError> &errors) {
                             for (const auto &error : errors)
                                 warnings.append(error.toString());
                         });
        QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/pages/MusicStage.qml"_s));
        root.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
                { { u"librarySession"_s, QVariant::fromValue(&library) },
                  { u"motionController"_s, QVariant::fromValue(&motion) },
                  { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
                  { u"openingController"_s, QVariant::fromValue(&opening) },
                  { u"offscreenRendering"_s, true } })));
        if (!root)
            qWarning() << component.errors();
        require(bool(root), "Cannot create production MusicStage");
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        root->setParentItem(window.contentItem());
        root->setSize(window.size());
        native = qobject_cast<rhi::SceneItem *>(
                yalahoo::review::findVisual(root.get(), u"nativeScene"_s));
        require(native, "Missing production SceneItem");
        native->setTelemetryEnabled(true);
        QObject::connect(native, &rhi::SceneItem::frameSubmitted, &window, [&](QJsonObject sample) {
            sample[u"openingTime"_s] = opening.time();
            samples.append(sample);
        });
        surface.show();
    }
    ~Driver()
    {
        root.reset();
        surface.hide();
        window.releaseResources();
    }
    void frames(int count, const std::function<void(int)> &prepare = { })
    {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        int rendered = 0;
        surface.connectFrames(&loop, [&] {
            if (rendered >= count)
                return;
            ++rendered;
            if (rendered == count)
                loop.quit();
            else {
                if (prepare)
                    prepare(rendered);
                surface.requestFrame();
            }
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        if (prepare)
            prepare(0);
        deadline.start(yalahoo::review::DefaultDeadlineMilliseconds);
        surface.requestFrame();
        loop.exec();
        require(rendered == count, "Opening frame deadline");
        require(native->errorText().isEmpty(), "Production opening render failed");
        require(warnings.isEmpty(), "Production opening QML warnings");
    }
    void ready()
    {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&opening, &OpeningController::changed, &loop, [&] {
            if (!opening.preparing())
                loop.quit();
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        opening.replay();
        deadline.start(yalahoo::review::DefaultDeadlineMilliseconds);
        surface.requestFrame();
        loop.exec();
        require(opening.film(), "Production renderer/cover readiness did not start the film");
        require(opening.time() == 0, "Preparation consumed film time");
    }
    void seek(double time)
    {
        while (opening.time() + 1e-9 < time && opening.film())
            motion.clock()->advance(std::min(.05, time - opening.time()));
    }
    void resize(QSize size, double dpr)
    {
        window.resize(size);
        root->setSize(size);
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        frames(2);
    }
    QQuickRenderControl control;
    QQuickWindow window;
    OffscreenSurface surface;
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OpeningController opening{ &motion, &scene, &selection };
    LibrarySession library{ nullptr };
    QQmlEngine engine;
    std::unique_ptr<QQuickItem> root;
    rhi::SceneItem *native = nullptr;
    QStringList warnings;
    QJsonArray samples;
};

auto observeOpeningFrame(Driver &d, yalahoo::OpeningInk *&ink, QJsonArray &costs,
                         const QString &output, bool quick)
{
    QQmlComponent component(&d.engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/OpeningComparison.qml"_s));
    std::unique_ptr<QQuickItem> shape;
    if (quick) {
        shape.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
                { { u"source"_s, QVariant::fromValue(ink) } })));
        require(bool(shape), "Cannot create Quick Shape comparison");
        shape->setParentItem(ink->parentItem());
        shape->setPosition(ink->position());
        ink->setVisible(false);
    }
    QElapsedTimer timer;
    const auto before = openingInkStats().paintNs.load();
    const auto paints = openingInkStats().paints.load();
    timer.start();
    d.frames(32, [&](int i) {
        ink->setTime(.7 + i * .02);
        if (shape)
            shape->setProperty("paths", ink->comparisonPaths());
    });
    costs.append(QJsonObject{
            { u"api"_s, quick ? u"QtQuick.Shape.CurveRenderer"_s : u"C++ Canvas Painter"_s },
            { u"frames"_s, 32 },
            { u"guiAndQuickWallMs"_s, timer.nsecsElapsed() / NanosecondsPerMillisecond },
            { u"canvasPaintMs"_s,
              (openingInkStats().paintNs.load() - before) / NanosecondsPerMillisecond },
            { u"canvasPaints"_s, qint64(openingInkStats().paints.load() - paints) },
            { u"gpuCompleteMs"_s, QJsonValue::Null },
            { u"scope"_s, u"Local serial diagnostic; no FPS or cross-API image score"_s } });
    const auto image = d.surface.grab();
    require(image.save(output + (quick ? "/quick-shape.png"_L1 : "/canvas-painter.png"_L1)),
            "Cannot save drawing comparison");
    shape.reset();
    ink->setVisible(true);
}
}
int reviewOpening(QGuiApplication &, const QString &output, bool representative)
{
    QDir().mkpath(output);
    QJsonArray checks, captures, costs;
    const bool compareDrawing = qEnvironmentVariableIntValue("YALAHOO_VERIFY_COMPARE_DRAWING") == 1;
    const bool lifecycle = qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1;
    bool passed = true;
    const auto check = [&](const QString &name, bool value) {
        passed &= value;
        checks.append(QJsonObject{ { u"name"_s, name }, { u"passed"_s, value } });
    };
    try {
        Driver d;
        QSignalSpy handed(&d.opening, &OpeningController::handedOff),
                completed(&d.opening, &OpeningController::completed);
        d.ready();
        d.frames(2);
        const auto warmDraws = rhi::sceneSubmissionStats().rendered.load();
        const auto warmSubmission = d.native->submission();
        d.frames(126, [&](int) {
            d.motion.clock()->advance(.05);
        });
        check(u"opaque-paper-zero-3d-submission"_s,
              d.opening.time() < 6.32 && rhi::sceneSubmissionStats().rendered.load() == warmDraws
                      && d.native->submission() == warmSubmission);
        d.frames(108, [&](int) {
            d.motion.clock()->advance(.05);
        });
        check(u"one-film-handoff"_s, handed.size() == 1 && !d.opening.film());
        check(u"locked-during-chrome-reveal"_s, d.opening.active());
        d.frames(15, [&](int) {
            d.motion.clock()->advance(.05);
        });
        check(u"one-completion-after-720ms"_s, !d.opening.active() && completed.size() == 1);
        check(u"resting-camera"_s, d.scene.settled());
        check(u"focus-restored"_s,
              d.window.activeFocusItem() != nullptr
                      && d.window.activeFocusItem()
                              != yalahoo::review::findVisual(d.root.get(), u"openingSkip"_s));
        check(u"scene-resumed"_s, rhi::sceneSubmissionStats().rendered.load() > warmDraws);
        d.ready();
        for (double time : { 6.4, 7.2, 9.0, 11.6 }) {
            d.seek(time);
            d.frames(2);
            const auto image = d.surface.grab();
            require(!image.isNull() && image.save(output + u"/scene-t%1.png"_s.arg(time)),
                    "Cannot save live opening scene output");
        }
        for (int i = 0; i < 15; ++i)
            d.motion.clock()->advance(.05);
        d.frames(2);
        const auto paintAtEnd = openingInkStats().paints.load();
        d.frames(3);
        check(u"exited-canvas-zero-repaint"_s, openingInkStats().paints.load() == paintAtEnd);
        const auto playback = d.library.playbackTrackId();
        const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                  { 960, 900 },   { 930, 900 },   { 390, 844 },
                                  { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
        for (bool night : { false, true }) {
            d.motion.setReducedMotion(true);
            d.scene.lighting()->setNight(night);
            d.motion.setReducedMotion(false);
            for (double dpr : { 1.0, 1.5, 2.0 }) {
                if (!OffscreenSurface::Enabled && dpr != d.window.devicePixelRatio())
                    continue;
                for (const auto &size : sizes) {
                    if (representative
                        && ((dpr != 1 || night) && size != QSize(1280, 720)
                            && size != QSize(390, 844)))
                        continue;
                    d.resize(size, dpr);
                    d.ready();
                    for (double time : { .95, 1.64, 2.2, 4.3 }) {
                        d.seek(time);
                        d.frames(2);
                        const QString name =
                                u"%1-%2x%3-dpr%4-t%5"_s.arg(night ? "night"_L1 : "day"_L1)
                                        .arg(size.width())
                                        .arg(size.height())
                                        .arg(dpr)
                                        .arg(time);
                        const auto image = d.surface.grab();
                        require(!image.isNull() && image.save(output + '/'_L1 + name + ".png"_L1),
                                "Cannot save actual opening output");
                        if (time == 4.3 && size == QSize(1280, 720) && dpr == 1) {
                            const auto paints = openingInkStats().paints.load();
                            d.frames(3, [&](int) {
                                d.motion.clock()->advance(.05);
                            });
                            check(u"static-slogan-zero-canvas-work"_s,
                                  openingInkStats().paints.load() == paints);
                        }
                        const auto *skip =
                                yalahoo::review::findVisual(d.root.get(), u"openingSkip"_s);
                        check("bounded-skip-backdrop-"_L1 + name,
                              skip && skip->width() + 60 <= 220 && skip->height() + 60 <= 120);
                        const auto *headerNav =
                                yalahoo::review::findVisual(d.root.get(), u"headerNavigation"_s);
                        check("chrome-hidden-during-film-"_L1 + name,
                              headerNav && !headerNav->isVisible());
                        const auto *mark =
                                yalahoo::review::findVisual(d.root.get(), u"openingMark"_s);
                        check("brand-container-"_L1 + name,
                              mark
                                      && std::abs(mark->width()
                                                  - std::clamp(size.width() * .17, 155.0, 235.0))
                                              < 1e-6);
                        captures.append(QJsonObject{
                                { u"name"_s, name },
                                { u"time"_s, time },
                                { u"size"_s, QJsonArray{ size.width(), size.height() } },
                                { u"dpr"_s, d.window.effectiveDevicePixelRatio() } });
                    }
                    d.opening.skip();
                    for (int i = 0; i < 15; ++i)
                        d.motion.clock()->advance(.05);
                    d.frames(2);
                }
            }
        }
        // Compare the actual bounded brand component with Qt Quick Shape using identical trimmed paths.
        d.resize({ 1280, 720 }, 1);
        d.ready();
        d.seek(.95);
        d.frames(2);
        auto *ink = qobject_cast<OpeningInk *>(
                yalahoo::review::findVisual(d.root.get(), u"openingBrandInk"_s));
        require(ink, "Missing production brand painter");
        const auto compare = [&](bool quick) {
            return observeOpeningFrame(d, ink, costs, output, quick);
        };
        if (compareDrawing) {
            compare(false);
            compare(true);
        }
        QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QCoreApplication::sendEvent(&d.window, &tab);
        check(u"tab-focuses-skip"_s,
              d.window.activeFocusItem()
                      == yalahoo::review::findVisual(d.root.get(), u"openingSkip"_s));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(&d.window, &enter);
        check(u"enter-skips-film"_s, !d.opening.film());
        d.motion.setReducedMotion(true);
        check(u"reduced-motion-completes-reveal"_s, !d.opening.active());
        for (int cycle = 0; cycle < (lifecycle ? 10 : 0); ++cycle) {
            d.motion.setReducedMotion(false);
            d.ready();
            d.seek(cycle % 2 ? 2.2 : .95);
            d.frames(2);
            const auto paused = d.opening.time();
            d.motion.clock()->setActive(false);
            d.motion.clock()->advance(10);
            d.surface.hide();
            d.window.releaseResources();
            d.surface.show();
            d.frames(2);
            check(u"background-retains-time"_s, d.opening.time() == paused);
            d.motion.clock()->setActive(true);
            d.motion.clock()->advance(10);
            check(u"resume-discards-first-delta"_s, d.opening.time() == paused);
            const auto sceneBefore = rhi::sceneSubmissionStats().rendered.load();
            d.motion.setReducedMotion(true);
            d.frames(2);
            check(u"first-3d-frame-after-opaque-rebuild"_s,
                  rhi::sceneSubmissionStats().rendered.load() > sceneBefore);
            check(u"renderer-release-after-replay"_s, openingInkStats().renderers.load() == 0);
        }
        check(u"playback-isolated"_s, d.library.playbackTrackId() == playback);
        check(u"window-policy"_s, verification::windowPolicySatisfied(d.window));
        check(u"no-qml-warnings"_s, d.warnings.isEmpty());
        QJsonObject report{ { u"passed"_s, passed },
                            { u"representative"_s, representative },
                            { u"drawingComparisonRequested"_s, compareDrawing },
                            { u"lifecycleRequested"_s, lifecycle },
                            { u"checks"_s, checks },
                            { u"captures"_s, captures },
                            { u"costs"_s, costs },
                            { u"windowPolicy"_s, verification::windowPolicy(d.window) },
                            { u"offscreen"_s, d.surface.diagnostics() },
                            { u"samples"_s, d.samples },
                            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                            { u"backend"_s, int(d.window.rendererInterface()->graphicsApi()) },
                            { u"normalVsync"_s, QJsonValue::Null },
                            { u"physicalDisplay"_s, QJsonValue::Null } };
        require(yalahoo::writeJsonFile(output + "/report.json"_L1, QJsonDocument(report)),
                "Cannot commit opening report");
    } catch (const std::exception &error) {
        qCritical("Opening audit: %s", error.what());
        return 3;
    }
    return passed ? 0 : 1;
}
